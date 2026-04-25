#pragma once

#include "raylib.h"
#include "raymath.h"
#include "rlImGui.h"
#include "imgui.h"

#include "dalia.h"
#include "Panel.h"
#include "Listener.h"
#include "PlaybackInstance.h"

#include <vector>
#include <memory>
#include <array>

class Sandbox {
public:
	Sandbox();
	~Sandbox();

	void Run();

private:
	void Update();
	void Draw();

	bool m_isExiting = false;

	enum class SelectionType { None, Listener, Sound, Playback, Bus };
	SelectionType m_currentSelectionType = SelectionType::None;
	void* m_selectedObject = nullptr;

	dalia::Engine m_audioEngine;
	Camera3D m_spectatorCamera;

	std::array<Listener, 4> m_listeners;

	std::vector<std::unique_ptr<Panel>> m_panels;

	std::vector<std::unique_ptr<PlaybackInstance>> m_playbackInstances;
};