

# File Device.hpp

[**File List**](files.md) **>** [**dalia**](dir_7ee0a0312321b1cfc250dbf199abb419.md) **>** [**Device.hpp**](Device_8hpp.md)

[Go to the documentation of this file](Device_8hpp.md)


```C++
#pragma once


#include <cstdint>

namespace dalia {

    constexpr size_t MAX_STR_LEN_DEVICE = 256;

    struct OutputDeviceInfo {
        char name[MAX_STR_LEN_DEVICE]; 
        char identifier[MAX_STR_LEN_DEVICE]; 
        bool isDefault = false; 
    };
}
```


