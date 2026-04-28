#pragma once

#include "raylib.h"
#include "raymath.h"
#include "rlImGui.h"
#include "imgui.h"
#include "ImGuizmo.h"

#include "UI.h"

#include "dalia.h"
#include "Listener.h"
#include "PlaybackInstance.h"
#include "SoundAsset.h"
#include "MixingBus.h"

#include <vector>
#include <memory>
#include <array>

struct ConsoleLog {
	dalia::LogLevel level;
	std::string context;
	std::string message;
};

class Sandbox {
public:
	Sandbox();
	~Sandbox();

	void Run();

	enum class SelectionType {
		None,
		Listener,
		Sound,
		Playback,
		Bus
	};

private:
	void ApplyTheme();
	void Update();
	void Draw();

	bool m_isExiting = false;

	SelectionType m_selectionType = SelectionType::None;
	void* m_selectedObject = nullptr;

	void DrawMenuBar();
	void DrawSceneOutliner();
	void DrawInspector();
	void DrawAssetBrowser();
	void DrawViewport();
	void DrawConsole();
	void DrawMixingHierarchy();
	void DrawBusNodeRecursive(MixingBus* currentBus); // Helper

	void DrawHotkeysWindow();

	void RefreshAvailableAssets();

	dalia::Engine m_engine;

	UIContext m_ui;

	bool m_showInspector = true;
	bool m_showAssetBrowser = true;
	bool m_showSceneOutliner = true;
	bool m_showMixingHierarchy = true;
	bool m_showViewport = true;
	bool m_showConsole = true;

	bool m_showHotkeysWindow = false;

	// 3D
	RenderTexture2D m_viewportTexture;
	Camera3D m_spectatorCamera;
	bool m_in3DMode = false;

	// std::vector<std::unique_ptr<Panel>> m_panels;

	std::array<Listener, 4> m_listeners;

	// Playback
	std::vector<std::unique_ptr<PlaybackInstance>> m_playbackInstances;

	// Assets
	std::vector<std::string> m_availableAssets;
	std::vector<std::unique_ptr<SoundAsset>> m_sounds;
	char m_newSoundPathBuffer[256] = "";
	int m_newSoundType = 0; // Resident = 0, Stream = 1
	bool m_showDuplicateSoundWarning = false;

	// Buses
	std::vector<std::unique_ptr<MixingBus>> m_buses;
	char m_newBusNameBuffer[128] = "";
	bool m_showDuplicateBusWarning = false;

	// Console logging
	std::vector<ConsoleLog> m_logs;
	bool m_consoleAutoScroll = true;
	bool m_consoleScrollToBottom = false;
	bool m_showDebug = true;
	bool m_showInfo = true;
	bool m_showWarnings = true;
	bool m_showErrors = true;
};