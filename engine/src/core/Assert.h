#pragma once
#include "Constants.h"
#include "core/Constants.h"
#include "core/Logger.h"

#if defined(DALIA_DEBUG)
	#define DALIA_ASSERT(condition, message, ...) \
		do { \
			if (!(condition)) { \
				DALIA_LOG_ERR(LOG_CTX_CORE, "ASSERTION_FAILED: %s \nFile: %s\nLine: %d\n" message, \
					#condition, __FILE__, __LINE__, ##__VA_ARGS__); \
				DALIA_DEBUG_BREAK(); \
			} \
		} \
		while (false)
#else
#define DALIA_ASSERT(condition, message, ...) do { (void)sizeof(condition); } while(false)
#endif