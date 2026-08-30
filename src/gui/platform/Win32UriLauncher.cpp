#include "gui/platform/Win32UriLauncher.h"

#include <Windows.h>
#include <shellapi.h>

#include <algorithm>
#include <string>

namespace wgrd::gui {
namespace {
	bool Printable(const std::string_view text) {
		return std::ranges::all_of(text, [](const char character) {
				return static_cast<unsigned char>(character) > 0x20
				       && static_cast<unsigned char>(character) < 0x7F;
			}
		);
	}

	std::wstring Widen(const std::string_view text) {
		const int needed = MultiByteToWideChar(
			CP_UTF8,
			0,
			text.data(),
			static_cast<int>(text.size()),
			nullptr,
			0
		);

		if (needed <= 0) {
			return {};
		}

		std::wstring wide(static_cast<std::size_t>(needed), L'\0');
		MultiByteToWideChar(
			CP_UTF8,
			0,
			text.data(),
			static_cast<int>(text.size()),
			wide.data(),
			needed
		);

		return wide;
	}
}

Win32UriLauncher::Win32UriLauncher() = default;

Win32UriLauncher::~Win32UriLauncher() = default;

bool Win32UriLauncher::Acceptable_(const std::string_view uri) {
	if (uri.size() < SCHEME.size() || uri.size() > URI_LIMIT) {
		return false;
	}

	if (!uri.starts_with(SCHEME)) {
		return false;
	}

	return Printable(uri);
}

bool Win32UriLauncher::Open(const std::string_view uri) const {
	if (!Acceptable_(uri)) {
		return false;
	}

	const std::wstring wide = Widen(uri);
	if (wide.empty()) {
		return false;
	}

	const HINSTANCE outcome = ShellExecuteW(
		nullptr,
		L"open",
		wide.c_str(),
		nullptr,
		nullptr,
		SW_SHOWNORMAL
	);

	return reinterpret_cast<INT_PTR>(outcome) > 32;
}
}
