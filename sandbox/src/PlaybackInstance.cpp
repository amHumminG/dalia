#include "PlaybackInstance.h"

#include "raylib.h"
#include "raymath.h"
#include "imgui.h"

#include "DrawHelper.h"

PlaybackInstance::PlaybackInstance(dalia::Engine* engine, dalia::SoundHandle sound, const std::string& name)
	: m_engine(engine), m_name(name) {
	// Define the exit callback
	auto exitCallback = [this](dalia::PlaybackHandle handle, dalia::PlaybackExitCondition exitCondition) {
		this->m_state = PlaybackState::Stopped;
		this->m_exitCondition = exitCondition;
	};

	m_result = m_engine->CreatePlayback(m_handle, sound, exitCallback);
	if (m_result == dalia::Result::Ok) {
		m_engine->GetSoundLength(sound, m_soundLength);
	}
}

PlaybackInstance::~PlaybackInstance() {
	if (m_state != PlaybackState::Stopped) m_engine->StopPlayback(m_handle);
}

void PlaybackInstance::Update(float deltaTime) {
	if (m_isSpatial) {
		UpdateMovementPreset(m_movementState, m_position, deltaTime);

		// Velocity Approximation
		Vector3 movementDelta = Vector3Subtract(m_position, m_previousPosition);
		Vector3 rawVelocity = { 0.0f, 0.0f, 0.0f };
		if (Vector3Length(movementDelta) < 10.0f) rawVelocity = Vector3Scale(movementDelta, 1.0f / deltaTime); // Should we keep this safety guard?

		float smoothing = 15.0f * deltaTime;
		m_velocity.x = std::lerp(m_velocity.x, rawVelocity.x, smoothing);
		m_velocity.y = std::lerp(m_velocity.y, rawVelocity.y, smoothing);
		m_velocity.z = std::lerp(m_velocity.z, rawVelocity.z, smoothing);

		m_previousPosition = m_position;

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
	float radius = 0.5f;
	switch (m_state) {
		case PlaybackState::Inactive:
			baseColor = GRAY;
			break;
		case PlaybackState::Playing: {
			float time = static_cast<float>(GetTime());
			float pulseSpeed = 6.0f;
			float t = (sinf(time * pulseSpeed) + 1.0f) * 0.5f;

			Color color1 = { 0, 255, 0, 255 };
			Color color2 = { 0, 117, 44, 255 };

			baseColor.r = static_cast<unsigned char>(color2.r + t * (color1.r - color2.r));
			baseColor.g = static_cast<unsigned char>(color2.g + t * (color1.g - color2.g));
			baseColor.b = static_cast<unsigned char>(color2.b + t * (color1.b - color2.b));
			baseColor.a = 255;

			radius = 0.45f + (t * 0.1f);
			break;
		}
		case PlaybackState::Paused:
			baseColor = YELLOW;
			break;
		case PlaybackState::Stopped:
			baseColor = RED;
			break;
	}
	DrawSphere(m_position, radius, Fade(baseColor, 0.8f));

	float AnchorOffset = 0.1f;
	if (isSelected) {
		DrawSelectionAnchor(m_position, radius + AnchorOffset, WHITE);

		// Attenuation ranges
		DrawCircle3D(m_position, m_minDistance, {1.0f, 0.0f, 0.0f}, 90.0f, Fade(GREEN, 0.5f));
		DrawCircle3D(m_position, m_maxDistance, {1.0f, 0.0f, 0.0f}, 90.0f, Fade(RED, 0.5f));
	}
	else {
		DrawSelectionAnchor(m_position, radius + AnchorOffset, DARKGRAY);
	}
}

void PlaybackInstance::DrawInspectorUI(const UIContext& ui) {
	ImGui::PushID(this);

	ImGui::PushFont(ui.headerFont);
	const char* typeTag = (m_soundType == dalia::SoundType::Resident) ? "Resident" : "Stream";
	ImGui::Text("[P] %s (%s)", m_name.c_str(), typeTag);
	ImGui::PopFont();

	ImGui::Separator();

	ImGui::Text("Result: ");
	ImGui::SameLine();
	if (m_result == dalia::Result::Ok) {
		ImGui::TextColored({0.0f, 1.0f, 0.0f, 1.0f}, dalia::GetResultString(m_result));
	}
	else {
		ImGui::TextColored({1.0f, 0.0f, 0.0f, 1.0f}, dalia::GetResultString(m_result));
	}

	ImGui::SeparatorText("State");
	dalia::Result res;

	switch (m_state) {
		case PlaybackState::Inactive: ImGui::TextColored({0.7f, 0.7f, 0.7f, 1.0f}, "INACTIVE");	break;
		case PlaybackState::Playing:  ImGui::TextColored({0.0f, 1.0f, 0.0f, 1.0f}, "PLAYING");	break;
		case PlaybackState::Paused:   ImGui::TextColored({1.0f, 1.0f, 0.0f, 1.0f}, "PAUSED");	break;
		case PlaybackState::Stopped:  ImGui::TextColored({1.0f, 0.0f, 0.0f, 1.0f}, "STOPPED");	break;
	}

	if (m_state == PlaybackState::Inactive || m_state == PlaybackState::Paused) {
		if (ImGui::Button("Play")) {
			res = m_engine->PlayPlayback(m_handle);
			if (res != dalia::Result::Ok) m_result = res;
			else m_state = PlaybackState::Playing;
		}
	}
	else if (m_state == PlaybackState::Playing) {
		if (ImGui::Button("Pause")) {
			res = m_engine->PausePlayback(m_handle);
			if (res != dalia::Result::Ok) m_result = res;
			else m_state = PlaybackState::Paused;
		}
	}

	if (m_state != PlaybackState::Stopped) {
		ImGui::SameLine();
		if (ImGui::Button("Stop")) {
			res = m_engine->StopPlayback(m_handle);
			if (res != dalia::Result::Ok) m_result = res;
		}
	}

	ImGui::Text("Length: %u min %u s",
		static_cast<uint32_t>(m_soundLength / 60.0),
		static_cast<uint32_t>(m_soundLength) % 60
	);

	ImGui::InputDouble("Time (s)", &m_seekTime, 1.0, 10.0, "%.1f");
	ImGui::SameLine();
	if (ImGui::Button("Seek")) {
		res = m_engine->SeekPlayback(m_handle, m_seekTime);
		if (res != dalia::Result::Ok) m_result = res;
	}

	ImGui::SeparatorText("Routing");

	ImGui::Text("Routed to: %s", m_busIdentifier.c_str());

	ImGui::InputText("##New routing", m_routeInputBuffer, sizeof(m_routeInputBuffer));
	ImGui::SameLine();

	if (ImGui::Button("Route")) {
		std::string newBus(m_routeInputBuffer);

		dalia::Result res = m_engine->RouteBus(m_busIdentifier.c_str(), newBus.c_str());

		if (res == dalia::Result::Ok) {
			m_busIdentifier = newBus;
		}
		else {
			std::strncpy(m_routeInputBuffer, m_busIdentifier.c_str(), sizeof(m_routeInputBuffer) - 1);
		}
	}

	ImGui::SeparatorText("General Properties");

	if (ImGui::SliderFloat("Volume (dB)", &m_volumeDb, -60.0f, 12.0f)) {
		res = m_engine->SetPlaybackVolumeDb(m_handle, m_volumeDb);
		if (res != dalia::Result::Ok) m_result = res;
	}

	if (ImGui::SliderFloat("Playback Rate", &m_playbackRate, 0.1f, 4.0f)) {
		res = m_engine->SetPlaybackRate(m_handle, m_playbackRate);
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

	if (m_isSpatial) {
		ImGui::Indent();
		ImGui::SeparatorText("Spatial Properties");

		bool changed = false;
		ImGui::Text("Attenuation Curve");
		ImGui::SameLine();
		int attenuationCurve = static_cast<int>(m_attenuationCurve);
		changed |= ImGui::RadioButton("InvSquare", &attenuationCurve, static_cast<int>(dalia::AttenuationCurve::InverseSquare));
		ImGui::SameLine();
		changed |= ImGui::RadioButton("Linear", &attenuationCurve, static_cast<int>(dalia::AttenuationCurve::Linear));
		ImGui::SameLine();
		changed |= ImGui::RadioButton("Quadratic", &attenuationCurve, static_cast<int>(dalia::AttenuationCurve::Quadratic));
		if (changed) {
			m_attenuationCurve = static_cast<dalia::AttenuationCurve>(attenuationCurve);
			res = m_engine->SetPlaybackAttenuationCurve(m_handle, m_attenuationCurve);
			if (res != dalia::Result::Ok) m_result = res;
		}

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

		DrawMovementInspector(m_movementState, m_position, m_velocity);

		changed = false;
		ImGui::Text("Distance Mode");
		ImGui::SameLine();
		int distanceMode = static_cast<int>(m_distanceMode);
		changed |= ImGui::RadioButton("From Listener", &distanceMode, static_cast<int>(dalia::DistanceMode::FromListener));
		ImGui::SameLine();
		changed |= ImGui::RadioButton("From Probe", &distanceMode, static_cast<int>(dalia::DistanceMode::FromDistanceProbe));
		if (changed) {
			m_distanceMode = static_cast<dalia::DistanceMode>(distanceMode);
			res = m_engine->SetPlaybackDistanceMode(m_handle, m_distanceMode);
			if (res != dalia::Result::Ok) m_result = res;
		}

		ImGui::TextDisabled("Listener Masks");
		changed = false;
		changed |= ImGui::CheckboxFlags("Listener 0", &m_listenerMask, dalia::MASK_LISTENER_0);
		ImGui::SameLine();
		ImGui::Text("  ");
		ImGui::SameLine();
		changed |= ImGui::CheckboxFlags("Listener 1", &m_listenerMask, dalia::MASK_LISTENER_1);
		ImGui::SameLine();
		ImGui::Text("  ");
		ImGui::SameLine();
		changed |= ImGui::CheckboxFlags("Listener 2", &m_listenerMask, dalia::MASK_LISTENER_2);
		ImGui::SameLine();
		ImGui::Text("  ");
		ImGui::SameLine();
		changed |= ImGui::CheckboxFlags("Listener 3", &m_listenerMask, dalia::MASK_LISTENER_3);

		if (changed) {
			res = m_engine->SetPlaybackListenerMask(m_handle, m_listenerMask);
			if (res != dalia::Result::Ok) m_result = res;
		}

		ImGui::Unindent();
	}

	ImGui::PopID();
}




