# Getting Started

!!! info "Platform Support"
    DALIA currently only supports Windows (WASAPI).

## Building the Standalone Tools
If you want to compile DALIA directly to run the Demo or the Studio tool (rather than linking it to your own project), 
you can build it from source.

**Requirements**
* CMake 3.20+
* Compiler with C++20 support

```bash
git clone https://github.com/amHumminG/dalia.git
cd dalia

cmake -B build
cmake --build build --config Release
```
*The compiled executables will be located in the `/build` directory*

!!! note "Under Development"
    The DALIA Studio tool is currently in active development and is not yet in a usable state.

## Integration and Setup
DALIA automatically detects when it is built as a subproject and will exclude the demo and studio applications from the
build.

**Via FetchContent:**
```cmake
include(FetchContent)
FetchContent_Declare(
    dalia
    GIT_REPOSITORY https://github.com/amHumminG/dalia.git
    GIT_TAG main
)
FetchContent_MakeAvailable(dalia)

target_link_libraries(YourTarget PRIVATE dalia::engine)
```

**Via Git Submodule:**
```cmake
add_subdirectory(third_party/dalia)
target_link_libraries(YourTarget PRIVATE dalia::engine)
```

## Using the API

### Initialization and Shutdown
The `dalia::Engine` class is the central context that controls everything DALIA related. It is advised to only create
one instance of this class as it spawns multiple background threads. It is recommended to keep your `dalia::Engine`
instance alive for the entire lifetime of your application. When you wish to destroy the engine, simply call the 
`dalia::Engine::Shutdown()` method. This will trigger the engine to release all resources and background threads.

Before an instance of `dalia::Engine` can be used to play sounds, it must be initialized. To do this, although optional,
it is recommended to create a `dalia::EngineConfig`. This config can be used to specify how the memory-footprint of the 
engine along with some other settings. For simplicity, we will leave the config to use its default values in this guide.
```c++
#include <dalia.h>
#include <iostream>

int main() {
    dalia::Engine audioEngine;
    
    dalia::EngineConfig config;
    dalia::Result res = audioEngine.Init(config);
    if (res != dalia::Result::Ok) {
        // All result codes can be converted into c-strings like this
        std::cerr << "Failed to initialize audio engine. Error code: " << dalia::GetResultString(res) << std::endl;
        return -1;
    }
    
    audioEngine.Shutdown();
}
```

### Update Tick
DALIA handles all processing on dedicated background threads. However, your game loop needs to communicate with those
threads via lock-free queues. `dalia::Engine::Update()` must be called exactly once per frame on your main thread. 
This method acts like a bus, sending all engine calls collected during the frame to the background threads. 
It is therefore advised to call the `dalia::Engine::Update()` method **after** all other engine calls have been made.
```c++
// Your main game loop
void GameLoop() {
    while (isRunning) {
        audioEngine.Update();
    }
}
```

### Loading and Playing a Sound
DALIA handles all asset loading asynchronously. However, you don't need to wait for the load operation to finish
(although that is possible if you provide a callback to the load method). If you trigger a playback for an asset that
is still loading, the engine will safely queue the request and play it the moment the load has finished.

Most resources and instances in the DALIA API are managed using handles. Handles are unique to identifiers and become
invalid as soon as the instance it is referencing is released, destroyed, finished. Under the hood, all handles are
represented using `uint64_t` id's. The underlying id of any handle can be acquired using its `GetRawId()` method.
Handles can also be created from a raw `uint64_t` id.

Sounds can be loaded as two types: `Resident` or `Stream`. 
* `Resident` sounds will be fully loaded into RAM. This allows them to be played with very low latency and is perfect for shorter sound effects. 
* `Stream` sounds are streamed directly from a file. This means that they consume much less RAM, but in return, they are not as quick to start playing. This type of sound is recommended for longer tracks like music or ambiance.
```c++
// Request the sound to be loaded into memory
dalia::SoundHandle sound;
audioEngine.LoadSoundAsync(sound, dalia::Sound::Type::Resident, "assets/audio/explosion.ogg");

// Create a playback instance of for the sound
dalia::PlaybackHandle playback;
audioEngine.CreatePlayback(playback, sound);

// Start playback
audioEngine.PlayPlayback(playback);
```

!!! note
    DALIA currently only supports loading OGG/Vorbis files.


