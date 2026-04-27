#pragma once

#include "raylib.h"
#include "raymath.h"
#include "rlImGui.h"
#include "imgui.h"

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
	void DrawViewportPanel();
	void DrawConsolePanel();
	void DrawBusHierarchyPanel();
	void DrawBusNodeRecursive(MixingBus* currentBus); // Helper

	void RefreshAvailableAssets();

	UIContext m_ui;

	dalia::Engine m_engine;

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

	// Buses
	std::vector<std::unique_ptr<MixingBus>> m_buses;
	char m_newBusNameBuffer[128] = "";
	bool m_showDuplicateWarning = false;

	// Console logging
	std::vector<ConsoleLog> m_logs;
	bool m_consoleAutoScroll = true;
	bool m_consoleScrollToBottom = false;
	bool m_showDebug = true;
	bool m_showInfo = true;
	bool m_showWarnings = true;
	bool m_showErrors = true;
};