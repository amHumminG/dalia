# Playback Control

## Creating a Playback Instance
Using a `SoundHandle`, you can create a playback instance. A single `SoundHandle` can be used to spawn multiple
concurrent playback instances. To prepare a sound for playback, call `Engine::CreatePlayback()`. This function allocates a voice
from the engine's pre-allocated pool and gives you a handle that can be used to control it.
```c++
dalia::PlaybackHandle explosionPlayback;
engine->CreatePlayback(explosionPlayback, explosionSound);
```

### Exit Callbacks
You can optionally pass a `PlaybackExitCallback` function when creating the playback instance. This callback will be
triggered during the `Engine::Update()` tick when the sound exits. A playback exit could happen for a number of reasons. 
All conditions are covered by the `PlaybackExitCondition` enum. If you want to trigger something if a playback instance
finishes naturally, just check the value of the `PlaybackExitCondition` parameter. The `PlaybackHandle` parameter will 
hold the handle to the playback instance that just stopped. This is especially useful if you intend to use the same
callback for multiple playback instances.
```c++
void ExplosionExitCallback(dalia::PlaybackHandle playback, dalia::PlaybackExitCondition exitCondition) {
    if (exitCondition == dalia::PlaybackExitCondition::NaturalEnd) {
        // Handle exit logic
    }
}
```

## State Control
Once a playback instance is created, it begins in an inactive state. You control the state of the instance using the
following functions:
```c++
engine->PlayPlayback(explosionPlayback); // Starts or resumes the playback

engine->PausePlayback(explosionPlayback); // Pauses the playback

engine->StopPlayback(explosionPlayback); // Stops the playback
```
Stopping a playback instance is an irreversible action. A playback instance is considered "dead" after stopping (be it
explicitly or non-explicitly). This means that the internal voice is freed and the playback handle is invalidated. If
you want to play the sound again, a new playback instance must be created. A call to any playback control function 
using the handle of a "dead" playback instance will simply return the `Result::ExpiredHandle` error.

## Parameter Control
You can modify the parameters of a playback instance in real time. This goes for playback instances in all states
except those that have stopped (including the inactive state before the playback is started). It is advised to set all
desired starting parameters for a playback instance before calling `Engine::PlayPlayback()`. Doing this guarantees that
all parameters are applied simultaneously when the voice starts playing.

### Volume, Pitch, and Stereo Pan
Volume is measured in decibels (dB), where `0.0` is unity gain (the original volume of the sound file). Pitch is a
multiplier, where `1.0` is the original speed, `2.0` is twice as fast (and an octave higher), and `0.5` is half the
original speed (and an octave lower).
```c++
engine->SetPlaybackVolumeDb(explosionPlayback, -4.0f); // Reduce volume by 4dB

engine->SetPlaybackPitch(explosionPlayback, 1.3f); // Play 30% faster and higher
```
For standard 2D playback, you can pan the sound between the left and right speakers. The pan value is normalized from
`-1.0` (full left) to `1.0` (full right), with `0.0` representing the middle.
```c++
engine->SetPlaybackStereoPan(explosionPlayback, 0.5f); // Pan slightly to the right
```
*Note that if the playback instance has spatialization enabled, manual stereo panning is ignored.*

### Looping and Seeking
By default, a playback instance plays a sound from start to finish once. You can set a playback instance to loop
indefinitely:
```c++
engine->SetPlaybackLooping(explosionPlayback, true); // Set playback to loop
```
You can also jump to any specific time offset (in seconds) within the sound:
```c++
engine->SeekPlayback(explosionPlayback, 40.0f); // Seek to 40 seconds into the sound
```