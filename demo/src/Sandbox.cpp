#include "Sandbox.h"

#include <filesystem>

#include "DrawHelper.h"

static constexpr float DEG_TO_RAD = PI / 180.0f;

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
	ApplyTheme();
	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	io.ConfigDockingTransparentPayload = true;

	m_ui.defaultFont = io.Fonts->AddFontFromFileTTF("assets/fonts/Inter_18pt-Regular.ttf", 16.0f);
	m_ui.headerFont = io.Fonts->AddFontFromFileTTF("assets/fonts/Inter_18pt-SemiBold.ttf", 18.0f);

	io.FontDefault = m_ui.defaultFont;

	// Camera Setup
	m_spectatorCamera.position = { 0.0f, 10.0f, -10.0f };
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

void Sandbox::ApplyTheme() {
	ImGuiStyle& style = ImGui::GetStyle();
    ImVec4* colors = style.Colors;

    // Base Styling
    style.WindowRounding    = 4.0f;
    style.FrameRounding     = 3.0f;
    style.ScrollbarRounding = 2.0f;
    style.GrabRounding      = 2.0f;
    style.WindowBorderSize  = 1.0f;
    style.FrameBorderSize   = 1.0f;

    // Backgrounds
    colors[ImGuiCol_WindowBg]             = ImVec4(0.06f, 0.06f, 0.06f, 1.00f);
    colors[ImGuiCol_ChildBg]              = ImVec4(0.08f, 0.08f, 0.08f, 1.00f);
    colors[ImGuiCol_PopupBg]              = ImVec4(0.04f, 0.04f, 0.04f, 0.96f);
    colors[ImGuiCol_Border]               = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
    colors[ImGuiCol_FrameBg]              = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);

    // Interactions
    colors[ImGuiCol_FrameBgHovered]       = ImVec4(0.24f, 0.24f, 0.24f, 1.00f);
    colors[ImGuiCol_FrameBgActive]        = ImVec4(0.38f, 0.38f, 0.38f, 1.00f);
    colors[ImGuiCol_Button]               = ImVec4(0.18f, 0.18f, 0.18f, 1.00f);
    colors[ImGuiCol_ButtonHovered]        = ImVec4(0.28f, 0.28f, 0.28f, 1.00f);
    colors[ImGuiCol_ButtonActive]         = ImVec4(0.45f, 0.45f, 0.45f, 1.00f);

    // Accents
    colors[ImGuiCol_CheckMark]            = ImVec4(0.95f, 0.95f, 0.95f, 1.00f);
    colors[ImGuiCol_SliderGrab]           = ImVec4(0.60f, 0.60f, 0.60f, 1.00f);
    colors[ImGuiCol_SliderGrabActive]     = ImVec4(0.95f, 0.95f, 0.95f, 1.00f);

    // Headers & Tabs
    colors[ImGuiCol_Header]               = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
    colors[ImGuiCol_HeaderHovered]        = ImVec4(0.30f, 0.30f, 0.30f, 1.00f);
    colors[ImGuiCol_HeaderActive]         = ImVec4(0.45f, 0.45f, 0.45f, 1.00f);
    colors[ImGuiCol_Tab]                  = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
    colors[ImGuiCol_TabHovered]           = ImVec4(0.28f, 0.28f, 0.28f, 1.00f);
    colors[ImGuiCol_TabActive]            = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
    colors[ImGuiCol_TabUnfocused]         = ImVec4(0.08f, 0.08f, 0.08f, 1.00f);
    colors[ImGuiCol_TabUnfocusedActive]   = ImVec4(0.16f, 0.16f, 0.16f, 1.00f);

    // Text
    colors[ImGuiCol_Text]                 = ImVec4(0.95f, 0.95f, 0.95f, 1.00f);
    colors[ImGuiCol_TextDisabled]         = ImVec4(0.40f, 0.40f, 0.40f, 1.00f);

    // Docking specifics
    colors[ImGuiCol_TitleBg]              = ImVec4(0.04f, 0.04f, 0.04f, 1.00f);
    colors[ImGuiCol_TitleBgActive]        = ImVec4(0.08f, 0.08f, 0.08f, 1.00f);
    colors[ImGuiCol_TitleBgCollapsed]     = ImVec4(0.00f, 0.00f, 0.00f, 0.51f);
    colors[ImGuiCol_DockingPreview]       = ImVec4(0.60f, 0.60f, 0.60f, 0.70f);

	// Separators (The lines between docked panels)
	colors[ImGuiCol_Separator]            = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
	colors[ImGuiCol_SeparatorHovered]     = ImVec4(0.30f, 0.30f, 0.30f, 1.00f);
	colors[ImGuiCol_SeparatorActive]      = ImVec4(0.45f, 0.45f, 0.45f, 1.00f);

	// Resize Grips (Bottom right corners of floating windows)
	colors[ImGuiCol_ResizeGrip]           = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
	colors[ImGuiCol_ResizeGripHovered]    = ImVec4(0.30f, 0.30f, 0.30f, 1.00f);
	colors[ImGuiCol_ResizeGripActive]     = ImVec4(0.45f, 0.45f, 0.45f, 1.00f);

	// Text Selection
	colors[ImGuiCol_TextSelectedBg]       = ImVec4(0.38f, 0.38f, 0.38f, 0.50f);

	// Drag & Drop Target
	colors[ImGuiCol_DragDropTarget]       = ImVec4(0.80f, 0.80f, 0.80f, 1.00f);

	// Nav Highlight
	colors[ImGuiCol_NavHighlight]         = ImVec4(0.60f, 0.60f, 0.60f, 1.00f);

	// Scrollbars
	colors[ImGuiCol_ScrollbarBg]          = ImVec4(0.04f, 0.04f, 0.04f, 0.50f);
	colors[ImGuiCol_ScrollbarGrab]        = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
	colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.30f, 0.30f, 0.30f, 1.00f);
	colors[ImGuiCol_ScrollbarGrabActive]  = ImVec4(0.45f, 0.45f, 0.45f, 1.00f);

	// Tab Overlines (The thin line at the top of active/selected tabs)
	colors[ImGuiCol_TabSelectedOverline]        = ImVec4(0.60f, 0.60f, 0.60f, 1.00f);
	colors[ImGuiCol_TabDimmedSelectedOverline]  = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);

	// Nav Windowing
	colors[ImGuiCol_NavWindowingHighlight]      = ImVec4(0.60f, 0.60f, 0.60f, 0.70f);
	colors[ImGuiCol_NavWindowingDimBg]          = ImVec4(0.04f, 0.04f, 0.04f, 0.20f);

	// Modals
	colors[ImGuiCol_ModalWindowDimBg]           = ImVec4(0.00f, 0.00f, 0.00f, 0.60f);

	// Tables
	colors[ImGuiCol_TableHeaderBg]              = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
	colors[ImGuiCol_TableBorderStrong]          = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
	colors[ImGuiCol_TableBorderLight]           = ImVec4(0.20f, 0.20f, 0.20f, 0.50f);
	colors[ImGuiCol_TableRowBg]                 = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
	colors[ImGuiCol_TableRowBgAlt]              = ImVec4(1.00f, 1.00f, 1.00f, 0.03f);
}

