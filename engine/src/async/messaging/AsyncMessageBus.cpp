#include "AsyncMessageBus.h"

#include "core/Logger.h"

namespace dalia {
	AsyncMessageBus::AsyncMessageBus(size_t mixerQueueCapacity, size_t streamQueueCapacity, size_t loadQueueCapacity,
		size_t controlQueueCapacity)
			: m_mixerCommandQueue(mixerQueueCapacity),
			m_streamCommandQueue(streamQueueCapacity),
			m_loadCommandQueue(loadQueueCapacity),
			m_controlCommandQueue(controlQueueCapacity),
			m_mixerEventQueue(mixerQueueCapacity),
			m_loadEventQueue(loadQueueCapacity),
			m_controlEventQueue(controlQueueCapacity) {
	}

	void AsyncMessageBus::DispatchStagedCommands() {
		m_mixerCommandQueue.Dispatch();
	}

	void AsyncMessageBus::ProcessMainEvents() {
		CatchMixerEvents();
		CatchLoadEvents();
		CatchControlEvents();
	}

	void AsyncMessageBus::ProcessMixerCommands() {
		CatchMixerCommands();
	}

	void AsyncMessageBus::ProcessStreamCommands() {
		CatchStreamCommands();
	}

	void AsyncMessageBus::ProcessLoadCommands() {
		CatchLoadCommands();
	}

	void AsyncMessageBus::ProcessControlCommands() {
		CatchControlCommands();
	}

	// void MessageRouter::SetStreamWakeupCallback(WakeupCallback cb) {
	// 	m_streamWakeup = std::move(cb);
	// }

	void AsyncMessageBus::SetLoadWakeupCallback(WakeupCallback cb) {
		m_loadWakeup = std::move(cb);
	}

	void AsyncMessageBus::SetControlWakeupCallback(WakeupCallback cb) {
		m_controlWakeup = std::move(cb);
	}

	void AsyncMessageBus::PushMixerCommand(const AsyncMixerCommand& command) {
		m_mixerCommandQueue.Enqueue(command);
	}

	bool AsyncMessageBus::PushStreamCommand(const AsyncStreamCommand& command) {
		if (m_streamCommandQueue.Push(command)) {
			return true;
		}
		DALIA_LOG_ERR(LOG_CTX_ASYNC, "Failed to push stream command. Queue is at capacity.");
		return false;
	}

	bool AsyncMessageBus::PushLoadCommand(const AsyncLoadCommand& command) {
		bool wasEmtpy = false;
		if (m_loadCommandQueue.Push(command, wasEmtpy)) {
			if (wasEmtpy && m_loadWakeup) m_loadWakeup();
			return true;
		}
		DALIA_LOG_ERR(LOG_CTX_ASYNC, "Failed to push load command. Queue is at capacity.");
		return false;
	}

	bool AsyncMessageBus::PushControlCommand(const AsyncControlCommand& command) {
		bool wasEmpty = false;
		if (m_controlCommandQueue.Push(command, wasEmpty)) {
			if (wasEmpty && m_controlWakeup) m_controlWakeup();
			return true;
		}
		DALIA_LOG_ERR(LOG_CTX_ASYNC, "Failed to push control command. Queue is at capacity.");
		return false;
	}

	void AsyncMessageBus::SubscribeToMixerCommand(AsyncMixerCommand::Type type,
		MessageCallback<AsyncMixerCommand> cb) {
		m_mixerCommandTable.Subscribe(type, cb);
	}

	void AsyncMessageBus::SubscribeToStreamCommand(AsyncStreamCommand::Type type,
	MessageCallback<AsyncStreamCommand> cb) {
		m_streamCommandTable.Subscribe(type, cb);
	}

	void AsyncMessageBus::SubscribeToLoadCommand(AsyncLoadCommand::Type type,
		MessageCallback<AsyncLoadCommand> cb) {
		m_loadCommandTable.Subscribe(type, cb);
	}

	void AsyncMessageBus::SubscribeToControlCommand(AsyncControlCommand::Type type,
		MessageCallback<AsyncControlCommand> cb) {
		m_controlCommandTable.Subscribe(type, cb);
	}

