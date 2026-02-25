#pragma once
#include "dalia/AudioHandle.h"
#include "SPSCRingBuffer.h"
 
namespace dalia {

	struct AudioEvent {
		enum class Type {
			None,
			VoiceFinished,
			PlaybackError,
			BeatMarker, // Could be useful for rythm games maybe
			// ...
		};
		Type type = Type::None;
		AudioHandle handle;
		// Maybe this should have data as well?

		static AudioEvent VoiceFinished() {
			AudioEvent ev;
			return ev;
		}

		static AudioEvent PlaybackError() {
			AudioEvent ev;
			return ev;
		}
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