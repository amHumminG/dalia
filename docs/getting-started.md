# Getting Started

!!! info "Platform Support"
    Dalia currently only supports Windows (WASAPI).

## Building the Standalone Tools
If you want to compile Dalia directly to run the Sandbox or the Studio tool (rather than linking it to your own project), 
you can build it from source.

**Requirements**

* CMake 3.21+
* C++ 20 compliant GCC or MSVC compiler

```bash
git clone https://github.com/amHumminG/dalia.git
cd dalia

cmake -B build
cmake --build build --config Release
```
*The compiled executables will be located in the `/build` directory*

!!! note "Under Development"
    The DALIA Studio tool is currently in early development and is not yet in a usable state.

## Integration
Dalia automatically detects when it is built as a subproject and will exclude the sandbox and studio applications from
the build.

**CMake FetchContent**
```cmake
include(FetchContent)
FetchContent_Declare(
    dalia
    GIT_REPOSITORY https://github.com/amHumminG/dalia.git
    GIT_TAG v0.1.0
)
FetchContent_MakeAvailable(dalia)

target_link_libraries(your_target PRIVATE dalia::engine)
```

**Git Submodule**
```bash
git submodule add -b v0.1.0 https://github.com/amHumminG/dalia.git third_party/dalia
```
```cmake
add_subdirectory(third_party/dalia EXCLUDE_FROM_ALL)
target_link_libraries(your_target PRIVATE dalia::engine)
```

## Using the API

### Error Handling
Almost every function in the Dalia API returns a `Result` enum. If an operation fails, the engine will return an error
code. It is advised to always check the return values of engine calls. You can convert any `Result` into a readable
string for logging using the `GetResultString()` function.

### Initialization and Shutdown
The `Engine` class is the central context that controls everything Dalia related. It is advised to only create
one instance of this class as it spawns multiple background threads. It is also recommended to keep your `Engine`
instance alive for the entire lifetime of your application. When you wish to destroy the engine, simply call the 
`Engine::Shutdown()` function. This will trigger the engine to release all resources and background threads.

Before an instance of `Engine` can be used to play sounds, it must be initialized. To do this, although optional,
it is recommended to create an `EngineConfig`. This config can be used to specify how the memory-footprint of the 
engine along with other useful settings. For simplicity, we will leave the config to use its default values in this 
guide.
```c++
#include <dalia/dalia.h>
#include <iostream>

int main() {
    dalia::Engine engine;
    
    dalia::EngineConfig config;
    dalia::Result res = engine.Initialize(config);
    if (res != dalia::Result::Ok) {
        // All result codes can be converted into c-strings like this
        std::cerr << "Failed to initialize audio engine. Error code: " << dalia::GetResultString(res) << std::endl;
        return -1;
    }
    
    engine.Shutdown();
}
```

### Update Tick
Dalia handles all processing on dedicated background threads. However, your game loop needs to communicate with those
threads via lock-free queues. `Engine::Update()` must be called exactly once per frame on your main thread. 
This function acts like a dispatcher, sending all engine calls collected during the frame to the background threads. 
It is therefore advised to call the `Engine::Update()` function **after** all other engine calls have been made.
```c++
// Your main game loop
void GameLoop() {
    while (isRunning) {
        engine.Update();
    }
}
```

### Logging
The `Engine` provides an internal logging system. The logs are accumulated from all background threads and get posted
by the engine during the `Engine::Update()` tick. By default, the engine prints log messages directly to the standard
output with the verbosity set to `LogLevel::Warning`. The `LogLevel` is hierarchical. This means that setting the level
to `LogLevel::Warning` will log all warnings, as well as more severe messages
(`LogLevel::Error` and `LogLevel::Critical`), while filtering out less urgent messages like `LogLevel::Info`.

If you want to route these logs to a custom destination (such as a text file, a dev-console, or your own central logger),
you can hook into this system by providing a custom sink function via the `EngineConfig` when initializing the engine.

```c++
// Define the logging sink
void CustomLogSink(dalia::LogLevel level, const char* context, const char* message) {
    // Handle the log message however you want
}

// Pass to engine when initializing
dalia::EngineConfig config;
config.logLevel = dalia::LogLevel::Info;
config.logCallback = CustomLogSink;

engine.Inititalize(config);
```
*Note: The `context` string describes which internal part of the engine the log originates from.*

### Loading and Playing a Sound
Dalia handles all asset loading asynchronously. However, you don't need to wait for the load operation to finish
(although that is possible if you provide a callback to the load function). If you trigger a playback for an asset that
is still loading, the engine will safely queue the request and play it the moment the load has finished.

Most resources and instances in the Dalia API are managed using handles. Handles are unique identifiers and become
invalid as soon as the instance it is referencing is released, destroyed, or finished. Under the hood, all handles are
represented using unsigned 64-bit integer IDs. The underlying "raw" ID of any handle can be acquired using its 
`GetRawId()` function. Handles can also be created directly from unsigned 64-bit integers if need be.

Sounds can be loaded as two types: `Resident` or `Stream`. 
* `Resident` sounds will be fully loaded into memory. This allows them to be played with very low latency and is perfect 
for shorter sounds. 
* `Stream` sounds are streamed directly from a file. This means that they consume much less RAM, but in return, 
they require slightly more processing power while playing. This type of sound is recommended for longer tracks like 
music or dialogue.
```c++
// Request the sound to be loaded into memory
dalia::SoundHandle sound;
engine.LoadSoundAsync(sound, dalia::SoundType::Resident, "assets/explosion.ogg");

// Create a playback instance for the sound
dalia::PlaybackHandle playback;
engine.CreatePlayback(playback, sound);

// Start playback
engine.PlayPlayback(playback);
```

!!! note
    Dalia currently only supports loading and streaming OGG-Vorbis files.


