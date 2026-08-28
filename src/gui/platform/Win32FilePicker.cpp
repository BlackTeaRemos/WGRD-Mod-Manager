#include "gui/platform/Win32FilePicker.h"

#include <shobjidl.h>

#include <string>

namespace wgrd::gui {
namespace {
	std::wstring Widen(const std::string_view value) {
		if (value.empty()) {
			return std::wstring();
		}

		const int needed = MultiByteToWideChar(
			CP_UTF8,
			0,
			value.data(),
			static_cast<int>(value.size()),
			nullptr,
			0
		);

		if (needed <= 0) {
			return std::wstring();
		}

		std::wstring wide(static_cast<std::size_t>(needed), L'\0');
		MultiByteToWideChar(
			CP_UTF8,
			0,
			value.data(),
			static_cast<int>(value.size()),
			wide.data(),
			needed
		);

		return wide;
	}

	class ApartmentScope {
	public:
		ApartmentScope()
			: _owned(false) {
			const HRESULT entered = CoInitializeEx(
				nullptr,
				COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE
			);

			_owned = entered == S_OK || entered == S_FALSE;
		}

		~ApartmentScope() {
			if (_owned) {
				CoUninitialize();
			}
		}

		ApartmentScope(const ApartmentScope&) = delete;
		ApartmentScope& operator=(const ApartmentScope&) = delete;

	private:
		bool _owned;
	};

	std::optional<std::filesystem::path> ReadChosenPath(IFileDialog* dialog) {
		IShellItem* item = nullptr;
		if (FAILED(dialog->GetResult(&item)) || item == nullptr) {
			return std::nullopt;
		}

		PWSTR chosen = nullptr;
		const HRESULT named = item->GetDisplayName(SIGDN_FILESYSPATH, &chosen);
		item->Release();

		if (FAILED(named) || chosen == nullptr) {
			return std::nullopt;
		}

		std::filesystem::path result(chosen);
		CoTaskMemFree(chosen);

		return result;
	}

	std::optional<std::filesystem::path> RunDialog(
		const HWND owner,
		const CLSID& dialogClass,
		const std::string_view title,
		const std::string_view suggestedName,
		const std::string_view filterLabel,
		const std::string_view filterPattern,
		const bool foldersOnly
	) {
		const ApartmentScope apartment;

		IFileDialog* dialog = nullptr;
		const HRESULT created = CoCreateInstance(
			dialogClass,
			nullptr,
			CLSCTX_INPROC_SERVER,
			IID_PPV_ARGS(&dialog)
		);

		if (FAILED(created) || dialog == nullptr) {
			return std::nullopt;
		}

		const std::wstring wideTitle = Widen(title);
		if (!wideTitle.empty()) {
			dialog->SetTitle(wideTitle.c_str());
		}

		DWORD options = 0;
		if (SUCCEEDED(dialog->GetOptions(&options))) {
			options |= FOS_FORCEFILESYSTEM | FOS_NOCHANGEDIR;
			if (foldersOnly) {
				options |= FOS_PICKFOLDERS;
			}
			dialog->SetOptions(options);
		}

		const std::wstring wideLabel = Widen(filterLabel);
		const std::wstring widePattern = Widen(filterPattern);

		if (!foldersOnly && !widePattern.empty()) {
			const COMDLG_FILTERSPEC filters[] = {{wideLabel.c_str(), widePattern.c_str()}};
			dialog->SetFileTypes(1, filters);
			dialog->SetFileTypeIndex(1);

			const std::size_t dot = widePattern.find(L'.');
			if (dot != std::wstring::npos) {
				dialog->SetDefaultExtension(widePattern.substr(dot + 1).c_str());
			}
		}

		const std::wstring wideSuggested = Widen(suggestedName);
		if (!wideSuggested.empty()) {
			dialog->SetFileName(wideSuggested.c_str());
		}

		const HRESULT shown = dialog->Show(owner);
		if (FAILED(shown)) {
			dialog->Release();
			return std::nullopt;
		}

		std::optional<std::filesystem::path> chosen = ReadChosenPath(dialog);
		dialog->Release();

		return chosen;
	}
}

Win32FilePicker::Win32FilePicker(const HWND owner)
	: _owner(owner) {}

Win32FilePicker::~Win32FilePicker() = default;

void Win32FilePicker::SetOwner(const HWND owner) {
	_owner = owner;
}

std::optional<std::filesystem::path> Win32FilePicker::SaveFile(
	const std::string_view title,
	const std::string_view suggestedName,
	const std::string_view filterLabel,
	const std::string_view filterPattern
) const {
	return RunDialog(
		_owner,
		CLSID_FileSaveDialog,
		title,
		suggestedName,
		filterLabel,
		filterPattern,
		false
	);
}

std::optional<std::filesystem::path> Win32FilePicker::OpenFile(
	const std::string_view title,
	const std::string_view filterLabel,
	const std::string_view filterPattern
) const {
	return RunDialog(
		_owner,
		CLSID_FileOpenDialog,
		title,
		{},
		filterLabel,
		filterPattern,
		false
	);
}

std::optional<std::filesystem::path> Win32FilePicker::PickFolder(const std::string_view title) const {
	return RunDialog(
		_owner,
		CLSID_FileOpenDialog,
		title,
		{},
		{},
		{},
		true
	);
}
}
