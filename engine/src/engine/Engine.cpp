#include "dalia/Engine.h"

#include "engine/EngineInternalState.h"

namespace dalia {

	Engine::Engine() = default;
	Engine::~Engine() { if (m_state) m_state->Shutdown(); };

	Result Engine::Initialize(const EngineConfig& config) {
		if (m_state != nullptr) {
			DALIA_LOG_WARN(LOG_CTX_API, "Attempting to initialize engine that is already initialized.");
			return Result::AlreadyInitialized;
		}

		Logger::Init(config.logLevel, 256);
		if (config.logCallback != nullptr) Logger::SetSink(config.logCallback);

		// --- INTERNAL STATE SETUP ---
		m_state = new EngineInternalState(config);
		Result res = m_state->Initialize(config);
		if (res != Result::Ok) {
			delete m_state;
			m_state = nullptr;
			Logger::Shutdown();
			return res;
		}

		DALIA_LOG_INFO(LOG_CTX_API, "Initialized engine.");
		return Result::Ok;
	}

	Result Engine::Shutdown() {
		if (!m_state) {
			DALIA_LOG_WARN(LOG_CTX_API, "Attempting to shutdown uninitialized engine.");
			return Result::NotInitialized;
		}

		m_state->Shutdown();
		delete m_state;
		m_state = nullptr;

		DALIA_LOG_INFO(LOG_CTX_API, "Engine shutdown.");
		Logger::ProcessLogs();
		return Result::Ok;
	}

	void Engine::Update() {
		if (!EngineInternalState::IsInitialized(m_state)) return;

		m_state->deviceController->Update(); // Poll device changes
		m_state->asyncMessageBus->ProcessMainEvents(); // Process all events from background threads
		m_state->mixController->SyncPools(); // Node parameter updates
		m_state->asyncMessageBus->DispatchStagedCommands(); // Dispatch staged mixer commands

		Logger::ProcessLogs();
	}
}
