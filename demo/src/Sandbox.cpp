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
	// SetExitKey(NULL);
	m_viewportTexture = LoadRenderTexture(screenWidth, screenHeight);

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
	config.logCallback = [this](dalia::LogLevel level, const char* context, const char* message) {
		m_logs.push_back({level, context, message});
	};
	config.listenerCapacity = 4;
	m_engine.Init(config);

	m_buses.push_back(std::make_unique<MixingBus>(&m_engine, "Master"));

	RefreshAvailableAssets();
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
	ImGuiIO& io = ImGui::GetIO();

	if (!io.WantTextInput) {
		if (IsKeyPressed(KEY_C)) {
			if (m_in3DMode) {
				m_in3DMode = false;
				EnableCursor();
			}
			else {
				m_in3DMode = true;
				DisableCursor();
			}
		}
	}

	if (m_in3DMode) UpdateCamera(&m_spectatorCamera, CAMERA_FREE);

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
	// 3D Pass (to viewport texture)
	BeginTextureMode(m_viewportTexture);
	ClearBackground(DARKGRAY);
	BeginMode3D(m_spectatorCamera);

	DrawGrid(20, 1.0f);

	for (auto& listener : m_listeners) {
		bool isSelected = (m_selectionType == SelectionType::Listener && m_selectedObject == &listener);
		listener.Draw3D(isSelected);
	}

	for (auto& playback : m_playbackInstances) {
		bool isSelected = (m_selectionType == SelectionType::Playback && m_selectedObject == playback.get());
		playback->Draw3D(isSelected);
	}

	EndMode3D();
	EndTextureMode();

	// UI Pass
	BeginDrawing();
	ClearBackground(BLACK);

	rlImGuiBegin();
	ImGuiID dockSpaceId = ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport());

	DrawMenuBar();
	DrawSceneOutliner();
	DrawInspector();
	DrawAssetBrowser();
	DrawViewportPanel();
	DrawConsolePanel();
	DrawBusHierarchyPanel();

	rlImGuiEnd();
	EndDrawing();
}

