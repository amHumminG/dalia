#include "PlaybackInstance.h"

#include "raylib.h"
#include "raymath.h"
#include "rlImGui.h"
#include "imgui.h"

PlaybackInstance::PlaybackInstance(dalia::Engine* engine, dalia::SoundHandle sound, const std::string& name)
	: m_engine(engine), m_name(name) {
	// Define the exit callback
	auto exitCallback = [this](dalia::PlaybackHandle handle, dalia::PlaybackExitCondition exitCondition) {
		this->m_state = PlaybackState::Stopped;
		this->m_exitCondition = exitCondition;
	};

	m_result = m_engine->CreatePlayback(m_handle, sound, exitCallback);
}

PlaybackInstance::~PlaybackInstance() {
	if (m_state != PlaybackState::Stopped) m_engine->StopPlayback(m_handle);
}

void PlaybackInstance::Update() {
	if (m_state != PlaybackState::Playing) return;

	if (m_isSpatial) {
		m_position.x += m_velocity.x * GetFrameTime();
		m_position.y += m_velocity.y * GetFrameTime();
		m_position.z += m_velocity.z * GetFrameTime();

		dalia::Result res;
		 res = m_engine->SetPlaybackPosition(m_handle, {m_position.x, m_position.y, m_position.z});
		if (res != dalia::Result::Ok) m_result = res;
		res = m_engine->SetPlaybackVelocity(m_handle, {m_velocity.x, m_velocity.y, m_velocity.z});
		if (res != dalia::Result::Ok) m_result = res;
	}
}

void PlaybackInstance::Draw3D(bool isSelected) {
	if (!m_isSpatial) return;

	Color baseColor = GRAY;
	switch (m_state) {
		case PlaybackState::Inactive: baseColor = GRAY;		break;
		case PlaybackState::Playing:  baseColor = GREEN;	break;
		case PlaybackState::Paused:   baseColor = YELLOW;	break;
		case PlaybackState::Stopped:  baseColor = RED;		break;
	}

	DrawSphere(m_position, 0.5f, Fade(baseColor, 0.8f));

	if (isSelected) {
		DrawSphereWires(m_position, 0.6f, 8, 8, WHITE);

		// Gizmo (Do we need this?)
		DrawLine3D(m_position, {m_position.x + 2.0f, m_position.y, m_position.z}, RED);   // X
		DrawLine3D(m_position, {m_position.x, m_position.y + 2.0f, m_position.z}, GREEN); // Y
		DrawLine3D(m_position, {m_position.x, m_position.y, m_position.z + 2.0f}, BLUE);  // Z

		// Attenuation ranges
		DrawCircle3D(m_position, m_minDistance, {1.0f, 0.0f, 0.0f}, 90.0f, Fade(baseColor, 0.5f));
		DrawCircle3D(m_position, m_maxDistance, {1.0f, 0.0f, 0.0f}, 90.0f, Fade(DARKGRAY, 0.3f));
	}
	else {
		DrawSphereWires(m_position, 0.5f, 8, 8, DARKGRAY);
	}
}

