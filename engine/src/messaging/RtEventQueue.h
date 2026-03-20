#pragma once
#include "core/SPSCRingBuffer.h"
#include "dalia/audio/PlaybackControl.h"
 
namespace dalia {

	struct  RtEvent {
		enum class Type {
			None,

			// General
			MixOrderSwapped,

			// Voice Lifecycle
			VoiceStopped,
		};

		Type type = Type::None;

		// Payload
		union Data {
			struct {
				uint32_t index;
				uint32_t generation;
				PlaybackExitCondition exitCondition;
			} voice;

		} data = {};

		static RtEvent MixOrderSwapped() {
			RtEvent ev;
			ev.type = RtEvent::Type::MixOrderSwapped;
			return ev;
		}

		static RtEvent VoiceStopped(uint32_t index, uint32_t generation, PlaybackExitCondition exitCondition) {
			RtEvent ev;
			ev.type = Type::VoiceStopped;
			ev.data.voice.index = index;
			ev.data.voice.generation = generation;
			ev.data.voice.exitCondition = exitCondition;
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