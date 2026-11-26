

# File Log.hpp

[**File List**](files.md) **>** [**dalia**](dir_7ee0a0312321b1cfc250dbf199abb419.md) **>** [**Log.hpp**](Log_8hpp.md)

[Go to the documentation of this file](Log_8hpp.md)


```C++
#pragma once


#include <cstdint>
#include <functional>

namespace dalia {

    enum class LogLevel : uint8_t {
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


