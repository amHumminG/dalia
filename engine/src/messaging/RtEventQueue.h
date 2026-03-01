#pragma once
#include "core/SPSCRingBuffer.h"
 
namespace dalia {

	struct RtEvent {
		enum class Type {
			None,
			VoiceFinished,
			PlaybackError,
			GraphSwapped
			// ...
		};
		Type type = Type::None;
		uint32_t voicePoolIndex;
		uint32_t generation; // Might be useless here?
		// Maybe this should have data as well?

		static RtEvent VoiceFinished(uint32_t voicePoolIndex) {
			RtEvent ev;
			return ev;
		}

		static RtEvent PlaybackError() {
			RtEvent ev;
			return ev;
		}

		static RtEvent GraphSwapped() {
			RtEvent ev;
			ev.type = RtEvent::Type::GraphSwapped;
			return ev;
		}

		// TODO: Add factory functions for all event types
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