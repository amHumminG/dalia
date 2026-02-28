#pragma once
#include "dalia/audio/AudioControl.h"
#include "../core/SPSCRingBuffer.h"
 
namespace dalia {

	struct AudioEvent {
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

		static AudioEvent VoiceFinished(uint32_t voicePoolIndex) {
			AudioEvent ev;
			return ev;
		}

		static AudioEvent PlaybackError() {
			AudioEvent ev;
			return ev;
		}

		static AudioEvent GraphSwapped() {
			AudioEvent ev;
			ev.type = AudioEvent::Type::GraphSwapped;
			return ev;
		}

		// TODO: Add factory functions for all event types
	};

	class AudioEventQueue {
	public:
		AudioEventQueue(size_t eventCapacity);
		~AudioEventQueue() = default;

		// Audio thread API
		bool Push(const AudioEvent& event);

		// Game thread API
		bool Pop(AudioEvent& event);

	private:
		SPSCRingBuffer<AudioEvent> m_buffer;
	};
}