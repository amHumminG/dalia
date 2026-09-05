

# File DeviceControl.h

[**File List**](files.md) **>** [**audio**](dir_0ee167d633b723baeec4094afeaf5d43.md) **>** [**DeviceControl.h**](DeviceControl_8h.md)

[Go to the documentation of this file](DeviceControl_8h.md)


```C++
#pragma once

#include <cstdint>

namespace dalia {

    constexpr size_t MAX_STR_LEN_DEVICE = 256; // The maximum string length (including null-terminator) for device names and identifiers.

    struct OutputDeviceInfo {
        char name[MAX_STR_LEN_DEVICE]; // Readable device name.
        char identifier[MAX_STR_LEN_DEVICE]; // OS-level identifier.
        bool isDefault = false; // True if the device is set as OS default.
    };
}
```


