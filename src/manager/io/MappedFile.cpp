#include "manager/io/MappedFile.h"

#include <windows.h>

#include <system_error>

namespace wgrd::manager {
MappedFile::MappedFile() noexcept
	: _fileHandle(INVALID_HANDLE_VALUE)
	, _mappingHandle(nullptr)
	, _view(nullptr)
	, _size(0) {}

MappedFile::MappedFile(MappedFile&& other) noexcept
	: _fileHandle(other._fileHandle)
	, _mappingHandle(other._mappingHandle)
	, _view(other._view)
	, _size(other._size) {
	other._fileHandle = INVALID_HANDLE_VALUE;
	other._mappingHandle = nullptr;
	other._view = nullptr;
	other._size = 0;
}

MappedFile& MappedFile::operator=(MappedFile&& other) noexcept {
	if (this != &other) {
		Release_();

		_fileHandle = other._fileHandle;
		_mappingHandle = other._mappingHandle;
		_view = other._view;
		_size = other._size;

		other._fileHandle = INVALID_HANDLE_VALUE;
		other._mappingHandle = nullptr;
		other._view = nullptr;
		other._size = 0;
	}

	return *this;
}

MappedFile::~MappedFile() {
	Release_();
}

void MappedFile::Release_() noexcept {
	if (_view != nullptr) {
		UnmapViewOfFile(_view);
		_view = nullptr;
	}

	if (_mappingHandle != nullptr) {
		CloseHandle(_mappingHandle);
		_mappingHandle = nullptr;
	}

	if (_fileHandle != INVALID_HANDLE_VALUE) {
		CloseHandle(_fileHandle);
		_fileHandle = INVALID_HANDLE_VALUE;
	}

	_size = 0;
}

std::expected<MappedFile, MappedFileError> MappedFile::Open(const std::filesystem::path& path) {
	std::error_code failure;
	if (!std::filesystem::is_regular_file(path, failure)) {
		return std::unexpected(MappedFileError::NotFound);
	}

	const std::uintmax_t fileSize = std::filesystem::file_size(path, failure);
	if (failure) {
		return std::unexpected(MappedFileError::Unopenable);
	}

	MappedFile mapped;

	mapped._fileHandle = CreateFileW(
		path.c_str(),
		GENERIC_READ,
		FILE_SHARE_READ,
		nullptr,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL,
		nullptr
	);

	if (mapped._fileHandle == INVALID_HANDLE_VALUE) {
		return std::unexpected(MappedFileError::Unopenable);
	}

	if (fileSize == 0) {
		return mapped;
	}

	mapped._mappingHandle = CreateFileMappingW(
		mapped._fileHandle,
		nullptr,
		PAGE_READONLY,
		0,
		0,
		nullptr
	);

	if (mapped._mappingHandle == nullptr) {
		return std::unexpected(MappedFileError::Unmappable);
	}

	const void* view = MapViewOfFile(mapped._mappingHandle, FILE_MAP_READ, 0, 0, 0);
	if (view == nullptr) {
		return std::unexpected(MappedFileError::Unmappable);
	}

	mapped._view = static_cast<const std::byte*>(view);
	mapped._size = static_cast<std::size_t>(fileSize);

	return mapped;
}

std::span<const std::byte> MappedFile::Data() const noexcept {
	if (_view == nullptr) {
		return {};
	}

	return std::span<const std::byte>(_view, _size);
}

std::size_t MappedFile::Size() const noexcept {
	return _size;
}
}
