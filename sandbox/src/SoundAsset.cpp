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
			m_engine->GetSoundLength(m_handle, m_lengthInSeconds);
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

void SoundAsset::DrawInspectorUI(const UIContext& ui) {
	ImGui::PushID(this);

	ImGui::PushFont(ui.headerFont);
	const char* typeTag = (m_type == dalia::SoundType::Resident) ? "Resident" : "Streaming";
	ImGui::Text("[A] %s (%s)", m_filepath.c_str(), typeTag);
	ImGui::PopFont();
	ImGui::Separator();

	ImGui::Separator();

	ImGui::Text("Result: ");
	ImGui::SameLine();
	if (m_result == dalia::Result::Ok) {
		ImGui::TextColored({0.0f, 1.0f, 0.0f, 1.0f}, dalia::GetResultString(m_result));
	}
	else {
		ImGui::TextColored({1.0f, 0.0f, 0.0f, 1.0f}, dalia::GetResultString(m_result));
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
	}

	ImGui::Text("Length: %u min %u s",
		static_cast<uint32_t>(m_lengthInSeconds / 60.0),
		static_cast<uint32_t>(m_lengthInSeconds) % 60
	);

	ImGui::PopID();
}