	void AsyncMessageBus::UnsubscribeFromMixerCommand(AsyncMixerCommand::Type type) {
		m_mixerCommandTable.Unsubscribe(type);
	}

	void AsyncMessageBus::UnsubscribeFromStreamCommand(AsyncStreamCommand::Type type) {
		m_streamCommandTable.Unsubscribe(type);
	}

	void AsyncMessageBus::UnsubscribeFromLoadCommand(AsyncLoadCommand::Type type) {
		m_loadCommandTable.Unsubscribe(type);
	}

	void AsyncMessageBus::UnsubscribeFromControlCommand(AsyncControlCommand::Type type) {
		m_controlCommandTable.Unsubscribe(type);
	}

	bool AsyncMessageBus::PushMixerEvent(const AsyncMixerEvent& event) {
		if (!m_mixerEventQueue.Push(event)) {
			DALIA_LOG_ERR(LOG_CTX_ASYNC, "Failed to push mixer event. Queue is at capacity.");
			return false;
		}
		return true;
	}

	bool AsyncMessageBus::PushLoadEvent(const AsyncLoadEvent& event) {
		if (!m_loadEventQueue.Push(event)) {
			DALIA_LOG_ERR(LOG_CTX_ASYNC, "Failed to push load event. Queue is at capacity.");
			return false;
		}
		return true;
	}

	bool AsyncMessageBus::PushControlEvent(const AsyncControlEvent& event) {
		if (!m_controlEventQueue.Push(event)) {
			DALIA_LOG_ERR(LOG_CTX_ASYNC, "Failed to push control event. Queue is at capacity.");
			return false;
		}
		return true;
	}

	void AsyncMessageBus::SubscribeToMixerEvent(AsyncMixerEvent::Type type, MessageCallback<AsyncMixerEvent> cb) {
		m_mixerEventTable.Subscribe(type, cb);
	}

	void AsyncMessageBus::SubscribeToLoadEvent(AsyncLoadEvent::Type type, MessageCallback<AsyncLoadEvent> cb) {
		m_loadEventTable.Subscribe(type, cb);
	}

	void AsyncMessageBus::SubscribeToControlEvent(AsyncControlEvent::Type type, MessageCallback<AsyncControlEvent> cb) {
		m_controlEventTable.Subscribe(type, cb);
	}

	void AsyncMessageBus::UnsubscribeFromMixerEvent(AsyncMixerEvent::Type type) {
		m_mixerEventTable.Unsubscribe(type);
	}

	void AsyncMessageBus::UnsubscribeFromLoadEvent(AsyncLoadEvent::Type type) {
		m_loadEventTable.Unsubscribe(type);
	}

	void AsyncMessageBus::UnsubscribeFromControlEvent(AsyncControlEvent::Type type) {
		m_controlEventTable.Unsubscribe(type);
	}

	void AsyncMessageBus::CatchMixerCommands() {
		AsyncMixerCommand cmd;
		while (m_mixerCommandQueue.Pop(cmd)) m_mixerCommandTable.Dispatch(cmd);
	}

	void AsyncMessageBus::CatchStreamCommands() {
		AsyncStreamCommand cmd;
		while (m_streamCommandQueue.Pop(cmd)) m_streamCommandTable.Dispatch(cmd);
	}

	void AsyncMessageBus::CatchLoadCommands() {
		AsyncLoadCommand cmd;
		while (m_loadCommandQueue.Pop(cmd)) m_loadCommandTable.Dispatch(cmd);
	}

	void AsyncMessageBus::CatchControlCommands() {
		AsyncControlCommand cmd;
		while (m_controlCommandQueue.Pop(cmd)) m_controlCommandTable.Dispatch(cmd);
	}

	void AsyncMessageBus::CatchMixerEvents() {
		AsyncMixerEvent ev;
		while (m_mixerEventQueue.Pop(ev)) m_mixerEventTable.Dispatch(ev);
	}

	void AsyncMessageBus::CatchLoadEvents() {
		AsyncLoadEvent ev;
		while (m_loadEventQueue.Pop(ev)) m_loadEventTable.Dispatch(ev);
	}

	void AsyncMessageBus::CatchControlEvents() {
		AsyncControlEvent ev;
		while (m_controlEventQueue.Pop(ev)) m_controlEventTable.Dispatch(ev);
	}
}
