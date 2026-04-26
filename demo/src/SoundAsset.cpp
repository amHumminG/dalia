#include "SoundAsset.h"

#include "raylib.h"
#include "raymath.h"
#include "rlImGui.h"
#include "imgui.h"

SoundAsset::SoundAsset(dalia::Engine* engine, dalia::SoundType type, const std::string& filepath)
	: m_engine(engine), m_type(type), m_filepath(filepath) {
	auto loadCallback = [this](uint32_t requestId, dalia::Result loadResult) {
		if (loadResult == dalia::Result::Ok) {
			this->m_loadState = SoundLoadState::Loaded;
		}
		else {
			this->m_loadState = SoundLoadState::Failed;
			m_result = loadResult;
		}
	};

	m_result = m_engine->LoadSoundAsync(m_handle, m_type, m_filepath.c_str(), loadCallback, &m_requestId);
	if (m_result != dalia::Result::Ok) {
		m_loadState = SoundLoadState::Failed;
	}
}

SoundAsset::~SoundAsset() {
	m_engine->UnloadSound(m_handle);
}

void SoundAsset::DrawInspectorUI(const UIContext& ui, std::function<void(dalia::SoundHandle, const std::string&)> onSpawnPlayback) {
	ImGui::PushID(this);

	ImGui::PushFont(ui.headerFont);
	ImGui::Text("[S] %s", m_filepath.c_str());
	ImGui::PopFont();
	ImGui::Separator();

	ImGui::TextDisabled("Type: %s", (m_type == dalia::SoundType::Resident) ? "Resident" : "Streaming");

	ImGui::Separator();

	ImGui::Text("Result: ");
	ImGui::SameLine();
	if (m_result == dalia::Result::Ok) {
		ImGui::TextColored({0.0f, 1.0f, 0.0f, 1.0f}, dalia::GetErrorString(m_result));
	}
	else {
		ImGui::TextColored({1.0f, 0.0f, 0.0f, 1.0f}, dalia::GetErrorString(m_result));
	}

	ImGui::Text("Status: ");
	ImGui::SameLine();
	if (m_loadState == SoundLoadState::Loading) {
		ImGui::TextColored({1.0f, 1.0f, 0.0f, 1.0f}, "LOADING");
		ImGui::TextDisabled("Request ID: %u", m_requestId);
	}
	if (m_loadState == SoundLoadState::Failed) {
		ImGui::TextColored({1.0f, 0.0f, 0.0f, 1.0f}, "LOAD FAILED");
		ImGui::TextDisabled("Check result for error");
	}
	else if (m_loadState == SoundLoadState::Loaded) {
		ImGui::TextColored({0.0f, 1.0f, 0.0f, 1.0f}, "LOADED");

		ImGui::Separator();

		if (ImGui::Button("Create Playback Instance", ImVec2(-1, 30))) {
			if (onSpawnPlayback) onSpawnPlayback(m_handle, m_filepath);
		}
	}

	ImGui::PopID();
}


