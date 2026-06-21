# Asset Management

As of now, the only assets supported by DALIA are sounds.

!!! info "Format Support"
    DALIA currently only supports loading sounds from OGG/Vorbis files.

## Sound Types

When you load a file into the engine, the sound type has to be specified.

### Resident
A `Resident` sound is fully decoded into PCM data and loaded entirely into RAM. This type fits for most short sounds 
(typically effects) that play frequently or trigger randomly. As it is fully decoded on load, playback does not add
any additional CPU overhead on top of sound mixing. The tradeoff is memory. This type of sound consumes a significant
amount of RAM (scaling with the length, sample rate, and channels of the file). It is therefore not advised to load longer
sounds with this type.

### Stream
A `Stream` sound is streamed from a file. The loading operation itself is therefore very cheap. The stream is double
buffered. Because of this, playing a `Stream` sound adds a tiny bit of CPU overhead during playback as one of the 
background threads has to continuously refill the buffers. The advantage is that it uses very little memory, and that
the memory usage stays consistent regardless of the file size. This type of sound is recommended for music, ambiance,
and voice-overs. It is also worth noting that as of right now, there is no way to prime a stream sound. This means that
there will be a small delay between calling play on the sound (for the first time) and playback starting. This happens
because the internal buffers have to be filled before playback can start.

## Asynchronous Loading

DALIA performs all sound loading asynchronously on background threads. Therefore, loading operations will never block
the calling thread. Successfully calling a load function will provide you with a `SoundHandle`. This handle must be
tracked in order to play and later on, unload the sound.
```c++
dalia::SoundHandle explosionSound;
engine->LoadSoundAsync(explosionSound, dalia::SoundType::Resident, "assets/explosion.ogg");
```
*Note: If you attempt to create and start a playback instance using a sound that is still loading, the engine will safely 
defer the playback start until the sound has been fully loaded.*

### Load Callbacks
A callback function can be passed to any load call to the engine. This function will be invoked when the asset has been
loaded (or failed to load). The load result will be accessible as a function parameter. The invocation is triggered
during the `Engine::Update()` tick.

When calling a load function, you can also optionally pass a pointer to retrieve a unique request ID. this ID is
passed directly into the callback, allowing an asset manager to track exactly which load request has finished.
*Note: If the requested sound is already loaded in, the engine will populate the request ID parameter with
`INVALID_REQUEST_ID` (`0`), as no new asynchronous operation was required.*
```c++
// Define the callback
void OnSoundLoaded(uint32_t requestId, dalia::Result result) {
    if (result == dalia::Result::Ok) {
        // The asset successfully loaded
    }
    else {
        // Handle load error
    }
}

// Load the sound
uint32_t requestId;
engine->LoadSoundAsync(explosionSound, dalia::SoundType::Resident, "assets/explosion.ogg", OnSoundLoaded, &requestId);
```

## Reference Counting & Unloading
DALIA pre-allocates all of its memory pools at startup. When you load a sound, you are claiming a spot in that pool.
Sounds are reference-counted via their filepaths. Calling `LoadSoundAsync` twice on `"explosion.ogg"` with the same
`SoundType` will simply increase the internal reference count of the existing loaded sound and hand back its handle.
```c++
engine->UnloadSound(explosionSound);
```
Because sounds are reference-counted, `UnloadSound` must be called the same number of times as `LoadSoundAsync` for the
sound to be unloaded.

If the reference count reaches zero on a sound that is currently being played, the engine handles the cleanup safely.
All active playbacks using that sound are forcefully stopped and trigger their (if provided) `PlaybackExitCallback`
function with `PlaybackExitCondition::ExplicitStop`. Any pending playback requests that were waiting for the sound to
load will be aborted.