void PlaybackInstance::DrawInspectorUI() {
	ImGui::PushID(this);

	ImGui::Text("Result: ");
	ImGui::SameLine();
	if (m_result == dalia::Result::Ok) {
		ImGui::TextColored({0.0f, 1.0f, 0.0f, 1.0f}, dalia::GetErrorString(m_result));
	}
	else {
		ImGui::TextColored({1.0f, 0.0f, 0.0f, 1.0f}, dalia::GetErrorString(m_result));
	}

	switch (m_state) {
		case PlaybackState::Inactive: ImGui::Text("State: INACTIVE");	break;
		case PlaybackState::Playing: ImGui::Text("State: PLAYING");		break;
		case PlaybackState::Paused: ImGui::Text("State: PAUSED");		break;
		case PlaybackState::Stopped: ImGui::Text("State: STOPPED");		break;
	}

	dalia::Result res;

	// --- State Control ---
	if (m_state == PlaybackState::Inactive || m_state == PlaybackState::Paused) {
		if (ImGui::Button("Play")) {
			res = m_engine->PlayPlayback(m_handle);
			if (res != dalia::Result::Ok) m_result = res;
		}
	}
	else if (m_state == PlaybackState::Playing) {
		if (ImGui::Button("Pause")) {
			res = m_engine->PausePlayback(m_handle);
			if (res != dalia::Result::Ok) m_result = res;
		}
	}

	ImGui::SameLine();
	if (ImGui::Button("Stop")) {
		res = m_engine->StopPlayback(m_handle);
		if (res != dalia::Result::Ok) m_result = res;
	}

	ImGui::Separator();

	// --- Navigation Control ---
	ImGui::TextDisabled("Navigation");

	ImGui::InputDouble("Time (s)", &m_seekTime, 1.0, 10.0, "%.1f");
	ImGui::SameLine();
	if (ImGui::Button("Seek")) {
		res = m_engine->SeekPlayback(m_handle, m_seekTime);
		if (res != dalia::Result::Ok) m_result = res;
	}

	// --- General Properties ---
	if (ImGui::SliderFloat("Volume (dB)", &m_volumeDb, -60.0f, 12.0f)) {
		res = m_engine->SetPlaybackVolumeDb(m_handle, m_volumeDb);
		if (res != dalia::Result::Ok) m_result = res;
	}

	if (ImGui::SliderFloat("Pitch", &m_pitch, 0.1f, 4.0f)) {
		res = m_engine->SetPlaybackPitch(m_handle, m_pitch);
		if (res != dalia::Result::Ok) m_result = res;
	}

	if (!m_isSpatial) {
		if (ImGui::SliderFloat("Stereo Pan", &m_stereoPan, -1.0f, 1.0f)) {
			res = m_engine->SetPlaybackStereoPan(m_handle, m_stereoPan);
			if (res != dalia::Result::Ok) m_result = res;
		}
	}

	if (ImGui::Checkbox("Loop", &m_isLooping)) {
		res = m_engine->SetPlaybackLooping(m_handle, m_isLooping);
		if (res != dalia::Result::Ok) m_result = res;
	}

	if (ImGui::Checkbox("Spatial", &m_isSpatial)) {
		res = m_engine->SetPlaybackSpatial(m_handle, m_isSpatial);
		if (res != dalia::Result::Ok) m_result = res;
	}

	ImGui::Separator();

	// --- Spatial Properties ---
	if (m_isSpatial) {
		ImGui::Indent();

		// TODO: Add position and velocity setters

		if (ImGui::SliderFloat("Min Dist", &m_minDistance, 0.1f, 100.0f) ||
			ImGui::SliderFloat("Max Dist", &m_maxDistance, m_minDistance, 500.0f)) {
			res = m_engine->SetPlaybackMinMaxDistance(m_handle, m_minDistance, m_maxDistance);
			if (res != dalia::Result::Ok) m_result = res;
		}

		if (ImGui::Checkbox("Doppler Effect", &m_useDoppler)) {
			res = m_engine->SetPlaybackUseDoppler(m_handle, m_useDoppler);
			if (res != dalia::Result::Ok) m_result = res;
		}

		if (m_useDoppler) {
			if (ImGui::SliderFloat("Doppler Factor", &m_dopplerFactor, 0.0f, 10.0f)) {
				res = m_engine->SetPlaybackDopplerFactor(m_handle, m_dopplerFactor);
				if (res != dalia::Result::Ok) m_result = res;
			}
		}

		// TODO: Add distance mode setter

		ImGui::TextDisabled("Listener Masks");

		bool l0 = false, l1 = false, l2 = false, l3 = false;
		dalia::ListenerMask mask = dalia::MASK_NONE;
		if (ImGui::Checkbox("Listener 0", &l0) || ImGui::Checkbox("Listener 1", &l1) ||
			ImGui::Checkbox("Listener 2", &l2) || ImGui::Checkbox("Listener 3", &l3)) {
			if (l0) mask |= dalia::MASK_LISTENER_0;
			if (l1) mask |= dalia::MASK_LISTENER_1;
			if (l2) mask |= dalia::MASK_LISTENER_2;
			if (l3) mask |= dalia::MASK_LISTENER_3;
			res = m_engine->SetPlaybackListenerMask(m_handle, mask);
			if (res != dalia::Result::Ok) m_result = res;
		}

		ImGui::Unindent();
	}

	ImGui::PopID();
}




