#pragma once
#include "AudioHandle.h"
#include "SPSCRingBuffer.h"
 
namespace placeholder_name {

	struct AudioEvent {
		enum class Type {
			None,
			SoundFinished,
			PlaybackError,
			BeatMarker, // Could be useful for rythm games maybe
			// ...
		};
		Type type = Type::None;
		AudioHandle handle;
		// Maybe this should have data as well?
	};

	class EventQueue {
	public:
		EventQueue(size_t eventCapacity);
		~EventQueue() = default;

		// Audio thread API
		bool Push(const AudioEvent& event);

		// Game thread API
		bool Poll(AudioEvent& event);

	private:
		SPSCRingBuffer<AudioEvent> m_buffer;
	};
}