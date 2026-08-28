#pragma once

#include <cstddef>
#include <expected>
#include <filesystem>
#include <span>

namespace wgrd::manager {
enum class MappedFileError {
	NotFound, Unopenable, Unmappable
};

class MappedFile {
public:
	MappedFile() noexcept;

	MappedFile(const MappedFile&) = delete;

	MappedFile& operator=(const MappedFile&) = delete;

	MappedFile(MappedFile&& other) noexcept;

	MappedFile& operator=(MappedFile&& other) noexcept;

	~MappedFile();

	[[nodiscard]] static std::expected<MappedFile, MappedFileError> Open(
		const std::filesystem::path& path
	);

	[[nodiscard]] std::span<const std::byte> Data() const noexcept;

	[[nodiscard]] std::size_t Size() const noexcept;

private:
	void Release_() noexcept;

	void* _fileHandle;
	void* _mappingHandle;
	const std::byte* _view;
	std::size_t _size;
};
}
