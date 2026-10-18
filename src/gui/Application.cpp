#include "gui/Application.h"

#include "gui/design/FontLibrary.h"
#include "gui/images/PreviewImages.h"
#include "gui/design/Primitives.h"
#include "gui/design/Tokens.h"
#include "gui/design/UiStyle.h"
#include "gui/render/RenderBackendProbe.h"
#include "gui/text/ShellText.h"
#include "gui/state/KeyboardNavigation.h"

#include <imgui.h>
#include <imgui_impl_win32.h>

namespace wgrd::gui {
namespace {}

Application::~Application() {
	_window.SetResizeHandler({});
	if (_backend != nullptr) {
		_backend->Shutdown();
		_backend.reset();
	}

	if (_started) {
		PreviewImages::Clear();
		_dpi.Shutdown();
		ImGui_ImplWin32_Shutdown();
		ImGui::DestroyContext();
		_started = false;
	}

	_window.Destroy();
}

bool Application::Start(const ApplicationServices& services) {
	_services = services;

	constexpr auto width = static_cast<std::uint32_t>(tokens::FRAME_WIDTH);
	constexpr auto height = static_cast<std::uint32_t>(tokens::FRAME_MIN_HEIGHT);

	if (!_window.Create(std::wstring(text::shell::WINDOW_TITLE), width, height)) {
		return false;
	}

	if (_services.windowPreferences != nullptr) {
		const domain::WindowPreferences preferences = _services.windowPreferences->Load();
		_state.SetBackgroundOnClose(preferences.backgroundOnClose);
		_window.RestorePlacement(preferences);
	}

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();

	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	io.IniFilename = nullptr;
	io.LogFilename = nullptr;

	if (!design::FontLibrary::Build()) {
		ImGui::DestroyContext();
		return false;
	}

	design::UiStyle::Apply();

	if (!ImGui_ImplWin32_Init(_window.Handle())) {
		ImGui::DestroyContext();
		return false;
	}

	_dpi.Initialize();
	_started = true;
	PreviewImages::Initialize();

	_backend = RenderBackendProbe::Select(_window.Handle());
	if (_backend == nullptr) {
		return false;
	}

	_window.SetResizeHandler([this](const std::uint32_t resizedWidth, const std::uint32_t resizedHeight) {
			_backend->Resize(resizedWidth, resizedHeight);
			if (!_drawing && _window.Visible()) {
				RenderFrame_();
			}
		}
	);

	return true;
}

HWND Application::WindowHandle() const {
	return _window.Handle();
}

int Application::Run() {
	while (_window.PumpMessages() && !_exitConfirmed) {
		if (_services.swarm != nullptr) {
			_services.swarm->Poll();
		}

		if (_services.install != nullptr) {
			_services.install->Poll();
			AdoptFinishedInstalls_();
		}

		AskGossipOnce_();

		if (_services.registry != nullptr) {
			_services.registry->Tick();
		}

		HandleClose_();
		if (_exitConfirmed) {
			break;
		}
		if (_services.mirror != nullptr) {
			_services.mirror->Poll();
		}

		_window.UpdateProgress(_services.install != nullptr ? _services.install->Progress() : domain::InstallProgress{});
		if (!_window.Visible()) {
			MsgWaitForMultipleObjectsEx(0, nullptr, 50, QS_ALLINPUT, MWMO_INPUTAVAILABLE);
			continue;
		}

		RenderFrame_();
	}

	SaveWindow_();
	_window.HideForShutdown();
	SettleTransfers_();

	return 0;
}

void Application::RenderFrame_() {
	if (_drawing || !_started || _backend == nullptr) {
		return;
	}
	_drawing = true;
	_backend->BeginFrame();
	DpiViewport::BeginNativeInput();
	ImGui_ImplWin32_NewFrame();
	DpiViewport::EndNativeInput();
	_dpi.Prepare(_window.Scale());
	ImGui::NewFrame();
	KeyboardNavigation::Apply(_state);
	DrawFrame_();
	ImGui::Render();
	PreviewImages::RegisterTextures();
	_backend->Present(ImGui::GetDrawData());
	_drawing = false;
}

void Application::AskGossipOnce_() {
	if (_askedGossip || _services.gossip == nullptr) {
		return;
	}

	_askedGossip = true;

	_services.gossip->RequestGossipRefresh();
}

void Application::HandleClose_() {
	if (!_window.CloseRequested() && !_state.ExitRequested()) {
		return;
	}
	const bool explicitExit = _window.ExplicitExitRequested() || _state.ExitRequested();
	_window.AcknowledgeClose();
	_state.ClearExitRequest();

	if (_state.BackgroundOnClose() && !explicitExit) {
		SaveWindow_();
		_window.Background(true);
		return;
	}
	const bool installing = _services.install != nullptr && _services.install->Progress().Busy();
	const bool publishing = _services.publish != nullptr && _services.publish->Progress().Busy();
	const bool updating = _services.updates != nullptr && _services.updates->Status().Busy();
	if (installing || publishing || updating) {
		_state.CloseSettings();
		_state.CloseDetail();
		_closeDialog.Open();
		return;
	}
	_exitConfirmed = true;
}

void Application::DrawCloseDialog_() {
	const WindowCloseChoice choice = _closeDialog.Draw(_state);
	if ((choice == WindowCloseChoice::KeepOpen || choice == WindowCloseChoice::Background) && _services.gameLocation != nullptr) {
		_services.gameLocation->CancelSelection();
	}
	if (choice == WindowCloseChoice::Background) {
		SaveWindow_();
		_window.Background(_state.BackgroundOnClose());
	} else if (choice == WindowCloseChoice::Exit) {
		_exitConfirmed = true;
	}
}

void Application::SaveWindow_() const {
	if (_services.windowPreferences != nullptr) {
		const bool saved = _services.windowPreferences->Save(_window.CapturePlacement(_state.BackgroundOnClose()));
		(void)saved;
	}
}

void Application::SettleTransfers_() {
	if (_services.install == nullptr) {
		return;
	}

	if (!_services.install->Progress().Busy()) {
		return;
	}

	_services.install->Cancel();
}

void Application::AdoptFinishedInstalls_() {
	const std::uint64_t completed = _services.install->CompletedInstalls();
	const std::uint64_t settled = _services.install->SettledAttempts();

	const bool newlyInstalled = completed != _seenInstalls;
	const bool newlySettled = settled != _seenSettled;

	if (!newlyInstalled && !newlySettled) {
		return;
	}

	_seenInstalls = completed;
	_seenSettled = settled;

	if (newlyInstalled && _services.order != nullptr) {
		_services.order->Refresh();
	}

	if (_services.catalog != nullptr) {
		_services.catalog->Refresh();
	}

	if (newlyInstalled && _services.profiles != nullptr) {
		_services.profiles->Refresh();
	}
}

void Application::DrawFrame_() {
	const ImGuiViewport* const viewport = ImGui::GetMainViewport();

	ImGui::SetNextWindowPos(viewport->WorkPos);
	ImGui::SetNextWindowSize(viewport->WorkSize);

	constexpr ImGuiWindowFlags flags =
			ImGuiWindowFlags_NoTitleBar |
			ImGuiWindowFlags_NoResize |
			ImGuiWindowFlags_NoMove |
			ImGuiWindowFlags_NoScrollbar |
			ImGuiWindowFlags_NoScrollWithMouse |
			ImGuiWindowFlags_NoSavedSettings |
			ImGuiWindowFlags_NoBringToFrontOnFocus;

	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

	if (ImGui::Begin("frame", nullptr, flags)) {
		const ImVec2 origin = ImGui::GetWindowPos();
		const ImVec2 size = ImGui::GetWindowSize();

		design::FillRect(origin, ImVec2(origin.x + size.x, origin.y + size.y), tokens::PRIMARY);

		const float bodyTop = origin.y + tokens::TITLE_BAR_HEIGHT;
		const float bodyBottom = origin.y + size.y - tokens::STATUS_BAR_HEIGHT;

		_rail.Draw(ImVec2(origin.x, bodyTop), bodyBottom - bodyTop, _state, _services);

		const ScreenArea area{
			ImVec2(origin.x + tokens::RAIL_WIDTH + 1.0f, bodyTop), size.x - tokens::RAIL_WIDTH - 1.0f, bodyBottom - bodyTop
		};
		_screens.Draw(area, _state, _services);

		_statusBar.Draw(ImVec2(origin.x, bodyBottom), size.x, _state, _services);
		_titleBar.Draw(origin, size.x, _state, _window, _services);

		_modals.Draw(_state, _services);
		DrawCloseDialog_();
	}
	ImGui::End();

	ImGui::PopStyleVar(3);
}
}
