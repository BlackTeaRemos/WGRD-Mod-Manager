#include "manager/update/ExecutableSwapper.h"

#include <windows.h>

#include <array>
#include <string>
#include <system_error>

namespace wgrd::manager {
std::filesystem::path ExecutableSwapper::CurrentExecutable() {
	std::array<wchar_t, MAX_PATH> buffer{};

	const DWORD written = GetModuleFileNameW(
		nullptr,
		buffer.data(),
		static_cast<DWORD>(buffer.size())
	);
	if (written == 0 || written >= buffer.size()) {
		return {};
	}

	return std::filesystem::path(std::wstring(buffer.data(), written));
}

std::filesystem::path ExecutableSwapper::StagedPath() {
	const std::filesystem::path current = CurrentExecutable();
	if (current.empty()) {
		return {};
	}

	return std::filesystem::path(current.string() + std::string(STAGED_SUFFIX));
}

std::filesystem::path ExecutableSwapper::RetiredPath() {
	const std::filesystem::path current = CurrentExecutable();
	if (current.empty()) {
		return {};
	}

	return std::filesystem::path(current.string() + std::string(RETIRED_SUFFIX));
}

bool ExecutableSwapper::DiscardRetired() {
	const std::filesystem::path retired = RetiredPath();
	if (retired.empty()) {
		return false;
	}

	std::error_code failure;
	return std::filesystem::remove(retired, failure);
}

bool ExecutableSwapper::Relaunch_(const std::filesystem::path& executable) {
	STARTUPINFOW startup{};
	startup.cb = sizeof(startup);

	PROCESS_INFORMATION process{};

	std::wstring commandLine = L"\"" + executable.wstring() + L"\"";

	const BOOL created = CreateProcessW(
		executable.c_str(),
		commandLine.data(),
		nullptr,
		nullptr,
		FALSE,
		0,
		nullptr,
		executable.parent_path().c_str(),
		&startup,
		&process
	);

	if (created == FALSE) {
		return false;
	}

	CloseHandle(process.hProcess);
	CloseHandle(process.hThread);

	return true;
}

std::expected<void, SwapError> ExecutableSwapper::Apply() {
	const std::filesystem::path current = CurrentExecutable();
	if (current.empty()) {
		return std::unexpected(SwapError::ExecutableUnknown);
	}

	const std::filesystem::path staged = StagedPath();
	const std::filesystem::path retired = RetiredPath();

	std::error_code failure;

	if (!std::filesystem::is_regular_file(staged, failure)) {
		return std::unexpected(SwapError::StagedMissing);
	}

	if (std::filesystem::file_size(staged, failure) == 0 || failure) {
		return std::unexpected(SwapError::StagedEmpty);
	}

	std::filesystem::remove(retired, failure);

	std::filesystem::rename(current, retired, failure);
	if (failure) {
		return std::unexpected(SwapError::RetireFailed);
	}

	std::filesystem::rename(staged, current, failure);
	if (failure) {
		std::error_code restore;
		std::filesystem::rename(retired, current, restore);
		return std::unexpected(SwapError::InstallFailed);
	}

	if (!Relaunch_(current)) {
		return std::unexpected(SwapError::RelaunchFailed);
	}

	return {};
}
}
