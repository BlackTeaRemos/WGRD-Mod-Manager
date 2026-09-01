#include "gui/Application.h"

#include "gui/design/Primitives.h"
#include "gui/design/Tokens.h"
#include "gui/render/RenderBackendProbe.h"
#include "gui/text/ShellText.h"

#include <imgui.h>
#include <imgui_impl_win32.h>

namespace wgrd::gui {
namespace {}

Application::~Application() {
	if (_backend != nullptr) {
		_backend->Shutdown();
		_backend.reset();
	}

	if (_started) {
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

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();

	ImGuiIO& io = ImGui::GetIO();
	io.IniFilename = nullptr;
	io.LogFilename = nullptr;

	ImGui::StyleColorsDark();

	if (!ImGui_ImplWin32_Init(_window.Handle())) {
		ImGui::DestroyContext();
		return false;
	}

	_started = true;

	_backend = RenderBackendProbe::Select(_window.Handle());
	if (_backend == nullptr) {
		return false;
	}

	_window.SetResizeHandler([this](const std::uint32_t resizedWidth, const std::uint32_t resizedHeight) {
			_backend->Resize(resizedWidth, resizedHeight);
		}
	);

	return true;
}

HWND Application::WindowHandle() const {
	return _window.Handle();
}

int Application::Run() {
	while (_window.PumpMessages() && !_state.ExitRequested()) {
		if (_services.swarm != nullptr) {
			_services.swarm->Poll();
		}

		if (_services.install != nullptr) {
			_services.install->Poll();
			AdoptFinishedInstalls_();
		}

		if (_services.mirror != nullptr) {
			_services.mirror->Poll();
		}

		_backend->BeginFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();

		DrawFrame_();

		ImGui::Render();
		_backend->Present(ImGui::GetDrawData());
	}

	SettleTransfers_();

	return 0;
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

		_modals.Reserve(origin, size.x, _state);

		_rail.Draw(ImVec2(origin.x, bodyTop), bodyBottom - bodyTop, _state, _services);

		const ScreenArea area{
			ImVec2(origin.x + tokens::RAIL_WIDTH + 1.0f, bodyTop), size.x - tokens::RAIL_WIDTH - 1.0f, bodyBottom - bodyTop
		};
		_screens.Draw(area, _state, _services);

		_statusBar.Draw(ImVec2(origin.x, bodyBottom), size.x, _services);
		_titleBar.Draw(origin, size.x, _state, _window, _services);

		_modals.Draw(origin, size.x, _state, _services);
	}
	ImGui::End();

	ImGui::PopStyleVar(3);
}
}
