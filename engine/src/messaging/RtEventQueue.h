#pragma once
#include "core/SPSCRingBuffer.h"
 
namespace dalia {

	struct RtEvent {
		enum class Type {
			None,

			// General
			MixOrderSwapped,

			// Voice Lifecycle
			VoiceStopped,	// Manually stopped by user command
		};

		Type type = Type::None;

		// Payload
		union Data {
			struct {
				uint32_t index;
				uint32_t generation;
				// TODO: Add info here about how the voice was stopped (killed, finished, and so on...)
			} voiceState;

		} data = {};

		static RtEvent MixOrderSwapped() {
			RtEvent ev;
			ev.type = RtEvent::Type::MixOrderSwapped;
			return ev;
		}

		static RtEvent VoiceStopped(uint32_t index, uint32_t generation) {
			RtEvent ev;
			ev.type = Type::VoiceStopped;
			ev.data.voiceState.index = index;
			ev.data.voiceState.generation = generation;
			return ev;
		}
	};

	class RtEventQueue {
	public:
		RtEventQueue(size_t eventCapacity);
		~RtEventQueue() = default;

		// Audio thread API
		bool Push(const RtEvent& event);

		// Game thread API
		bool Pop(RtEvent& event);

	private:
		SPSCRingBuffer<RtEvent> m_buffer;
	};
}