void Sandbox::Update() {
	ImGuiIO& io = ImGui::GetIO();

	if (!io.WantTextInput) {
		if (IsKeyPressed(KEY_C)) {
			m_inFreeCamMode = !m_inFreeCamMode;

			if (m_inFreeCamMode) {
				DisableCursor();
				io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
				io.ConfigFlags |= ImGuiConfigFlags_NoMouse;
			}
			else {
				EnableCursor();
				io.ConfigFlags &= ~ImGuiConfigFlags_NoMouseCursorChange;
				io.ConfigFlags &= ~ImGuiConfigFlags_NoMouse;
			}
		}
	}

	if (m_inFreeCamMode) UpdateCamera(&m_spectatorCamera, CAMERA_FREE);

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
	ClearBackground(Color(60, 60, 60, 1));
	BeginMode3D(m_spectatorCamera);

	DrawEditorGrid(100, 1.0f, Fade(WHITE, 0.3f));

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
	ImGuizmo::BeginFrame();
	ImGuiID dockSpaceId = ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport());

	DrawMenuBar();

	if (m_showInspector) DrawInspector();
	if (m_showAssetBrowser) DrawAssetBrowser();
	if (m_showSceneOutliner) DrawSceneOutliner();
	if (m_showMixingHierarchy) DrawMixingHierarchy();
	if (m_showViewport) DrawViewport();
	if (m_showConsole) DrawConsole();

	if (m_showHotkeysWindow) DrawHotkeysWindow();

	rlImGuiEnd();
	EndDrawing();
}

