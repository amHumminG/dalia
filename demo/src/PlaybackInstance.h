#pragma once

#include "raylib.h"
#include "dalia.h"

#include <string>

enum class PlaybackState {
	Inactive,
	Playing,
	Paused,
	Stopped
};

class PlaybackInstance {
public:
	PlaybackInstance(dalia::Engine* engine, dalia::SoundHandle sound, const std::string& name);
	~PlaybackInstance();

	void Update();
	void Draw3D(bool isSelected);
	void DrawInspectorUI();

	dalia::Result GetResult() const { return m_result; }
	std::string GetName() const { return m_name; }

private:
	dalia::Engine* m_engine = nullptr;
	dalia::Result m_result = dalia::Result::Ok;

	std::string m_name;

	dalia::PlaybackHandle m_handle;
	PlaybackState m_state = PlaybackState::Inactive;
	dalia::PlaybackExitCondition m_exitCondition = dalia::PlaybackExitCondition::None;

	double m_seekTime = 0.0;

	std::string m_bus = "Master";

	float m_volumeDb = 0.0f;
	float m_pitch = 1.0f;
	float m_stereoPan = 0.0f;

	bool m_isLooping = false;
	bool m_isSpatial = false;

	dalia::DistanceMode m_distanceMode = dalia::DistanceMode::FromListener;
	dalia::AttenuationCurve m_attenuationCurve = dalia::AttenuationCurve::InverseSquare;

	float m_minDistance = 1.0f;
	float m_maxDistance = 100.0f;

	Vector3 m_position = {0.0f, 0.0f, 0.0f};
	Vector3 m_velocity = {0.0f, 0.0f, 0.0f};

	bool m_useDoppler = false;
	float m_dopplerFactor = 1.0f;

	uint32_t m_listenerMask = dalia::MASK_ALL_LISTENERS;
};