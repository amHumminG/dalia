# Device Management

!!! info "Platform Support"
Device management is currently only implemented for Windows (WASAPI).

## Initial State
On engine initialization, DALIA automatically binds to the operating system's default audio endpoint (output device).
If you have no need for specific device selection in your application. No device management code will be needed.

## Manual Device Selection
### Device Enumeration
To swap output devices, you first need to know what output devices are available on the system. This can be queried by
calling `GetOutputDeviceCount()`. This forces a fresh hardware scan and updates the engine's internal cache. Once the
count has been acquired, simply loop through the indices and query the metadata for each device using
`GetOutputDeviceInfo()`.

```c++
uint32_t deviceCount = 0;
if (engine->GetOutputDeviceCount(deviceCount) == dalia::Result::Ok) {
    for (uint32_t i = 0; i < deviceCount; i++) {
        dalia::OutputDeviceInfo info;
        if (engine->GetOutputDeviceInfo(i, info) == dalia::Result::Ok) {
            // info.name contains the readable device name
            // info.identifier contains the device's OS identifier
            // info.isDefault is true if the device is set as the OS's current default audio endpoint
        }
    }
}
```
**Note:** *Calling `GetOutputDeviceCount()` is a heavy OS-level operation and should never be called in per-frame update
loops.*

### Requesting a Swap
Audio device swapping is an async operation and will never block the calling thread. Once you have the identifier 
of the desired output device, you can request a swap.
```c++
engine->SetOutputDevice(info.identifier); // Target a specific output device
engine->SetOutputDevice("default");       // Revert back to tracking the OS default output device
```
**Note:** *Passing a `nullptr` or an empty string also works to revert back to OS default output.*

## Querying the State
Because device swaps take time, there is likely a multi-frame window where the requested device is being initialized.
For this reason, device state is separated into two separate concepts:

### Target
The device identifier that the engine is currently attempting to use. This updates the moment `SetOutputDevice()` is
called. It can be queried like this:
```c++
char targetId[dalia::MAX_DEVICE_STR_LEN];
engine->GetTargetOutputDeviceId(targetId, sizeof(targetId));
```

### Active
The physical device that the engine is currently outputting audio to. This updates once the async swap operation has
fully completed. It can be queried like this:
```c++
dalia::OutputDeviceInfo activeInfo;
engine->GetActiveOutputDeviceInfo(activeInfo);
```
**Note:** *If no output devices are available, calling `GetActiveOutputDeviceInfo` will still succeed and populate the
`OutputDeviceInfo` parameter with a valid struct. In that case, the name string is `"No Output Device"`, the
identifier string is `"null_device"`, and the isDefault boolean is set to `true`.*

## Building a Settings UI
Because of the async nature of device swapping, it is recommended that any UI dropdown menus are bound to the *target*
output device rather than the *active* to avoid visual flickering. Here is some pseudocode for how this can be done:
```c++
// Get engine target
char targetId[dalia::MAX_DEVICE_STR_LEN] = {0};
engine->GetTargetOutputDeviceId(targetId, sizeof(targetId));
std::string_view currentTargetId(targetId);

// Draw UI with some generic dropdown menu
UIDropdown dropdown("Audio Output Device");
dropdown.AddOption("OS Default", currentTargetId == "default");
for (const auto& device : availableDevices) {
    dropdown.AddOption(device.name, currentTargetId == device.identifier);
}

if (dropdown.SelectionChanged()) {
    engine->SetOutputDevice(dropdown.GetSelectedId());
}
```

## Device Fallbacks
If a manual hardware swap fails, the engine will always attempt to fall back to the OS default device. If no
output device is available, the engine still renders audio into "the void" as if it was playing to an actual device. This
ensures that any triggers that rely on DALIA callbacks will continue to function predictably, even if no audio output
devices are connected to the system. Once an audio device is connected again, the engine will re-attempt to play through
the OS-default audio endpoint.
