#pragma once

#include "engine/EngineInternalState.h"

namespace dalia {

	static Result DispatchStreamPrepare(EngineInternalState* state, const char* filepath, uint32_t& streamIndex,
	uint32_t& streamGen) {
		if (!state->streams.Allocate(streamIndex)) return Result::StreamCapacityReached;
		streamGen = state->streams.Get(streamIndex).gen.load(std::memory_order_acquire); // Read the current generation

		// Send I/O request to prepare stream
		state->streams.Get(streamIndex).state.store(StreamState::Preparing, std::memory_order_release);
		AsyncStreamCommand req = AsyncStreamCommand::PrepareStream(streamIndex,state->streams.Get(streamIndex).gen ,filepath);
		if (!state->asyncStreamRequests->Push(req)) {
			// Rollback
			state->streams.Get(streamIndex).state.store(StreamState::Free, std::memory_order_release);
			state->streams.Free(streamIndex);

			return Result::IoStreamRequestQueueFull;
		}

		return Result::Ok;
	}
}
