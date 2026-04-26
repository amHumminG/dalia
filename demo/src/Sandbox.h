#pragma once

#include "raylib.h"
#include "raymath.h"
#include "rlImGui.h"
#include "imgui.h"

#include "dalia.h"
#include "Panel.h"
#include "Listener.h"
#include "PlaybackInstance.h"
#include "SoundAsset.h"

#include <vector>
#include <memory>
#include <array>

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
	void Update();
	void Draw();

	bool m_isExiting = false;

	SelectionType m_selectionType = SelectionType::None;
	void* m_selectedObject = nullptr;
	void DrawSceneOutliner();
	void DrawInspector();
	void DrawAssetBrowser();

	dalia::Engine m_engine;
	Camera3D m_spectatorCamera;


	std::vector<std::unique_ptr<Panel>> m_panels;

	std::array<Listener, 4> m_listeners;

	std::vector<std::unique_ptr<SoundAsset>> m_sounds;
	char m_newSoundPathBuffer[256] = "";
	int m_newSoundType = 0; // Resident = 0, Stream = 1

	std::vector<std::unique_ptr<PlaybackInstance>> m_playbackInstances;
};