#pragma once

#include "raylib.h"
#include "dalia.h"
#include "UI.h"

#include <string>

class MixingBus {
public:
	MixingBus(dalia::Engine* engine, const std::string& identifier, const std::string& parentIdentifier = "Master");
	~MixingBus();

	void DrawInspectorUI(const UIContext& ui);

	std::string GetIdentifier() const { return m_identifier; }
	std::string GetParentIdentifier() const { return m_parentIdentifier; }
	void SetParentIdentifier(const std::string& parentIdentifier) { m_parentIdentifier = parentIdentifier; }
	dalia::Result GetResult() const { return m_result; }

private:
	dalia::Engine* m_engine = nullptr;
	dalia::Result m_result = dalia::Result::Ok;

	std::string m_identifier;
	std::string m_parentIdentifier;

	float m_volumeDb = 0.0f;

	char m_routeInputBuffer[128] = "";
};