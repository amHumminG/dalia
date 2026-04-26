#include "Sandbox.h"

#include <filesystem>

Sandbox::Sandbox()
	: m_listeners{
		Listener(&m_engine, 0, true),
		Listener(&m_engine, 1, false),
		Listener(&m_engine, 2, false),
		Listener(&m_engine, 3, false)
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
	config.logLevel = dalia::LogLevel::Debug;
	config.listenerCapacity = 4;
	m_engine.Init(config);

	// Initialize UI Panels
	// m_panels.push_back(std::make_unique<>());
}

Sandbox::~Sandbox() {
	rlImGuiShutdown();
	CloseWindow();
	m_engine.Shutdown();
}

void Sandbox::Run() {
	while (!WindowShouldClose() && !m_isExiting) {
		Update();
		Draw();
	}
}

void Sandbox::Update() {
	UpdateCamera(&m_spectatorCamera, CAMERA_FREE);

	Listener* currentlyPiloted = nullptr;
	for (auto& listener : m_listeners) {
		if (listener.isPiloted) {
			if (currentlyPiloted == nullptr) {
				currentlyPiloted = &listener;
				listener.SyncWithCamera(m_spectatorCamera);
			}
			else {
				listener.isPiloted = false;
			}
		}
	}

	for (auto& listener : m_listeners) listener.Update();
	for (auto& playback: m_playbackInstances) playback->Update();

	m_engine.Update();
}

void Sandbox::Draw() {
	BeginDrawing();
	ClearBackground(DARKGRAY);

	// 3D Pass
	BeginMode3D(m_spectatorCamera);

	DrawGrid(20, 1.0f);

	// for (auto& listener : m_listeners) listener.Draw3D();
	for (auto& playback : m_playbackInstances) {
		bool isSelected = (m_selectionType == SelectionType::Playback && m_selectedObject == playback.get());
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

	DrawSceneOutliner();
	DrawInspector();
	DrawAssetBrowser();

	// for (auto& panel : m_panels) panel->Draw();

	rlImGuiEnd();
	EndDrawing();
}

void Sandbox::DrawSceneOutliner() {
	ImGui::Begin("Scene Outliner");

	// Listeners
	if (ImGui::CollapsingHeader("Listeners", ImGuiTreeNodeFlags_DefaultOpen)) {
		for (auto& listener : m_listeners) {
			bool isSelected = (m_selectionType == SelectionType::Listener && m_selectedObject == &listener);

			char label[64];
			snprintf(label, sizeof(label), "[Listener] %u", listener.GetIndex());

			if (ImGui::Selectable(label, isSelected)) {
				m_selectionType = SelectionType::Listener;
				m_selectedObject = &listener;
			}
		}
	}

	// Playback instances
	if (ImGui::CollapsingHeader("Playback Instances", ImGuiTreeNodeFlags_DefaultOpen)) {
		for (auto& playback : m_playbackInstances) {
			bool isSelected = (m_selectionType == SelectionType::Playback && m_selectedObject == playback.get());

			char label[128];
			snprintf(label, sizeof(label), "[Playback] %s", playback->GetName().c_str());

			if (ImGui::Selectable(label, isSelected)) {
				m_selectionType = SelectionType::Playback;
				m_selectedObject = playback.get();
			}
		}
	}

	ImGui::End();
}

void Sandbox::DrawInspector() {
	ImGui::Begin("Inspector");

	bool requestDelete = false;

	if (m_selectionType == SelectionType::None || m_selectedObject == nullptr) {
		ImGui::TextDisabled("Select an object");
	}
	else if (m_selectionType == SelectionType::Listener) {
		auto* listener = static_cast<Listener*>(m_selectedObject);
		listener->DrawInspectorUI();
	}
	else if (m_selectionType == SelectionType::Playback) {
		auto* playback = static_cast<PlaybackInstance*>(m_selectedObject);
		playback->DrawInspectorUI();

		ImGui::Separator();
		ImGui::PushStyleColor(ImGuiCol_Button, {0.8f, 0.2f, 0.2f, 1.0f});
		if (ImGui::Button("Delete Playback Instance", ImVec2(-1, 30))) requestDelete = true;
		ImGui::PopStyleColor();
	}
	else if (m_selectionType == SelectionType::Sound) {
		auto* sound = static_cast<SoundAsset*>(m_selectedObject);

		auto spawnLambda = [this](dalia::SoundHandle handle, const std::string& filepath) {
			std::filesystem::path fullpath = filepath;
			std::string baseName = fullpath.stem().string();

			static uint32_t playbackSpawnCounter = 0;
			char nameBuffer[128];
			snprintf(nameBuffer, sizeof(nameBuffer), "%s (%u)", baseName.c_str(), playbackSpawnCounter++);

			auto playback = std::make_unique<PlaybackInstance>(&m_engine, handle, nameBuffer);
			if (playback->GetResult() == dalia::Result::Ok) m_playbackInstances.push_back(std::move(playback));
		};

		sound->DrawInspectorUI(spawnLambda);

		ImGui::Separator();
		ImGui::PushStyleColor(ImGuiCol_Button, {0.8f, 0.2f, 0.2f, 1.0f});
		if (ImGui::Button("Delete Asset", ImVec2(-1, 30))) requestDelete = true;
		ImGui::PopStyleColor();
	}

	// Object deletion
	if (requestDelete) {
		if (m_selectionType == SelectionType::Playback) {
			std::erase_if(m_playbackInstances, [this](const std::unique_ptr<PlaybackInstance>& p) {
				return p.get() == m_selectedObject;
			});
		}
		else if (m_selectionType == SelectionType::Sound) {
			std::erase_if(m_sounds, [this](const std::unique_ptr<SoundAsset>& s) {
				return s.get() == m_selectedObject;
			});
		}

		m_selectionType = SelectionType::None;
		m_selectedObject = nullptr;
	}

	ImGui::End();
}

void Sandbox::DrawAssetBrowser() {
	ImGui::Begin("Asset Browser");

	// Loading new assets
	ImGui::TextDisabled("Load Audio File");
	ImGui::InputText("Filepath", m_newSoundPathBuffer, sizeof(m_newSoundPathBuffer));

	ImGui::Combo("Type", &m_newSoundType, "Resident \0Stream\0");

	if (ImGui::Button("Load Asset", ImVec2(-1, 0))) {
		std::string path(m_newSoundPathBuffer);
		if (!path.empty()) {
			dalia::SoundType type = (m_newSoundType == 0) ? dalia::SoundType::Resident : dalia::SoundType::Stream;
			auto sound = std::make_unique<SoundAsset>(&m_engine, type, path);
			if (sound->GetResult() == dalia::Result::Ok) m_sounds.push_back(std::move(sound));
			m_newSoundPathBuffer[0] = '\0';
		}
	}

	ImGui::Separator();

	ImGui::TextDisabled("Loaded Assets");
	for (auto& sound : m_sounds) {
		bool isSelected = (m_selectionType == SelectionType::Sound && m_selectedObject == sound.get());

		std::filesystem::path path(sound->GetFilePath());
		std::string filename = path.filename().string();

		char label[256];
		snprintf(label, sizeof(label), "[Asset] %s", filename.c_str());

		if (ImGui::Selectable(label, isSelected)) {
			m_selectionType = SelectionType::Sound;
			m_selectedObject = sound.get();
		}
	}

	ImGui::End();
}

