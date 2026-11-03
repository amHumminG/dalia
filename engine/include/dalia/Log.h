#pragma once

/// @file LogLevel.h
///	@brief Logging severity levels and callback signatures.

#include <functional>

namespace dalia {

	/// @brief The level at which the DALIA audio engine should produce log messages.
	enum class LogLevel : int {
		Debug		= 0, // Logs most core actions. Suitable for engine developers.
		Info		= 1, // Logs core engine events and state changes.
		Warning		= 2, // Logs non-fatal issues that need attention.
		Error		= 3, // Logs failures that prevent an operation from succeeding.
		Critical	= 4, // Logs unrecoverable errors and fatal states.
		None		= 5, // Disables all logging.
	};

	/// @brief Defines the signature for custom logging sinks.
	///
	/// By providing a custom callback function on engine initialization, the integrating application can route the
	/// engine's internal logs into its own logging framework.
	///
	/// @param level The severity of the log message.
	/// @param context A short string identifying the engine subcontext from which the log originated.
	/// @param message The log text.
	using LogCallback = std::function<void(LogLevel level, const char* context, const char* message)>;
}
