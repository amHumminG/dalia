#pragma once

#include "dalia.h"
#include "UI.h"

#include <memory>
#include <string>

class Effect {
public:
	virtual ~Effect() { if (m_handle.IsValid()) m_engine->DestroyEffect(m_handle); }
	virtual void DrawInspectorUI(const UIContext& ui) = 0;

	virtual dalia::Result AttachTo(const std::string& busId, uint32_t slotIndex) {
		m_busId = busId;
		m_slotIndex = slotIndex;
		m_isAttached = true;
		return m_engine->AttachEffect(m_handle, m_busId.c_str(), m_slotIndex);
	}

	virtual dalia::Result Detach() {
		m_isAttached = false;
		m_busId.clear();
		return m_engine->DetachEffect(m_handle);
	}

	dalia::Result GetResult() const { return m_result; }

	dalia::EffectHandle GetHandle() const { return m_handle; }

	void SetName(const std::string& name) { m_name = name; }
	std::string GetName() const { return m_name; }

	bool IsAttached() const { return m_isAttached; }
	std::string GetBusId() const { return m_busId; }
	uint32_t GetSlotIndex() const { return m_slotIndex; }

protected:
	dalia::Engine* m_engine = nullptr;
	dalia::Result m_result = dalia::Result::Ok;

	dalia::EffectHandle m_handle = dalia::InvalidEffectHandle;

	std::string m_name = "New Effect";
	bool m_isAttached = false;
	std::string m_busId;
	uint32_t m_slotIndex = 0;
};