void Sandbox::DrawMenuBar() {
	if (ImGui::BeginMainMenuBar()) {
		if (ImGui::BeginMenu("Options")) {
			if (ImGui::MenuItem("Exit")) m_isExiting = true;
			ImGui::EndMenu();
		}
		ImGui::EndMainMenuBar();
	}
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
	else if (m_selectionType == SelectionType::Bus) {
		auto* bus = static_cast<MixingBus*>(m_selectedObject);
		bus->DrawInspectorUI();

		if (bus->GetIdentifier() != "Master") {
			ImGui::Separator();
			ImGui::PushStyleColor(ImGuiCol_Button, {0.8f, 0.2f, 0.2f, 1.0f});
			if (ImGui::Button("Destroy Bus", ImVec2(-1, 30))) requestDelete = true;
			ImGui::PopStyleColor();
		}
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
		else if (m_selectionType == SelectionType::Bus) {
			std::erase_if(m_buses, [this](const std::unique_ptr<MixingBus>& b) {
				return b.get() == m_selectedObject;
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

	ImGui::SameLine();
	if (ImGui::BeginCombo("##AssetDropdown", "Browse...", ImGuiComboFlags_NoPreview)) {
		for (const auto& assetPath : m_availableAssets) {
			if (ImGui::Selectable(assetPath.c_str(), false)) {
				std::strncpy(m_newSoundPathBuffer, assetPath.c_str(), sizeof(m_newSoundPathBuffer) - 1);
				m_newSoundPathBuffer[sizeof(m_newSoundPathBuffer) - 1] = '\0';
			}
		}
		ImGui::EndCombo();
	}

	ImGui::SameLine();
	if (ImGui::Button("Refresh")) RefreshAvailableAssets();

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

void Sandbox::DrawViewportPanel() {
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
	ImGui::Begin("3D Viewport");


	// Dynamic resizing
	ImVec2 size = ImGui::GetContentRegionAvail();
	if (size.x > 0 && size.y > 0) {
		// Resize render texture if panel is resized
		if (size.x != static_cast<float>(m_viewportTexture.texture.width) ||
			size.y != static_cast<float>(m_viewportTexture.texture.height)) {
			UnloadRenderTexture(m_viewportTexture);
			m_viewportTexture = LoadRenderTexture(static_cast<int>(size.x), static_cast<int>(size.y));
		}

		rlImGuiImageRenderTexture(&m_viewportTexture);
	}

	if (!m_in3DMode) {
		ImVec2 pos = ImGui::GetWindowPos();
		ImGui::GetWindowDrawList()->AddText(
			ImVec2(pos.x + 10, pos.y + 30),
			IM_COL32(255, 255, 255, 150),
			"Press C enter 3D mode"
		);
	}
	else {
		ImVec2 pos = ImGui::GetWindowPos();
		ImGui::GetWindowDrawList()->AddText(
			ImVec2(pos.x + 10, pos.y + 30),
			IM_COL32(255, 255, 255, 150),
			"Press C to exit 3D mode"
		);
	}

	ImGui::End();
	ImGui::PopStyleVar();
}

void Sandbox::DrawConsolePanel() {
	ImGui::Begin("Console");

	// Toolbar
	if (ImGui::Button("Clear")) m_logs.clear();
	ImGui::SameLine();
	ImGui::Checkbox("Auto-scroll", &m_consoleAutoScroll);

	ImGui::TextDisabled("| Filters:");
	ImGui::SameLine();
	ImGui::Checkbox("Debug", &m_showDebug); ImGui::SameLine();
	ImGui::Checkbox("Info", &m_showInfo); ImGui::SameLine();
	ImGui::Checkbox("Warnings", &m_showWarnings); ImGui::SameLine();
	ImGui::Checkbox("Error", &m_showErrors);

	ImGui::Separator();

	// Logs
	ImGui::BeginChild("ScrollingRegion", ImVec2(0, 0), false, ImGuiWindowFlags_None);

	static size_t lastSize = 0;
	if (m_logs.size() > lastSize) {
		if (m_consoleAutoScroll) m_consoleScrollToBottom = true;
		lastSize = m_logs.size();
	}
	else if (m_logs.size() < lastSize) {
		lastSize = m_logs.size();
	}

	if (ImGui::BeginTable("LogsTable", 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit)) {
		ImGui::TableSetupColumn("Level", ImGuiTableColumnFlags_WidthFixed);
		ImGui::TableSetupColumn("Category", ImGuiTableColumnFlags_WidthFixed);
		ImGui::TableSetupColumn("Message", ImGuiTableColumnFlags_WidthStretch);

		// Draw logs
		for (const auto& log : m_logs) {
			if (!m_showDebug && log.level == dalia::LogLevel::Debug) continue;
			if (!m_showInfo && log.level == dalia::LogLevel::Info) continue;
			if (!m_showWarnings && log.level == dalia::LogLevel::Warning) continue;
			if (!m_showErrors && (log.level == dalia::LogLevel::Error || log.level == dalia::LogLevel::Critical)) continue;

			const char* levelStr = "NONE";
			ImVec4 color = ImVec4(0.7f, 0.7f, 0.7f, 1.0f); // Gray

			switch (log.level) {
				case dalia::LogLevel::Debug:
					levelStr = "DEBUG";
					color = ImVec4(0.45f, 0.45f, 0.45f, 1.0f);
					break;
				case dalia::LogLevel::Info:
					levelStr = "INFO";
					color = ImVec4(0.4f, 0.8f, 0.5f, 1.0f);
					break;
				case dalia::LogLevel::Warning:
					levelStr = "WARN";
					color = ImVec4(1.0f, 0.7f, 0.2f, 1.0f);
					break;
				case dalia::LogLevel::Error:
					levelStr = "ERROR";
					color = ImVec4(1.0f, 0.2f, 0.2f, 1.0f);
					break;
				case dalia::LogLevel::Critical:
					levelStr = "CRIT";
					color = ImVec4(1.0f, 0.2f, 0.4f, 1.0f);
					break;
			}

			ImGui::TableNextRow();

			ImGui::TableSetColumnIndex(0);
			ImGui::TextColored(color, "[%s]", levelStr);

			ImGui::TableSetColumnIndex(1);
			ImGui::TextDisabled("[%s]", log.context.c_str());

			ImGui::TableSetColumnIndex(2);
			ImGui::TextWrapped("%s", log.message.c_str());
		}

		ImGui::EndTable();
	}

	if (m_consoleScrollToBottom) {
		ImGui::SetScrollHereY(1.0f);
		m_consoleScrollToBottom = false;
	}

	ImGui::EndChild();
	ImGui::End();
}

void Sandbox::DrawBusHierarchyPanel() {
	ImGui::Begin("Bus Hierarchy");

	ImGui::TextDisabled("Create New Bus");
	ImGui::InputText("##NewBusName", m_newBusNameBuffer, sizeof(m_newBusNameBuffer));

	ImGui::SameLine();
	if (ImGui::Button("Create")) {
		std::string newName(m_newBusNameBuffer);
		bool exists = false;

		for (const auto& bus : m_buses) {
			if (bus->GetIdentifier() == newName) {
				exists = true;
				break;
			}
		}

		if (exists) {
			m_showDuplicateWarning = true;
		}
		else if (!newName.empty()) {
			auto bus = std::make_unique<MixingBus>(&m_engine, newName);
			if (bus->GetResult() == dalia::Result::Ok) m_buses.push_back(std::move(bus));
			m_newBusNameBuffer[0] = '\0';
			m_showDuplicateWarning = false;
		}
	}

	if (m_showDuplicateWarning) {
		ImGui::TextColored({1.0f, 0.0f, 0.0f, 1.0f}, "Bus with name already exists");
	}

	ImGui::Separator();

	ImGui::TextDisabled("Routing Topology");

	MixingBus* masterBus = nullptr;
	for (const auto& bus : m_buses) {
		if (bus->GetIdentifier() == "Master") {
			masterBus = bus.get();
			break;
		}
	}

	if (masterBus) DrawBusNodeRecursive(masterBus);
	else ImGui::TextColored({1.0f, 0.0f, 0.0f, 1.0f}, "Error! Master bus missing");

	ImGui::End();
}

void Sandbox::DrawBusNodeRecursive(MixingBus* currentBus) {
	bool hasChildren = false;
	for (const auto& bus : m_buses) {
		if (bus->GetParentIdentifier() == currentBus->GetIdentifier() && bus.get() != currentBus) {
			hasChildren = true;
			break;
		}
	}

	// --- Set up flags ---
	ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow |ImGuiTreeNodeFlags_OpenOnDoubleClick |
		ImGuiTreeNodeFlags_SpanAvailWidth;

	if (m_selectionType == SelectionType::Bus && m_selectedObject == currentBus) {
		flags |= ImGuiTreeNodeFlags_Selected;
	}
	if (!hasChildren) {
		flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
	}

	// --- Draw node ---

	// Use pointer as ImGui ID to make sure it is unique
	bool nodeOpen = ImGui::TreeNodeEx((void*)currentBus, flags, "%s", currentBus->GetIdentifier().c_str());

	if (ImGui::BeginDragDropSource()) {
		MixingBus* payloadBus = currentBus;
		ImGui::SetDragDropPayload("DND_BUS_NODE", &payloadBus, sizeof(MixingBus*));

		ImGui::Text("Route '%s'", currentBus->GetIdentifier().c_str());

		ImGui::EndDragDropSource();
	}

	if (ImGui::BeginDragDropTarget()) {
		if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("DND_BUS_NODE")) {
			MixingBus* droppedBus = *(MixingBus**)payload->Data;

			dalia::Result res = m_engine.RouteBus(droppedBus->GetIdentifier().c_str(), currentBus->GetIdentifier().c_str());
			if (res == dalia::Result::Ok) {
				droppedBus->SetParentIdentifier(currentBus->GetIdentifier());
			}
		}
		ImGui::EndDragDropTarget();
	}

	// Selection handling
	if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
		m_selectionType = SelectionType::Bus;
		m_selectedObject = currentBus;
	}

	// Draw children
	if (nodeOpen && hasChildren) {
		for (const auto& bus : m_buses) {
			if (bus->GetParentIdentifier() == currentBus->GetIdentifier()) {
				DrawBusNodeRecursive(bus.get());
			}
		}
		ImGui::TreePop();
	}
}

void Sandbox::RefreshAvailableAssets() {
	m_availableAssets.clear();

	std::string targetDir = "assets";

	if (std::filesystem::exists(targetDir) && std::filesystem::is_directory(targetDir)) {
		for (const auto& entry : std::filesystem::recursive_directory_iterator(targetDir)) {
			if (entry.is_regular_file()) {
				// Filter audio files only (extend this when we add banks)
				std::string ext = entry.path().extension().string();
				if (ext == ".wav" || ext == ".mp3" || ext == ".ogg") {
					m_availableAssets.push_back(entry.path().generic_string());
				}
			}
		}
	}
}

