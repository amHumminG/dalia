#include "Sandbox.h"

Sandbox::Sandbox()
	: m_listeners{
		Listener(0, true),
		Listener(1, false),
		Listener(2, false),
		Listener(3, false)
	}
{
	int screenWidth = 1920;
	int screenHeight = 1080;
	SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT);
	InitWindow(screenWidth, screenHeight, "Dalia Engine Sandbox");
	SetTargetFPS(60);

	rlImGuiSetup(true);
	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	io.ConfigDockingTransparentPayload = true;

	// Camera Setup
	m_spectatorCamera.position = { 0.0f, 10.0f, 10.0f };
	m_spectatorCamera.target = { 0.0f, 0.0f, 0.0f };
	m_spectatorCamera.up = { 0.0f, 1.0f, 0.0f };
	m_spectatorCamera.fovy = 45.0f;
	m_spectatorCamera.projection = CAMERA_PERSPECTIVE;

	// Dalia Engine Initialization
	dalia::EngineConfig config;
	config.coordinateSystem = dalia::CoordinateSystem::RightHanded;
	config.listenerCapacity = 4;
	m_audioEngine.Init(config);

	// Initialize UI Panels
	// m_panels.push_back(std::make_unique<>());
}

Sandbox::~Sandbox() {
	rlImGuiShutdown();
	CloseWindow();
	m_audioEngine.Shutdown();
}

void Sandbox::Run() {
	while (!WindowShouldClose() && !m_isExiting) {
		Update();
		Draw();
	}
}

void Sandbox::Update() {
	UpdateCamera(&m_spectatorCamera, CAMERA_FREE);

	// for (auto& listener : m_listeners) listener.Update(m_audioEngine);
	// for (auto& playback: m_playbackInstances) playback->Update(m_audioEngine);
}

void Sandbox::Draw() {
	BeginDrawing();
	ClearBackground(DARKGRAY);

	// 3D Pass
	BeginMode3D(m_spectatorCamera);

	DrawGrid(20, 1.0f);

	// for (auto& listener : m_listeners) listener.Draw3D();
	for (auto& playback : m_playbackInstances) {
		bool isSelected = (m_currentSelectionType == SelectionType::Playback && m_selectedObject == playback.get());
		playback->Draw3D(isSelected);
	}

	EndMode3D();

	// UI Pass
	rlImGuiBegin();

	ImGuiID dockSpaceId = ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport());

	if (ImGui::BeginMainMenuBar()) {
		if (ImGui::BeginMenu("Options")) {
			if (ImGui::MenuItem("Exit")) m_isExiting = true;
			ImGui::EndMenu();
		}
		ImGui::EndMainMenuBar();
	}

	// for (auto& panel : m_panels) panel->Draw(m_audioEngine);

	rlImGuiEnd();
	EndDrawing();
}
