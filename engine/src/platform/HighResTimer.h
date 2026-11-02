#pragma once

#include <cstdint>

namespace dalia {

	class HighResTimer {
	public:
		HighResTimer();
		~HighResTimer();
		void SleepMicroseconds(uint32_t microseconds);

	private:
		void* m_timerHandle = nullptr; // hides os-specifics
	};
}