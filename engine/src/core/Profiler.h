#pragma once

#if defined(DALIA_PROFILE_ENABLE) && defined(TRACY_ENABLE)
	#include <tracy/Tracy.hpp>

	#define DALIA_PROFILE_SCOPE()			ZoneScoped
	#define DALIA_PROFILE_SCOPE_N(name)		ZoneScopedN(name)
	#define DALIA_PROFILE_AUDIO_FRAME()		FrameMarkNamed("Audio Frame")
	#define DALIA_PROFILE_THREAD(name)		tracy::SetThreadName(name)
	#define DALIA_PROFILE_PLOT(name, val)	TracyPlot(name, val)
#else
	#define DALIA_PROFILE_SCOPE()			((void)0)
	#define DALIA_PROFILE_SCOPE_N(name)		((void)0)
	#define DALIA_PROFILE_AUDIO_FRAME()		((void)0)
	#define DALIA_PROFILE_THREAD(name)		((void)0)
	#define DALIA_PROFILE_PLOT(name, val)	((void)0)
#endif
