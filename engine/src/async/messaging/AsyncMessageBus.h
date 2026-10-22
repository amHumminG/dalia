#pragma once

#include "async/messaging/AsyncMixerMessaging.h"
#include "async/messaging/AsyncLoadMessaging.h"
#include "async/messaging/AsyncControlMessaging.h"
#include "async/messaging/AsyncStreamMessaging.h"

#include <functional>

namespace dalia {

	template<typename TMessage>
	using MessageCallback = std::function<void(const TMessage&)>;

	using WakeupCallback = std::function<void()>;

	template<typename TMessage>
	class DispatchTable {
	public:
		static constexpr size_t MAX_TYPES = static_cast<size_t>(TMessage::Type::Count);

		void Subscribe(TMessage::Type type, MessageCallback<TMessage> cb) {
			auto index = static_cast<size_t>(type);
			if (index < MAX_TYPES) m_callbacks[index] = std::move(cb);
		}

		void Unsubscribe(TMessage::Type type) {
			auto index = static_cast<size_t>(type);
			if (index < MAX_TYPES) m_callbacks[index] = nullptr;
		}

		void Dispatch(const TMessage& msg) const {
			auto index = static_cast<size_t>(msg.type);
			if (index < MAX_TYPES && m_callbacks[index]) m_callbacks[index](msg);
		}

	private:
		std::array<MessageCallback<TMessage>, MAX_TYPES> m_callbacks{};
	};

	class MessageRouter {
	public:
		MessageRouter(
			size_t mixerQueueCapacity,
			size_t streamQueueCapacity,
			size_t loadQueueCapacity,
			size_t controlQueueCapacity
		);
		~MessageRouter() = default;

		void DispatchStagedCommands();

		// --- Updates (to be called by each async system in its update loop) ---

		void ProcessMainEvents();
		void ProcessMixerCommands();
		void ProcessStreamCommands();
		void ProcessLoadCommands();
		void ProcessControlCommands();

		// --- Wakeup Hooks (for sleeping worker threads) ---

		// void SetStreamWakeupCallback(WakeupCallback cb);
		void SetLoadWakeupCallback(WakeupCallback cb);
		void SetControlWakeupCallback(WakeupCallback cb);

		// --- Commands ---

		void PushMixerCommand(const AsyncMixerCommand& command);
		bool PushStreamCommand(const AsyncStreamCommand& command);
		bool PushLoadCommand(const AsyncLoadCommand& command);
		bool PushControlCommand(const AsyncControlCommand& command);

		void SubscribeToMixerCommand(AsyncMixerCommand::Type type, MessageCallback<AsyncMixerCommand> cb);
		void SubscribeToStreamCommand(AsyncStreamCommand::Type type, MessageCallback<AsyncStreamCommand> cb);
		void SubscribeToLoadCommand(AsyncLoadCommand::Type type, MessageCallback<AsyncLoadCommand> cb);
		void SubscribeToControlCommand(AsyncControlCommand::Type type, MessageCallback<AsyncControlCommand> cb);

		void UnsubscribeFromMixerCommand(AsyncMixerCommand::Type type);
		void UnsubscribeFromStreamCommand(AsyncStreamCommand::Type type);
		void UnsubscribeFromLoadCommand(AsyncLoadCommand::Type type);
		void UnsubscribeFromControlCommand(AsyncControlCommand::Type type);

		// --- Events ---

		bool PushMixerEvent(const AsyncMixerEvent& event);
		bool PushLoadEvent(const AsyncLoadEvent& event);
		bool PushControlEvent(const AsyncControlEvent& event);

		void SubscribeToMixerEvent(AsyncMixerEvent::Type type, MessageCallback<AsyncMixerEvent> cb);
		void SubscribeToLoadEvent(AsyncLoadEvent::Type type, MessageCallback<AsyncLoadEvent> cb);
		void SubscribeToControlEvent(AsyncControlEvent::Type type, MessageCallback<AsyncControlEvent> cb);

		// TODO: Add unsubscribe methods
		void UnsubscribeFromMixerEvent(AsyncMixerEvent::Type type);
		void UnsubscribeFromLoadEvent(AsyncLoadEvent::Type type);
		void UnsubscribeFromControlEvent(AsyncControlEvent::Type type);

	private:
		void CatchMixerCommands();
		void CatchStreamCommands();
		void CatchLoadCommands();
		void CatchControlCommands();

		void CatchMixerEvents();
		void CatchLoadEvents();
		void CatchControlEvents();

		AsyncMixerCommandQueue m_mixerCommandQueue;
		AsyncStreamCommandQueue m_streamCommandQueue;
		AsyncLoadCommandQueue m_loadCommandQueue;
		AsyncControlCommandQueue m_controlCommandQueue;

		AsyncMixerEventQueue m_mixerEventQueue;
		AsyncLoadEventQueue m_loadEventQueue;
		AsyncControlEventQueue m_controlEventQueue;

		// WakeupCallback m_streamWakeup;
		WakeupCallback m_loadWakeup;
		WakeupCallback m_controlWakeup;

		DispatchTable<AsyncMixerCommand>	m_mixerCommandTable;
		DispatchTable<AsyncStreamCommand>	m_streamCommandTable;
		DispatchTable<AsyncLoadCommand>		m_loadCommandTable;
		DispatchTable<AsyncControlCommand>	m_controlCommandTable;

		DispatchTable<AsyncMixerEvent>		m_mixerEventTable;
		DispatchTable<AsyncLoadEvent>		m_loadEventTable;
		DispatchTable<AsyncControlEvent>	m_controlEventTable;
	};
}