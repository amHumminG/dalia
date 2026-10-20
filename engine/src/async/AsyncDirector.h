#pragma once

#include "messaging/MixerMessaging.h"
#include "messaging/AsyncLoadMessaging.h"
#include "messaging/AsyncControlMessaging.h"
#include "messaging/AsyncStreamMessaging.h"

namespace dalia {

	struct EngineInternalState;
	class AsyncMixerSystem;
	class AsyncStreamSystem;
	class AsyncLoadSystem;
	class AsyncControlSystem;

	class AsyncDirector {
	public:
		AsyncDirector(size_t mixerQueueCapacity, size_t streamQueueCapacity, size_t loadQueueCapacity, size_t controlQueueCapacity);
		~AsyncDirector() = default;

		void PushMixerCommand(const AsyncMixerCommand& command);
		void PushStreamCommand(const AsyncStreamCommand& command);
		void PushLoadCommand(const AsyncLoadCommand& command);
		void PushControlCommand(const AsyncControlCommand& command);

		void CatchMixerCommands(AsyncMixerSystem& mixerSystem);
		void CatchStreamCommands(AsyncStreamSystem& streamSystem);
		void CatchLoadCommands(AsyncLoadSystem& loadSystem);
		void CatchControlCommands(AsyncControlSystem& controlSystem);

		void PushMixerEvent(const AsyncMixerEvent& event);
		void PushLoadEvent(const AsyncLoadEvent& event);
		void PushControlEvent(const AsyncControlEvent& event);

		void CatchMixerEvents(EngineInternalState& state);
		void CatchLoadEvents(EngineInternalState& state);
		void CatchControlEvents(EngineInternalState& state);

	private:
		AsyncMixerCommandQueue m_mixerCommandQueue;
		AsyncMixerEventQueue m_mixerEventQueue;

		AsyncStreamCommandQueue m_streamCommandQueue;

		AsyncLoadCommandQueue m_loadCommandQueue;
		AsyncLoadEventQueue m_loadEventQueue;

		AsyncControlCommandQueue m_controlCommandQueue;
		AsyncControlEventQueue m_controlEventQueue;
	};
}