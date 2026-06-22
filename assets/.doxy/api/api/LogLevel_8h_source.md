

# File LogLevel.h

[**File List**](files.md) **>** [**core**](dir_283be189f6de867d90b5075907410367.md) **>** [**LogLevel.h**](LogLevel_8h.md)

[Go to the documentation of this file](LogLevel_8h.md)


```C++
#pragma once
#include <functional>

namespace dalia {

    // TODO: Add descriptive comments to explain what these mean
    enum class LogLevel : int {
        Debug       = 0,
        Info        = 1,
        Warning     = 2,
        Error       = 3,
        Critical    = 4,
        None        = 5,
    };

    using LogCallback = std::function<void(LogLevel level, const char* context, const char* message)>;
}
```