void Sandbox::DrawMenuBar() {
	if (ImGui::BeginMainMenuBar()) {
		if (ImGui::BeginMenu("File")) {
			if (ImGui::MenuItem("Exit", "(Esc)")) m_isExiting = true;

			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("View")) {
			ImGui::MenuItem("Inspector", nullptr, &m_showInspector);
			ImGui::MenuItem("Asset Browser", nullptr, &m_showAssetBrowser);
			ImGui::MenuItem("Scene Outliner", nullptr, &m_showSceneOutliner);
			ImGui::MenuItem("Mixing Hierarchy", nullptr, &m_showMixingHierarchy);
			ImGui::MenuItem("Viewport", nullptr, &m_showViewport);
			ImGui::MenuItem("Console", nullptr, &m_showConsole);

			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Tools")) {
			if (ImGui::MenuItem("Reset Camera")) {
				m_spectatorCamera.position = { 0.0f, 10.0f, -10.0f };
				m_spectatorCamera.target = { 0.0f, 0.0f, 0.0f };
			}

			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Help")) {
			ImGui::MenuItem("Hotkeys & Controls", nullptr, &m_showHotkeysWindow);

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
			snprintf(label, sizeof(label), "[L] %u", listener.GetIndex());

			if (ImGui::Selectable(label, isSelected)) {
				m_selectionType = SelectionType::Listener;
				m_selectedObject = &listener;
			}
		}
	}

	// Buses
	if (ImGui::CollapsingHeader("Buses", ImGuiTreeNodeFlags_DefaultOpen)) {
		for (auto& bus : m_buses) {
			bool isSelected = (m_selectionType == SelectionType::Bus && m_selectedObject == bus.get());

			char label[128];
			snprintf(label, sizeof(label), "[B] %s", bus->GetIdentifier().c_str());

			if (ImGui::Selectable(label, isSelected)) {
				m_selectionType = SelectionType::Bus;
				m_selectedObject = bus.get();
			}
		}
	}

	// Playback instances
	if (ImGui::CollapsingHeader("Playback Instances", ImGuiTreeNodeFlags_DefaultOpen)) {
		for (auto& playback : m_playbackInstances) {
			bool isSelected = (m_selectionType == SelectionType::Playback && m_selectedObject == playback.get());

			const char* typeTag = (playback->GetSoundType() == dalia::SoundType::Resident) ? "Resident" : "Stream";
			char label[128];
			snprintf(label, sizeof(label), "[P] %s (%s)", playback->GetName().c_str(), typeTag);

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
		listener->DrawInspectorUI(m_ui);
	}
	else if (m_selectionType == SelectionType::Playback) {
		auto* playback = static_cast<PlaybackInstance*>(m_selectedObject);
		playback->DrawInspectorUI(m_ui);

		ImGui::Separator();
		ImGui::PushStyleColor(ImGuiCol_Button, {0.8f, 0.2f, 0.2f, 1.0f});
		if (ImGui::Button("Delete Playback Instance", ImVec2(-1, 30))) requestDelete = true;
		ImGui::PopStyleColor();
	}
	else if (m_selectionType == SelectionType::Sound) {
		auto* sound = static_cast<SoundAsset*>(m_selectedObject);

		auto spawnLambda = [this](dalia::SoundHandle handle, const std::string& filepath, dalia::SoundType soundType) {
			std::filesystem::path fullpath = filepath;
			std::string baseName = fullpath.stem().string();

			static uint32_t playbackSpawnCounter = 0;
			char nameBuffer[128];
			snprintf(nameBuffer, sizeof(nameBuffer), "%s %u", baseName.c_str(), playbackSpawnCounter++);

			auto playback = std::make_unique<PlaybackInstance>(&m_engine, handle, nameBuffer);
			if (playback->GetResult() == dalia::Result::Ok) {
				playback->SetSoundType(soundType);
				m_playbackInstances.push_back(std::move(playback));
			}
		};

		sound->DrawInspectorUI(m_ui, spawnLambda);

		ImGui::Separator();
		ImGui::PushStyleColor(ImGuiCol_Button, {0.8f, 0.2f, 0.2f, 1.0f});
		if (ImGui::Button("Delete Asset", ImVec2(-1, 30))) requestDelete = true;
		ImGui::PopStyleColor();
	}
	else if (m_selectionType == SelectionType::Bus) {
		auto* bus = static_cast<MixingBus*>(m_selectedObject);
		bus->DrawInspectorUI(m_ui);

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
	ImGui::SeparatorText("Load Sound From File");
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

	if (ImGui::Button("Load", ImVec2(-1, 0))) {
		std::string path(m_newSoundPathBuffer);
		if (!path.empty()) {
			// Check if sound already exists
			bool exists = false;
			dalia::SoundType type = (m_newSoundType == 0) ? dalia::SoundType::Resident : dalia::SoundType::Stream;
			for (const auto& sound : m_sounds) {
				if (path == sound->GetFilePath() && sound->GetType() == type) exists = true;
				break;
			}

			if (exists) {
				m_showDuplicateSoundWarning = true;
			}
			else {

				auto sound = std::make_unique<SoundAsset>(&m_engine, type, path);
				if (sound->GetResult() == dalia::Result::Ok) m_sounds.push_back(std::move(sound));
				m_newSoundPathBuffer[0] = '\0';
				m_showDuplicateSoundWarning = false;
			}
		}
	}

	if (m_showDuplicateSoundWarning) {
		ImGui::TextColored({1.0f, 0.0f, 0.0f, 1.0f}, "Sound is already loaded");
	}

	ImGui::SeparatorText("Loaded Assets");

	for (auto& sound : m_sounds) {
		bool isSelected = (m_selectionType == SelectionType::Sound && m_selectedObject == sound.get());

		std::filesystem::path path(sound->GetFilePath());
		std::string filename = path.filename().string();

		const char* typeTag = (sound->GetType() == dalia::SoundType::Resident) ? "Resident" : "Stream";
		char label[256];
		snprintf(label, sizeof(label), "[A] %s (%s)", filename.c_str(), typeTag);

		if (ImGui::Selectable(label, isSelected)) {
			m_selectionType = SelectionType::Sound;
			m_selectedObject = sound.get();
		}
	}

	ImGui::End();
}

void Sandbox::DrawViewport() {
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
	ImGui::Begin("3D Viewport");

	// ---  Dynamic Resizing ---
	ImVec2 size = ImGui::GetContentRegionAvail();
	ImVec2 pos = ImGui::GetWindowPos();

	if (size.x > 0 && size.y > 0) {
		// Resize render texture if panel is resized
		if (size.x != static_cast<float>(m_viewportTexture.texture.width) ||
			size.y != static_cast<float>(m_viewportTexture.texture.height)) {
			UnloadRenderTexture(m_viewportTexture);
			m_viewportTexture = LoadRenderTexture(static_cast<int>(size.x), static_cast<int>(size.y));
		}

		rlImGuiImageRenderTexture(&m_viewportTexture);
	}

	// --- Viewport Hover Controls ---
	static bool isDraggingCamera = false;
	static Vector2 anchorMousePos = { 0, 0 };

	if (!m_inFreeCamMode) {
		ImGuiIO& io = ImGui::GetIO();

		if (ImGui::IsWindowHovered()) {
			// Snap camera target to selection (F)
			if (ImGui::IsKeyPressed(ImGuiKey_F) && m_selectedObject != nullptr) {
				Vector3 focusPos = { 0, 0, 0 };
				bool hasFocusTarget = false;

				if (m_selectionType == SelectionType::Listener) {
					auto* listener = static_cast<Listener*>(m_selectedObject);
					if (listener->IsActive()) {
						if (listener->targetBody == Listener::TargetBody::Probe) focusPos = listener->GetProbePosition();
						else focusPos = listener->GetPosition();
						hasFocusTarget = true;
					}
				}
				else if (m_selectionType == SelectionType::Playback) {
					auto* playback = static_cast<PlaybackInstance*>(m_selectedObject);
					if (playback->IsSpatial()) focusPos = playback->GetPosition();
					hasFocusTarget = true;
				}

				if (hasFocusTarget) {
					Vector3 currentOffset = Vector3Subtract(m_spectatorCamera.position, m_spectatorCamera.target);
					Vector3 lookDirection = Vector3Normalize(currentOffset);
					float distance = Vector3Distance(m_spectatorCamera.position, focusPos);

					m_spectatorCamera.target = focusPos;
					m_spectatorCamera.position = Vector3Add(focusPos, Vector3Scale(lookDirection, distance));
				}
			}

			// Camera movement forwards and backwards using (MOUSE SCROLL)
			if (io.MouseWheel != 0.0f) {
				Vector3 lookDir = Vector3Subtract(m_spectatorCamera.target, m_spectatorCamera.position);
				float currentDist = Vector3Length(lookDir);
				lookDir = Vector3Normalize(lookDir);
				float zoomAmount = io.MouseWheel * 2.5f;

				// If we zoom past original target, push target forward
				if (zoomAmount > 0.0f && (currentDist - zoomAmount) < 1.0f) {
					m_spectatorCamera.target = Vector3Add(m_spectatorCamera.target, Vector3Scale(lookDir, zoomAmount));
				}
				m_spectatorCamera.position = Vector3Add(m_spectatorCamera.position, Vector3Scale(lookDir, zoomAmount));
			}

			if (ImGui::IsMouseClicked(ImGuiMouseButton_Right) || ImGui::IsMouseClicked(ImGuiMouseButton_Middle)) {
				isDraggingCamera = true;
				anchorMousePos = GetMousePosition();
				HideCursor();
			}
		}

		if (isDraggingCamera) {
			ImGui::SetMouseCursor(ImGuiMouseCursor_None);

			Vector3 movement = { 0.0f, 0.0f, 0.0f };
			Vector3 rotation = { 0.0f, 0.0f, 0.0f };

			Vector2 currentMousePos = GetMousePosition();
			Vector2 delta = {
				currentMousePos.x - anchorMousePos.x,
				currentMousePos.y - anchorMousePos.y
			};
			SetMousePosition(static_cast<int>(anchorMousePos.x), static_cast<int>(anchorMousePos.y));

			// Rotate Camera (LEFT ALT + MOUSE 2)
			if (ImGui::IsMouseDown(ImGuiMouseButton_Right) && ImGui::IsKeyDown(ImGuiKey_LeftAlt)) {
				float lookSensitivity = 0.20f;
				rotation.x = delta.x * lookSensitivity;
				rotation.y = delta.y * lookSensitivity;
				UpdateCameraPro(&m_spectatorCamera, movement, rotation, 0.0f);
			}

			// Orbit Camera (LEFT ALT + MOUSE 2)
			else if (ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
				Vector3 pivot = m_spectatorCamera.target;
				Vector3 offset = Vector3Subtract(m_spectatorCamera.position, pivot);

				float orbitSpeed = 0.005f;
				float yawAngle = -delta.x * orbitSpeed;
				float pitchAngle = -delta.y * orbitSpeed;

				// Apply yaw
				Matrix yawMatrix = MatrixRotate({0.0f,  1.0f, 0.0f}, yawAngle);
				offset = Vector3Transform(offset, yawMatrix);

				// Apply pitch
				Vector3 right = Vector3Normalize(Vector3CrossProduct({0.0f, 1.0f, 0.0f}, offset));
				Matrix pitchMatrix = MatrixRotate(right, pitchAngle);

				Vector3 newOffset = Vector3Transform(offset, pitchMatrix);
				float upDot = Vector3DotProduct(Vector3Normalize(newOffset), {0.0f, 1.0f, 0.0f});

				if (fabs(upDot) < 0.99f) offset = newOffset; // Safety check

				m_spectatorCamera.position = Vector3Add(pivot, offset);
				m_spectatorCamera.target = pivot;
			}

			// Pan Camera (MIDDLE MOUSE)
			else if (ImGui::IsMouseDown(ImGuiMouseButton_Middle)) {
				float panSensitivity = 0.05f;
				movement.y = -delta.x * panSensitivity;
				movement.z = delta.y * panSensitivity;
				UpdateCameraPro(&m_spectatorCamera, movement, rotation, 0.0f);
			}
		}

		if (isDraggingCamera) {
			if (!ImGui::IsMouseDown(ImGuiMouseButton_Right) && !ImGui::IsMouseDown(ImGuiMouseButton_Middle)) {
				isDraggingCamera = false;
				ShowCursor();
			}
		}
	}

	// --- Gizmos ---
	static ImGuizmo::OPERATION currentGizmoOp = ImGuizmo::TRANSLATE;

	bool shouldDrawGizmo = false;
	Vector3 objPos = {0, 0, 0};
	Vector3 objFwd = {0, 0, 1};
	Vector3 objUp  = {0, 1, 0};
	bool canRotate = false;

	// Should we draw a gizmo?
	if (!m_inFreeCamMode && m_selectedObject != nullptr) {
		if (m_selectionType == SelectionType::Listener) {
			auto* listener = static_cast<Listener*>(m_selectedObject);

			if (listener->IsActive()) {
				if (listener->targetBody == Listener::TargetBody::Probe) objPos = listener->GetProbePosition();
				else objPos = listener->GetPosition();

				shouldDrawGizmo = true;
				objFwd = listener->GetForward();
				if (listener->targetBody != Listener::TargetBody::Probe) canRotate = true;
			}
		}
		else if (m_selectionType == SelectionType::Playback) {
			auto* playback = static_cast<PlaybackInstance*>(m_selectedObject);

			if (playback->IsSpatial()) {
				shouldDrawGizmo = true;
				objPos = playback->GetPosition();
				canRotate = false;
				currentGizmoOp = ImGuizmo::TRANSLATE;
			}
		}
	}

	if (shouldDrawGizmo) {
		Vector3 objRight = Vector3CrossProduct(objUp, objFwd);
		if (Vector3Length(objRight) < 0.001f) objRight = { 1.0f, 0.0f, 0.0f};
		else objRight = Vector3Normalize(objRight);

		Vector3 orthoUp = Vector3Normalize(Vector3CrossProduct(objFwd, objRight));
		float matrixArr[16] = {
			objRight.x, objRight.y, objRight.z, 0.0f,	// Red Arrow
			orthoUp.x,  orthoUp.y,  orthoUp.z,  0.0f,	// Green Arrow
			objFwd.x,   objFwd.y,   objFwd.z,   0.0f,	// Blue Arrow
			objPos.x,   objPos.y,   objPos.z,   1.0f	// Position
		};

		ImGuizmo::SetDrawlist();
		ImGuizmo::SetRect(pos.x, pos.y + ImGui::GetCursorStartPos().y, size.x, size.y);

		Matrix view = MatrixTranspose(GetCameraMatrix(m_spectatorCamera));
		float aspectRatio = size.x / size.y;
		Matrix projection = MatrixTranspose(MatrixPerspective(m_spectatorCamera.fovy * DEG_TO_RAD, aspectRatio, 0.1f, 1000.0f));
		ImGuizmo::SetOrthographic(false);

		if (canRotate) {
			ImGui::SetCursorPos(ImVec2(10, 50));

			if (!ImGui::GetIO().WantTextInput) {
				if (ImGui::IsKeyPressed(ImGuiKey_W)) currentGizmoOp = ImGuizmo::TRANSLATE;
				if (ImGui::IsKeyPressed(ImGuiKey_E)) currentGizmoOp = ImGuizmo::ROTATE;
			}

			if (ImGui::RadioButton("Translate [W]", currentGizmoOp == ImGuizmo::TRANSLATE)) {
				currentGizmoOp = ImGuizmo::TRANSLATE;
			}
			ImGui::SameLine();
			if (ImGui::RadioButton("Rotate [E]", currentGizmoOp == ImGuizmo::ROTATE)) {
				currentGizmoOp = ImGuizmo::ROTATE;
			}
		}

		ImGuizmo::Manipulate(&view.m0, &projection.m0, currentGizmoOp, ImGuizmo::WORLD, matrixArr);

		if (ImGuizmo::IsUsing()) {
			if (m_selectionType == SelectionType::Listener) {
				auto* listener = static_cast<Listener*>(m_selectedObject);

				if (currentGizmoOp == ImGuizmo::TRANSLATE) {
					Vector3 draggedPos = {matrixArr[12], matrixArr[13], matrixArr[14] };
					Vector3 delta = Vector3Subtract(draggedPos, objPos);

					if (listener->targetBody == Listener::TargetBody::Head) {
						listener->SetPosition(draggedPos);
					}
					else if (listener->targetBody == Listener::TargetBody::Probe) {
						listener->SetProbePosition(draggedPos);
					}
					else if (listener->targetBody == Listener::TargetBody::Both) {
						listener->SetPosition(draggedPos);
						listener->SetProbePosition(Vector3Add(listener->GetProbePosition(), delta));
					}
				}
				else if (currentGizmoOp == ImGuizmo::ROTATE) {
					listener->SetForward(Vector3Normalize({matrixArr[8], matrixArr[9], matrixArr[10] }));
				}
			}
			else if (m_selectionType == SelectionType::Playback) {
				auto* playback = static_cast<PlaybackInstance*>(m_selectedObject);
				playback->SetPosition({matrixArr[12], matrixArr[13], matrixArr[14]});
			}
		}
	}

	// Text Overlays
	ImVec2 windowPos = ImGui::GetWindowPos();
	ImVec2 windowSize = ImGui::GetWindowSize();

	if (!m_inFreeCamMode) {
		const char* text = "Press C to enter free camera mode";
		float textWidth = ImGui::CalcTextSize(text).x;
		ImGui::GetWindowDrawList()->AddText(
			ImVec2(windowPos.x + (windowSize.x / 2) - (textWidth / 2), windowPos.y + 30),
			IM_COL32(255, 255, 255, 150),
			text
		);
	}
	else {
		const char* text = "Press C to exit free camera mode";
		float textWidth = ImGui::CalcTextSize(text).x;
		ImGui::GetWindowDrawList()->AddText(
		ImVec2(windowPos.x + (windowSize.x / 2) - (textWidth / 2), windowPos.y + 30),
			IM_COL32(255, 255, 255, 150),
			text
		);
	}

	// Draw camera coordinates
	Vector3 cameraPos = m_spectatorCamera.position;
	char coordText[64];
	snprintf(coordText, sizeof(coordText), "X: %.1f Y: %.1f Z: %.1f", cameraPos.x, cameraPos.y, cameraPos.z);
	ImGui::GetWindowDrawList()->AddText(
		ImVec2(windowPos.x + 10, windowPos.y + 30),
		IM_COL32(255, 255, 255, 150),
		coordText
	);

	ImGui::End();
	ImGui::PopStyleVar();
}

void Sandbox::DrawConsole() {
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

void Sandbox::DrawMixingHierarchy() {
	ImGui::Begin("Mixing Hierarchy");

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
			m_showDuplicateBusWarning = true;
		}
		else if (!newName.empty()) {
			auto bus = std::make_unique<MixingBus>(&m_engine, newName);
			if (bus->GetResult() == dalia::Result::Ok) m_buses.push_back(std::move(bus));
			m_newBusNameBuffer[0] = '\0';
			m_showDuplicateBusWarning = false;
		}
	}

	if (m_showDuplicateBusWarning) {
		ImGui::TextColored({1.0f, 0.0f, 0.0f, 1.0f}, "Bus with name already exists");
	}

	ImGui::SeparatorText("Hierarchy");

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
	bool hasChildBuses = false;
	for (const auto& bus : m_buses) {
		if (bus->GetParentIdentifier() == currentBus->GetIdentifier() && bus.get() != currentBus) {
			hasChildBuses = true;
			break;
		}
	}

	bool hasChildPlaybacks = false;
	for (const auto& playback : m_playbackInstances) {
		if (playback->GetBusIdentifier() == currentBus->GetIdentifier()) {
			hasChildPlaybacks = true;
		}
	}

	bool hasChildren = hasChildBuses || hasChildPlaybacks;

	// --- Set up flags ---
	ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow |ImGuiTreeNodeFlags_OpenOnDoubleClick |
		ImGuiTreeNodeFlags_SpanAvailWidth;

	if (m_selectionType == SelectionType::Bus && m_selectedObject == currentBus) {
		flags |= ImGuiTreeNodeFlags_Selected;
	}
	if (!hasChildren) {
		flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
	}

	// Draw bus node (Use pointer as ImGui ID to make sure it is unique
	bool nodeOpen = ImGui::TreeNodeEx(currentBus, flags, "[B] %s", currentBus->GetIdentifier().c_str());

	// Selection handling
	if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
		m_selectionType = SelectionType::Bus;
		m_selectedObject = currentBus;
	}

	// Drag and drop logic
	if (ImGui::BeginDragDropSource()) {
		MixingBus* payloadBus = currentBus;
		ImGui::SetDragDropPayload("DND_BUS_NODE", &payloadBus, sizeof(MixingBus*));

		ImGui::Text("Route '%s'", currentBus->GetIdentifier().c_str());

		ImGui::EndDragDropSource();
	}

	if (ImGui::BeginDragDropTarget()) {
		// Accept buses
		if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("DND_BUS_NODE")) {
			MixingBus* droppedBus = *(MixingBus**)payload->Data;

			dalia::Result res = m_engine.RouteBus(droppedBus->GetIdentifier().c_str(), currentBus->GetIdentifier().c_str());
			if (res == dalia::Result::Ok) {
				droppedBus->SetParentIdentifier(currentBus->GetIdentifier());
			}
		}

		// Accept playbacks
		if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("DND_PLAYBACK_NODE")) {
			PlaybackInstance* droppedPlayback = *(PlaybackInstance**)payload->Data;

			dalia::Result res = m_engine.RoutePlayback(droppedPlayback->GetHandle(), currentBus->GetIdentifier().c_str());
			if (res != dalia::Result::Ok) droppedPlayback->SetResult(res);
			else droppedPlayback->SetBusIdentifier(currentBus->GetIdentifier());
		}

		ImGui::EndDragDropTarget();
	}

	// Draw children
	if (nodeOpen && hasChildren) {
		// Draw child buses
		for (const auto& bus : m_buses) {
			if (bus->GetParentIdentifier() == currentBus->GetIdentifier()) {
				DrawBusNodeRecursive(bus.get());
			}
		}

		// Draw child playback instances
		for (const auto& playback : m_playbackInstances) {
			if (playback->GetBusIdentifier() == currentBus->GetIdentifier()) {
				ImGuiTreeNodeFlags pFlags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen |
					ImGuiTreeNodeFlags_SpanAvailWidth;

				if (m_selectionType == SelectionType::Playback && m_selectedObject == playback.get()) {
					pFlags |= ImGuiTreeNodeFlags_Selected;
				}

				const char* typeTag = (playback->GetSoundType() == dalia::SoundType::Resident) ? "Resident" : "Stream";

				ImGui::TreeNodeEx(playback.get(), pFlags, "[P] %s (%s)", playback->GetName().c_str(), typeTag);

				if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
					m_selectionType = SelectionType::Playback;
					m_selectedObject = playback.get();
				}

				if (ImGui::BeginDragDropSource()) {
					PlaybackInstance* payloadPb = playback.get();
					ImGui::SetDragDropPayload("DND_PLAYBACK_NODE", &payloadPb, sizeof(PlaybackInstance*));

					ImGui::Text("Route '%s", playback->GetName().c_str());

					ImGui::EndDragDropSource();
				}
			}
		}
		ImGui::TreePop();
	}
}

