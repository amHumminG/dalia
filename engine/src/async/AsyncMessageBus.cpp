#include "AsyncMessageBus.h"

#include "core/Logger.h"
#include "core/Profiler.h"

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

		DALIA_PROFILE_PLOT_CONFIG("AsyncMessageBus: Mixer Commands");
		DALIA_PROFILE_PLOT_CONFIG("AsyncMessageBus: Stream Commands");
		DALIA_PROFILE_PLOT_CONFIG("AsyncMessageBus: Load Commands");
		DALIA_PROFILE_PLOT_CONFIG("AsyncMessageBus: Control Commands");
		DALIA_PROFILE_PLOT_CONFIG("AsyncMessageBus: Mixer Events");
		DALIA_PROFILE_PLOT_CONFIG("AsyncMessageBus: Load Events");
		DALIA_PROFILE_PLOT_CONFIG("AsyncMessageBus: Control Events");
	}

	void AsyncMessageBus::DispatchStagedCommands() {
		m_mixerCommandQueue.Dispatch();
	}

	void AsyncMessageBus::ProcessEvents() {
		DALIA_PROFILE_SCOPE_N("AsyncMessageBus::ProcessEvents");
		CatchMixerEvents();
		CatchLoadEvents();
		CatchControlEvents();
	}

	void AsyncMessageBus::ProcessMixerCommands() {
		DALIA_PROFILE_SCOPE_N("AsyncMessageBus::ProcessMixerCommands");
		CatchMixerCommands();
	}

	void AsyncMessageBus::ProcessStreamCommands() {
		DALIA_PROFILE_SCOPE_N("AsyncMessageBus::ProcessStreamCommands");
		CatchStreamCommands();
	}

	void AsyncMessageBus::ProcessLoadCommands() {
		DALIA_PROFILE_SCOPE_N("AsyncMessageBus::ProcessLoadCommands");
		CatchLoadCommands();
	}

	void AsyncMessageBus::ProcessControlCommands() {
		DALIA_PROFILE_SCOPE_N("AsyncMessageBus::ProcessControlCommands");
		CatchControlCommands();
	}

	void AsyncMessageBus::SetLoadWakeupCallback(WakeupCallback cb) {
		m_loadWakeup = std::move(cb);
	}

	void AsyncMessageBus::SetControlWakeupCallback(WakeupCallback cb) {
		m_controlWakeup = std::move(cb);
	}

	void AsyncMessageBus::PublishMixerCommand(const AsyncMixerCommand& command) {
		m_mixerCommandQueue.Enqueue(command);
	}

	bool AsyncMessageBus::PublishStreamCommand(const AsyncStreamCommand& command) {
		if (m_streamCommandQueue.Push(command)) {
			return true;
		}
		DALIA_LOG_ERR(LOG_CTX_ASYNC, "Failed to push stream command. Queue is at capacity.");
		return false;
	}

	bool AsyncMessageBus::PublishLoadCommand(const AsyncLoadCommand& command) {
		bool wasEmtpy = false;
		if (m_loadCommandQueue.Push(command, wasEmtpy)) {
			if (wasEmtpy && m_loadWakeup) m_loadWakeup();
			return true;
		}
		DALIA_LOG_ERR(LOG_CTX_ASYNC, "Failed to push load command. Queue is at capacity.");
		return false;
	}

	bool AsyncMessageBus::PublishControlCommand(const AsyncControlCommand& command) {
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

	bool AsyncMessageBus::PublishMixerEvent(const AsyncMixerEvent& event) {
		if (!m_mixerEventQueue.Push(event)) {
			DALIA_LOG_ERR(LOG_CTX_ASYNC, "Failed to push mixer event. Queue is at capacity.");
			return false;
		}
		return true;
	}

	bool AsyncMessageBus::PublishLoadEvent(const AsyncLoadEvent& event) {
		if (!m_loadEventQueue.Push(event)) {
			DALIA_LOG_ERR(LOG_CTX_ASYNC, "Failed to push load event. Queue is at capacity.");
			return false;
		}
		return true;
	}

	bool AsyncMessageBus::PublishControlEvent(const AsyncControlEvent& event) {
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
		int64_t processedCount = 0;
		AsyncMixerCommand cmd;
		while (m_mixerCommandQueue.Pop(cmd)) {
			m_mixerCommandTable.Dispatch(cmd);
			processedCount++;
		}
		DALIA_PROFILE_PLOT("AsyncMessageBus: Mixer Commands", processedCount);
	}

	void AsyncMessageBus::CatchStreamCommands() {
		int64_t processedCount = 0;
		AsyncStreamCommand cmd;
		while (m_streamCommandQueue.Pop(cmd)) {
			m_streamCommandTable.Dispatch(cmd);
			processedCount++;
		}
		DALIA_PROFILE_PLOT("AsyncMessageBus: Stream Commands", processedCount);
	}

	void AsyncMessageBus::CatchLoadCommands() {
		int64_t processedCount = 0;
		AsyncLoadCommand cmd;
		while (m_loadCommandQueue.Pop(cmd)) {
			m_loadCommandTable.Dispatch(cmd);
			processedCount++;
		}
		DALIA_PROFILE_PLOT("AsyncMessageBus: Load Commands", processedCount);
	}

	void AsyncMessageBus::CatchControlCommands() {
		int64_t processedCount = 0;
		AsyncControlCommand cmd;
		while (m_controlCommandQueue.Pop(cmd)) {
			m_controlCommandTable.Dispatch(cmd);
			processedCount++;
		}
		DALIA_PROFILE_PLOT("AsyncMessageBus: Control Commands", processedCount);
	}

	void AsyncMessageBus::CatchMixerEvents() {
		int64_t processedCount = 0;
		AsyncMixerEvent ev;
		while (m_mixerEventQueue.Pop(ev)) {
			m_mixerEventTable.Dispatch(ev);
			processedCount++;
		}
		DALIA_PROFILE_PLOT("AsyncMessageBus: Mixer Events", processedCount);
	}

	void AsyncMessageBus::CatchLoadEvents() {
		int64_t processedCount = 0;
		AsyncLoadEvent ev;
		while (m_loadEventQueue.Pop(ev)) {
			m_loadEventTable.Dispatch(ev);
			processedCount++;
		}
		DALIA_PROFILE_PLOT("AsyncMessageBus: Load Events", processedCount);
	}

	void AsyncMessageBus::CatchControlEvents() {
		int64_t processedCount = 0;
		AsyncControlEvent ev;
		while (m_controlEventQueue.Pop(ev)) {
			m_controlEventTable.Dispatch(ev);
			processedCount++;
		}
		DALIA_PROFILE_PLOT("AsyncMessageBus: Control Events", processedCount);
	}
}
