#pragma once

#include "dalia.h"
#include "UI.h"

#include <string>
#include <functional>

enum class SoundLoadState {
	Loading,
	Loaded,
	Failed
};

class SoundAsset {
public:
	SoundAsset(dalia::Engine* engine, dalia::SoundType type, const std::string& filepath);
	~SoundAsset();

	void DrawInspectorUI(const UIContext& ui, std::function<void(dalia::SoundHandle, const std::string&, dalia::SoundType)> onSpawnPlayback);

	dalia::SoundHandle GetHandle() const { return m_handle; }
	dalia::Result GetResult() const { return m_result; }
	SoundLoadState GetLoadState() const { return m_loadState; }
	std::string GetFilePath() const { return m_filepath; }
	dalia::SoundType GetType() const { return m_type; }

private:
	dalia::Engine* m_engine = nullptr;

	dalia::Result m_result = dalia::Result::Ok;
	dalia::SoundHandle m_handle;
	uint32_t m_requestId = 0;

	std::string m_filepath;
	dalia::SoundType m_type = dalia::SoundType::None;

	SoundLoadState m_loadState = SoundLoadState::Loading;
};