void Sandbox::DrawHotkeysWindow() {
	ImGui::Begin("Hotkeys & Controls", &m_showHotkeysWindow, ImGuiWindowFlags_AlwaysAutoResize);

	auto DrawShortcut = [](const char* action, const char* shortcut) {
		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		ImGui::TextUnformatted(action);
		ImGui::TableNextColumn();
		ImGui::TextColored(ImVec4(0.95f, 0.95f, 0.95f, 1.0f), "%s", shortcut);
	};

	ImGui::SeparatorText("Viewport Navigation");
	if (ImGui::BeginTable("Viewport Navigation", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
		ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableSetupColumn("Shortcut", ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableHeadersRow();

		DrawShortcut("Toggle Free Camera Mode", "C");
		DrawShortcut("Focus Selected", "F");
		DrawShortcut("Orbit Selected", "Mouse2 + Drag");
		DrawShortcut("Look Around", "Left Alt + Mouse2 + Drag");
		DrawShortcut("Pan", "Mouse3 + Drag");
		DrawShortcut("Zoom", "Mouse3 Scroll");

		ImGui::EndTable();
	}

	ImGui::SeparatorText("Selection Manipulation");
	if (ImGui::BeginTable("Selection Manipulation", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
		ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableSetupColumn("Shortcut", ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableHeadersRow();

		DrawShortcut("Translate Tool", "W");
		DrawShortcut("Rotate Tool", "E");

		ImGui::EndTable();
	}

	ImGui::End();
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
