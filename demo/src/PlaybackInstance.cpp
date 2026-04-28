#include "PlaybackInstance.h"

#include "raylib.h"
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

	if (isSelected) {
		// DrawSphereWires(m_position, 0.6f, 16, 16, Fade(WHITE, 0.7f));
		DrawSelectionAnchor(m_position, 0.6f, WHITE);

		// Attenuation ranges
		DrawCircle3D(m_position, m_minDistance, {1.0f, 0.0f, 0.0f}, 90.0f, Fade(baseColor, 0.5f));
		DrawCircle3D(m_position, m_maxDistance, {1.0f, 0.0f, 0.0f}, 90.0f, Fade(DARKGRAY, 0.3f));
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
		ImGui::TextColored({0.0f, 1.0f, 0.0f, 1.0f}, dalia::GetErrorString(m_result));
	}
	else {
		ImGui::TextColored({1.0f, 0.0f, 0.0f, 1.0f}, dalia::GetErrorString(m_result));
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

	if (m_isSpatial) {
		ImGui::SeparatorText("Spatial Properties");
		ImGui::Indent();

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
		ImGui::Text("Distance Mode");
		ImGui::SameLine();

		int* distanceMode = reinterpret_cast<int*>(&m_distanceMode);
		ImGui::RadioButton("From Listener", distanceMode, 0);
		ImGui::SameLine();
		ImGui::RadioButton("From Probe", distanceMode, 1);

		ImGui::TextDisabled("Listener Masks");

		bool changed = false;
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




