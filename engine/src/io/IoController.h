#pragma once

#include "dalia/Result.h"
#include "core/AsyncPool.h"
#include "mixer/StreamContext.h"

#include <cstdint>
#include <span>
#include <functional>

namespace dalia {

	class AsyncMessageBus;
	struct AsyncLoadCommand;
	struct AsyncStreamCommand;

	class IoController {
	public:
		explicit IoController(uint32_t streamCapacity);
		~IoController();

		Result Initialize(AsyncMessageBus* asyncMessageBus);
		void Shutdown();

		/// @brief Allocates a stream slot and queues a stream prepare.
		/// @param filepath
		/// @param streamIndex
		/// @param streamGen
		/// @return
		Result AllocateAndPrepareStream(const char* filepath, uint32_t& streamIndex, uint32_t& streamGen);

		[[nodiscard]] std::span<StreamContext> GetStreamPoolSpan();

	private:
		// --- Load Thread ---
		void HandleLoadSoundCommand(const AsyncLoadCommand& cmd);

		// --- Stream Thread ---
		void HandlePrepareStreamCommand(const AsyncStreamCommand& cmd);
		void HandleRefillStreamCommand(const AsyncStreamCommand& cmd);
		void HandleSeekStreamCommand(const AsyncStreamCommand& cmd);
		void HandleReleaseStreamCommand(const AsyncStreamCommand& cmd);

		static void FillStreamBuffer(StreamContext& stream, uint32_t bufferIndex);

		AsyncPool<StreamContext> m_streams;
		AsyncMessageBus* m_asyncMessageBus = nullptr;
	};
}