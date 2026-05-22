#pragma once
#include <functional>

namespace dalia {

	// TODO: Add descriptive comments to explain what these mean
	enum class LogLevel : int {
		Debug		= 0,
		Info		= 1,
		Warning		= 2,
		Error		= 3,
		Critical	= 4,
		None		= 5,
	};

	using LogCallback = std::function<void(LogLevel level, const char* category, const char* message)>;
}
