#pragma once

#if defined(DALIA_PROFILE_ENABLE) && defined(TRACY_ENABLE)
	#include <tracy/Tracy.hpp>

	inline constexpr uint32_t DALIA_TRACE_COLOR = 0x9F0004;

	namespace dalia::profile {
		struct ScopedAudioFrame {
			ScopedAudioFrame()	{ FrameMarkStart("Dalia - Audio Frame"); }
			~ScopedAudioFrame() { FrameMarkEnd("Dalia - Audio Frame"); }
		};
	}

	#define DALIA_PROFILE_SCOPE()			ZoneScopedC(DALIA_TRACE_COLOR)
	#define DALIA_PROFILE_SCOPE_N(name)		ZoneScopedNC("Dalia - " name, DALIA_TRACE_COLOR)
	#define DALIA_PROFILE_AUDIO_FRAME()		FrameMarkNamed("Dalia - Mixer Thread Wake"); \
											dalia::profile::ScopedAudioFrame daliaAudioFrame{}
	#define DALIA_PROFILE_THREAD(name)		tracy::SetThreadName(name)

	// Plot macros
	#define DALIA_PROFILE_PLOT(name, val)	TracyPlot("Dalia - " name, val)
	#define DALIA_PROFILE_PLOT_CONFIG(name)	TracyPlotConfig("Dalia - " name, tracy::PlotFormatType::Number, true, true, DALIA_TRACE_COLOR)
#else
	#define DALIA_PROFILE_SCOPE()			((void)0)
	#define DALIA_PROFILE_SCOPE_N(name)		((void)0)
	#define DALIA_PROFILE_AUDIO_FRAME()		((void)0)
	#define DALIA_PROFILE_THREAD(name)		((void)0)
	#define DALIA_PROFILE_PLOT(name, val)	((void)0)
	#define DALIA_PROFILE_PLOT_CONFIG(name)	((void)0)
#endif
