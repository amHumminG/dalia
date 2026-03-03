#pragma once
#include "core/SPSCRingBuffer.h"
 
namespace dalia {

	struct RtEvent {
		enum class Type {
			None,

			// General
			MixOrderSwapped,

			// Voice Lifecycle
			VoiceFinished,	// Finished by reaching EOF
			VoiceStopped,	// Manually stopped by user command
			VoiceKilled		// Killed by the engine
		};

		Type type = Type::None;

		// Payload
		union Data {
			struct {
				uint32_t index;
				uint32_t generation;
			} voiceState;

		} data = {};

		static RtEvent MixOrderSwapped() {
			RtEvent ev;
			ev.type = RtEvent::Type::MixOrderSwapped;
			return ev;
		}

		static RtEvent VoiceFinished(uint32_t index, uint32_t generation) {
			RtEvent ev;
			ev.type = Type::MixOrderSwapped;
			ev.data.voiceState.index = index;
			ev.data.voiceState.generation = generation;
			return ev;
		}

		static RtEvent VoiceStopped(uint32_t index, uint32_t generation) {
			RtEvent ev;
			ev.type = Type::VoiceStopped;
			ev.data.voiceState.index = index;
			ev.data.voiceState.generation = generation;
			return ev;
		}

		static RtEvent VoiceKilled(uint32_t index, uint32_t generation) {
			RtEvent ev;
			ev.type = Type::VoiceKilled;
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