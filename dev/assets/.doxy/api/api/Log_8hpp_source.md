

# File Log.hpp

[**File List**](files.md) **>** [**dalia**](dir_7ee0a0312321b1cfc250dbf199abb419.md) **>** [**Log.hpp**](Log_8hpp.md)

[Go to the documentation of this file](Log_8hpp.md)


```C++
#pragma once


#include <functional>

namespace dalia {

    enum class LogLevel : int {
        Debug       = 0, // Logs most core actions. Suitable for engine developers.
        Info        = 1, // Logs core engine events and state changes.
        Warning     = 2, // Logs non-fatal issues that need attention.
        Error       = 3, // Logs failures that prevent an operation from succeeding.
        Critical    = 4, // Logs unrecoverable errors and fatal states.
        None        = 5, // Disables all logging.
    };

    using LogCallback = std::function<void(LogLevel level, const char* context, const char* message)>;
}
```


