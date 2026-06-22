

# Class dalia::Engine



[**ClassList**](annotated.md) **>** [**dalia**](namespacedalia.md) **>** [**Engine**](classdalia_1_1Engine.md)





* `#include <Engine.h>`





































## Public Functions

| Type | Name |
| ---: | :--- |
|  [**Result**](namespacedalia.md#enum-result) | [**AttachEffect**](#function-attacheffect) ([**EffectHandle**](structdalia_1_1EffectHandle.md) effect, const char \* busIdentifier, uint32\_t effectSlot) <br>_Inserts an allocated effect into the processing chain of a specified bus._  |
|  [**Result**](namespacedalia.md#enum-result) | [**CreateBus**](#function-createbus) (const char \* identifier, const char \* parentIdentifier="Master") <br>_Creates a new mixing bus._  |
|  [**Result**](namespacedalia.md#enum-result) | [**CreateEffect**](#function-createeffect) ([**EffectHandle**](structdalia_1_1EffectHandle.md) & effect, const TParams & params) <br>_Allocates a new DSP effect instance and initializes it with the provided parameters._  |
|  [**Result**](namespacedalia.md#enum-result) | [**CreatePlayback**](#function-createplayback) ([**PlaybackHandle**](structdalia_1_1PlaybackHandle.md) & playback, [**SoundHandle**](structdalia_1_1SoundHandle.md) sound, [**PlaybackExitCallback**](namespacedalia.md#typedef-playbackexitcallback) callback=nullptr) <br>_Creates a playback instance with a sound and prepares it for playback._  |
|  [**Result**](namespacedalia.md#enum-result) | [**DestroyBus**](#function-destroybus) (const char \* identifier) <br>_Decrements the reference count of a bus and destroys it if the count reaches zero._  |
|  [**Result**](namespacedalia.md#enum-result) | [**DestroyEffect**](#function-destroyeffect) ([**EffectHandle**](structdalia_1_1EffectHandle.md) effect) <br>_Destroys an effect._  |
|  [**Result**](namespacedalia.md#enum-result) | [**DetachEffect**](#function-detacheffect) ([**EffectHandle**](structdalia_1_1EffectHandle.md) effect) <br>_Removes an effect from the mix graph, regardless of which bus it is routed to._  |
|   | [**Engine**](#function-engine-12) () <br> |
|   | [**Engine**](#function-engine-22) (const [**Engine**](classdalia_1_1Engine.md) &) = delete<br> |
|  [**Result**](namespacedalia.md#enum-result) | [**GetSoundLength**](#function-getsoundlength) ([**SoundHandle**](structdalia_1_1SoundHandle.md) sound, double & lengthInSeconds) <br>_Fetches the total duration (in seconds) of a loaded sound._  |
|  [**Result**](namespacedalia.md#enum-result) | [**Init**](#function-init) (const [**EngineConfig**](structdalia_1_1EngineConfig.md) & config=[**EngineConfig**](structdalia_1_1EngineConfig.md){}) <br>_Initializes the engine, hardware device and background threads._  |
|  [**Result**](namespacedalia.md#enum-result) | [**LoadSoundAsync**](#function-loadsoundasync) ([**SoundHandle**](structdalia_1_1SoundHandle.md) & sound, [**SoundType**](namespacedalia.md#enum-soundtype) type, const char \* filepath, [**AssetLoadCallback**](namespacedalia.md#typedef-assetloadcallback) callback=nullptr, uint32\_t \* outRequestId=nullptr) <br>_Asynchronously loads a sound into memory or prepares a file for streaming._  |
|  [**Result**](namespacedalia.md#enum-result) | [**PausePlayback**](#function-pauseplayback) ([**PlaybackHandle**](structdalia_1_1PlaybackHandle.md) playback) <br>_Pauses a playback instance._  |
|  [**Result**](namespacedalia.md#enum-result) | [**PlayPlayback**](#function-playplayback) ([**PlaybackHandle**](structdalia_1_1PlaybackHandle.md) playback) <br>_Starts or resumes a playback instance._  |
|  [**Result**](namespacedalia.md#enum-result) | [**RouteBus**](#function-routebus) (const char \* identifier, const char \* parentIdentifier) <br>_Modifies the routing topology by changing the output destination of an existing bus._  |
|  [**Result**](namespacedalia.md#enum-result) | [**RoutePlayback**](#function-routeplayback) ([**PlaybackHandle**](structdalia_1_1PlaybackHandle.md) playback, const char \* busIdentifier) <br>_Routes a playback instance to output its audio into the specified mixing bus._  |
|  [**Result**](namespacedalia.md#enum-result) | [**SeekPlayback**](#function-seekplayback) ([**PlaybackHandle**](structdalia_1_1PlaybackHandle.md) playback, double timeInSeconds) <br>_Seeks a playback instance to a specified time offset._  |
|  [**Result**](namespacedalia.md#enum-result) | [**SetBusVolumeDb**](#function-setbusvolumedb) (const char \* identifier, float volumeDb) <br>_Sets the mixing volume of a bus in decibels (dB)._  |
|  [**Result**](namespacedalia.md#enum-result) | [**SetEffectParams**](#function-seteffectparams) ([**EffectHandle**](structdalia_1_1EffectHandle.md) effect, const TParams & params) <br>_Updates the parameters of an effect instance._  |
|  [**Result**](namespacedalia.md#enum-result) | [**SetGlobalDopplerFactor**](#function-setglobaldopplerfactor) (float globalDopplerFactor) <br>_Sets the global scaling factor for the Doppler effect for the entire engine._  |
|  [**Result**](namespacedalia.md#enum-result) | [**SetListener3DAttributes**](#function-setlistener3dattributes) (uint32\_t listenerIndex, const [**Listener3DAttributes**](structdalia_1_1Listener3DAttributes.md) & attributes) <br>_Updates the complete 3D state of a listener._  |
|  [**Result**](namespacedalia.md#enum-result) | [**SetListenerActive**](#function-setlisteneractive) (uint32\_t listenerIndex, bool active) <br>_Enables or disables a listener._  |
|  [**Result**](namespacedalia.md#enum-result) | [**SetListenerDistanceProbePosition**](#function-setlistenerdistanceprobeposition) (uint32\_t listenerIndex, const [**Vec3**](structdalia_1_1Vec3.md) & distanceProbePosition) <br>_Sets the distance probe world position for a listener._  |
|  [**Result**](namespacedalia.md#enum-result) | [**SetListenerOrientation**](#function-setlistenerorientation) (uint32\_t listenerIndex, const [**Vec3**](structdalia_1_1Vec3.md) & forward, const [**Vec3**](structdalia_1_1Vec3.md) & up) <br>_Sets the facing direction of a listener._  |
|  [**Result**](namespacedalia.md#enum-result) | [**SetListenerPosition**](#function-setlistenerposition) (uint32\_t listenerIndex, const [**Vec3**](structdalia_1_1Vec3.md) & position) <br>_Updates the world position of a listener._  |
|  [**Result**](namespacedalia.md#enum-result) | [**SetListenerVelocity**](#function-setlistenervelocity) (uint32\_t listenerIndex, const [**Vec3**](structdalia_1_1Vec3.md) & velocity) <br>_Sets the velocity of a listener._  |
|  [**Result**](namespacedalia.md#enum-result) | [**SetPlaybackAttenuationCurve**](#function-setplaybackattenuationcurve) ([**PlaybackHandle**](structdalia_1_1PlaybackHandle.md) playback, [**AttenuationCurve**](namespacedalia.md#enum-attenuationcurve) curve) <br>_Sets the curve used to evaluate volume drop-off over distance for a playback instance._  |
|  [**Result**](namespacedalia.md#enum-result) | [**SetPlaybackDistanceMode**](#function-setplaybackdistancemode) ([**PlaybackHandle**](structdalia_1_1PlaybackHandle.md) playback, [**DistanceMode**](namespacedalia.md#enum-distancemode) mode) <br>_Defines which listener position is used when calculating the distance attenuation for a playback instance._  |
|  [**Result**](namespacedalia.md#enum-result) | [**SetPlaybackDopplerFactor**](#function-setplaybackdopplerfactor) ([**PlaybackHandle**](structdalia_1_1PlaybackHandle.md) playback, float dopplerFactor) <br>_Sets the scaling factor for the Doppler effect for a playback instance._  |
|  [**Result**](namespacedalia.md#enum-result) | [**SetPlaybackListenerMask**](#function-setplaybacklistenermask) ([**PlaybackHandle**](structdalia_1_1PlaybackHandle.md) playback, [**ListenerMask**](namespacedalia.md#typedef-listenermask) mask) <br>_Sets the listener routing mask for a playback instance._  |
|  [**Result**](namespacedalia.md#enum-result) | [**SetPlaybackLooping**](#function-setplaybacklooping) ([**PlaybackHandle**](structdalia_1_1PlaybackHandle.md) playback, bool looping) <br>_Sets the looping state a playback instance._  |
|  [**Result**](namespacedalia.md#enum-result) | [**SetPlaybackMinMaxDistance**](#function-setplaybackminmaxdistance) ([**PlaybackHandle**](structdalia_1_1PlaybackHandle.md) playback, float minDistance, float maxDistance) <br>_Defines the distance boundaries for the attenuation curve of a playback instance._  |
|  [**Result**](namespacedalia.md#enum-result) | [**SetPlaybackPosition**](#function-setplaybackposition) ([**PlaybackHandle**](structdalia_1_1PlaybackHandle.md) playback, const [**Vec3**](structdalia_1_1Vec3.md) & position) <br>_Updates the 3D world position of a playback instance._  |
|  [**Result**](namespacedalia.md#enum-result) | [**SetPlaybackRate**](#function-setplaybackrate) ([**PlaybackHandle**](structdalia_1_1PlaybackHandle.md) playback, float rate) <br>_Sets the playback rate (speed) of a playback instance._  |
|  [**Result**](namespacedalia.md#enum-result) | [**SetPlaybackSpatial**](#function-setplaybackspatial) ([**PlaybackHandle**](structdalia_1_1PlaybackHandle.md) playback, bool spatial) <br>_Enables or disables 3D spatialization for a playback instance._  |
|  [**Result**](namespacedalia.md#enum-result) | [**SetPlaybackStereoPan**](#function-setplaybackstereopan) ([**PlaybackHandle**](structdalia_1_1PlaybackHandle.md) playback, float pan) <br>_Sets the stereo pan of a playback instance._  |
|  [**Result**](namespacedalia.md#enum-result) | [**SetPlaybackUseDoppler**](#function-setplaybackusedoppler) ([**PlaybackHandle**](structdalia_1_1PlaybackHandle.md) playback, bool useDoppler) <br>_Enables or disables the Doppler effect for a playback instance._  |
|  [**Result**](namespacedalia.md#enum-result) | [**SetPlaybackVelocity**](#function-setplaybackvelocity) ([**PlaybackHandle**](structdalia_1_1PlaybackHandle.md) playback, const [**Vec3**](structdalia_1_1Vec3.md) & velocity) <br>_Sets the velocity of a playback instance._  |
|  [**Result**](namespacedalia.md#enum-result) | [**SetPlaybackVolumeDb**](#function-setplaybackvolumedb) ([**PlaybackHandle**](structdalia_1_1PlaybackHandle.md) playback, float volumeDb) <br>_Sets the mixing volume of a playback instance in decibels (dB)._  |
|  [**Result**](namespacedalia.md#enum-result) | [**Shutdown**](#function-shutdown) () <br>_Halts all audio processing, terminates all background threads and frees allocated engine memory._  |
|  [**Result**](namespacedalia.md#enum-result) | [**StopPlayback**](#function-stopplayback) ([**PlaybackHandle**](structdalia_1_1PlaybackHandle.md) playback) <br>_Stops a playback instance._  |
|  [**Result**](namespacedalia.md#enum-result) | [**UnloadSound**](#function-unloadsound) ([**SoundHandle**](structdalia_1_1SoundHandle.md) sound) <br>_Decrements the reference count of a sound and unloads it if the count reaches zero._  |
|  void | [**Update**](#function-update) () <br>_The per-frame tick method for the engine._  |
|  [**Engine**](classdalia_1_1Engine.md) & | [**operator=**](#function-operator) (const [**Engine**](classdalia_1_1Engine.md) &) = delete<br> |
|   | [**~Engine**](#function-engine) () <br> |




























## Public Functions Documentation




### function AttachEffect 

_Inserts an allocated effect into the processing chain of a specified bus._ 
```C++
Result dalia::Engine::AttachEffect (
    EffectHandle effect,
    const char * busIdentifier,
    uint32_t effectSlot
) 
```



A bus can hold up to four effects at a time. Attached effects are processed sequentially from slot 0 to slot 3.




**Note:**

[Exclusive Routing] A single effect instance can only be attached to one bus at a time. Attaching an effect that is already attached to a bus will forcefully detach that effect from its old bus. Doing so is generally not recommended if the bus is being played through as it can potentially cause a pop in the audio output due to bypassing the built-in effect fade-out.




**Note:**

[Hot-Swap] If the specified slot on the target bus is already occupied by another effect, the existing effect will be forcefully detached and replaced by the new effect. Doing so is generally not recommended for the same reasons mentioned above.




**Parameters:**


* `effect` The handle to the effect instance. 
* `busIdentifier` The unique string identifier of the target bus. 
* `effectSlot` The index in the effect rack of the bus (0 to 3).



**Return value:**


* Result::Ok The effect was successfully attached to the bus. 
* Result::NotInitialized The engine is not initialized. 
* Result::InvalidHandle The effect handle is not recognized. 
* Result::ExpiredHandle The effect handle points to an already destroyed effect. 
* Result::BusNotFound The target bus does not exist. 
* Result::InvalidEffectSlot The requested slot exceeds the rack index limit. 




        

<hr>



### function CreateBus 

_Creates a new mixing bus._ 
```C++
Result dalia::Engine::CreateBus (
    const char * identifier,
    const char * parentIdentifier="Master"
) 
```





**Note:**

[Reference Counting] Buses are reference counted via their string identifiers. If CreateBus is called for an identifier that already exists, the engine will increment its reference count. If the bus already exists, any new parentIdentifier provided in subsequent calls is ignored and a warning is logged.




**Parameters:**


* `identifier` The unique string identifier of the bus. 
* `parentIdentifier` Optional. The unique string identifier of the parent bus (defaults to Master).



**Return value:**


* Result::Ok The bus was successfully created or its reference count was incremented. 
* Result::NotInitialized The engine is not initialized. 
* Result::BusNotFound The parent bus does not exist. 
* Result::BusPoolExhausted The allocated bus capacity has been reached. 




        

<hr>



### function CreateEffect 

_Allocates a new DSP effect instance and initializes it with the provided parameters._ 
```C++
template<typename TParams>
Result dalia::Engine::CreateEffect (
    EffectHandle & effect,
    const TParams & params
) 
```



This function is used to create all effects. The type of effect this function creates depends on the type of effect parameters that is passed into it.




**Template parameters:**


* `TParams` The parameter struct containing the effect settings. 



**Parameters:**


* `effect` The handle to be populated. 
* `params` The initial parameters.



**Return value:**


* Result::Ok The effect was successfully created. 
* Result::NotInitialized The engine is not initialized. 
* Result::EffectPoolExhausted The allocated effect capacity has been reached. 




        

<hr>



### function CreatePlayback 

_Creates a playback instance with a sound and prepares it for playback._ 
```C++
Result dalia::Engine::CreatePlayback (
    PlaybackHandle & playback,
    SoundHandle sound,
    PlaybackExitCallback callback=nullptr
) 
```



This function does not start playback.




**Note:**

[Deferred Playback] This function will still succeed if the sound is still being loaded asynchronously.




**Note:**

[Playback Lifecycle] Playback instances are immediately invalidated upon stopping. Because of this, always check the return value of any playback modifying methods to make sure the handle is still valid.




**Parameters:**


* `playback` The handle to be populated. 
* `sound` The handle to the sound. 
* `callback` Optional. If provided, this function will be called when the playback terminates.



**Return value:**


* Result::Ok The playback instance was successfully created. 
* Result::NotInitialized The engine is not initialized. 
* Result::InvalidHandle The sound handle is not recognized. 
* Result::SoundLoadError The sound asset previously failed to load. 
* Result::VoicePoolExhausted The allocated voice capacity has been reached. 
* Result::StreamPoolExhausted The allocated stream capacity has been reached. 
* Result::IoStreamRequestQueueFull The I/O streaming request queue is at capacity. 




        

<hr>



### function DestroyBus 

_Decrements the reference count of a bus and destroys it if the count reaches zero._ 
```C++
Result dalia::Engine::DestroyBus (
    const char * identifier
) 
```



If the bus is destroyed, any child buses or active playbacks that are routed to this bus are orphaned. An orphaned bus can be rerouted to a new parent bus, but an orphaned voice will immediately be stopped by the engine with the exit condition ExplicitStop.




**Note:**

[Master Immunity] The Master bus is permanently allocated by the engine and cannot be destroyed.




**Parameters:**


* `identifier` The unique string identifier of the bus.



**Return value:**


* Result::Ok The bus was successfully destroyed or its reference count was decremented. 
* Result::NotInitialized The engine is not initialized. 
* Result::BusNotFound The bus does not exist or has already been destroyed. 
* Result::Error Attempted to destroy Master bus. 




        

<hr>



### function DestroyEffect 

_Destroys an effect._ 
```C++
Result dalia::Engine::DestroyEffect (
    EffectHandle effect
) 
```



If the effect is attached to a bus, the engine will automatically detach it before destroying it.




**Note:**

[Attached Effects] Destroying an effect that is attached to a bus that is being played through is generally not recommended as it can cause a pop in the audio output due to bypassing the built-in effect fade-out.




**Parameters:**


* `effect` The handle to the effect instance.



**Return value:**


* Result::Ok The effect was successfully destroyed. 
* Result::NotInitialized The engine is not initialized. 
* Result::InvalidHandle The effect handle is not recognized. 
* Result::ExpiredHandle The effect handle points to an already destroyed effect. 




        

<hr>



### function DetachEffect 

_Removes an effect from the mix graph, regardless of which bus it is routed to._ 
```C++
Result dalia::Engine::DetachEffect (
    EffectHandle effect
) 
```





**Note:**

[Effect Lifetime] Detaching an effect does not destroy it.




**Parameters:**


* `effect` The handle to the effect instance.



**Return value:**


* Result::Ok The effect was successfully detached. 
* Result::NotInitialized The engine is not initialized. 
* Result::InvalidHandle The effect handle is not recognized. 
* Result::ExpiredHandle The effect handle points to an already destroyed effect. 




        

<hr>



### function Engine [1/2]

```C++
dalia::Engine::Engine () 
```




<hr>



### function Engine [2/2]

```C++
dalia::Engine::Engine (
    const Engine &
) = delete
```




<hr>



### function GetSoundLength 

_Fetches the total duration (in seconds) of a loaded sound._ 
```C++
Result dalia::Engine::GetSoundLength (
    SoundHandle sound,
    double & lengthInSeconds
) 
```





**Parameters:**


* `sound` The handle to the sound asset. 
* `lengthInSeconds` The variable to be populated with the length of the sound asset.



**Return value:**


* Result::Ok The sound length was successfully fetched. 
* Result::NotInitialized The engine is not initialized. 
* Result::InvalidHandle The sound handle is not recognized, or has already been freed. 




        

<hr>



### function Init 

_Initializes the engine, hardware device and background threads._ 
```C++
Result dalia::Engine::Init (
    const EngineConfig & config=EngineConfig {}
) 
```



This function allocates all internal memory pools and starts the background audio thread, I/O loading thread, and I/O streaming thread. It must be called once before any other engine methods are used.




**Note:**

[Hardware Device] The engine will default to the OS's default audio device.




**Parameters:**


* `config` The initialization settings for the engine.



**Return value:**


* Result::Ok The engine was successfully initialized and is ready for use. 
* Result::AlreadyInitialized Attempted to initialize an already initialized engine. 
* Result::DeviceFailed The operating system failed to initialize or start the audio device. 




        

<hr>



### function LoadSoundAsync 

_Asynchronously loads a sound into memory or prepares a file for streaming._ 
```C++
Result dalia::Engine::LoadSoundAsync (
    SoundHandle & sound,
    SoundType type,
    const char * filepath,
    AssetLoadCallback callback=nullptr,
    uint32_t * outRequestId=nullptr
) 
```



This function returns immediately and does not block the calling thread. The provided sound is valid immediately if the call succeeds. If playback using the sound is attempted before the load is complete, the engine will safely defer the playback request internally.




**Note:**

[Reference Counting] Sounds are reference counted. If the requested filepath with the same SoundType is already loaded, this function increments a reference count and returns the already loaded sound. Because of this, one must also unload a sound the same number of times as it has been loaded in order to properly unload it from the engine.




**Parameters:**


* `sound` The handle to be populated. 
* `type` Specifies if the sound should be fully decoded into memory (Resident) or streamed directly from disk (Stream). 
* `filepath` The string path to the audio asset. 
* `callback` Optional. If provided, this function will be called when the load finishes. 
* `outRequestId` Optional. Populated with a unique ID for the async request. It will evaluate to INVALID\_REQUEST\_ID if the asset is already loaded.



**Return value:**


* Result::Ok The request was successfully queued for load or is already loaded. 
* Result::NotInitialized The engine is not initialized. 
* Result::ResidentSoundPoolExhausted The allocated capacity for resident sounds has been reached. 
* Result::StreamSoundPoolExhausted The allocated capacity for stream sounds has been reached. 




        

<hr>



### function PausePlayback 

_Pauses a playback instance._ 
```C++
Result dalia::Engine::PausePlayback (
    PlaybackHandle playback
) 
```





**Parameters:**


* `playback` The handle to the playback instance.



**Return value:**


* Result::Ok The playback instance was successfully set to pause. 
* Result::NotInitialized The engine is not initialized. 
* Result::InvalidHandle The playback handle is not recognized. 
* Result::ExpiredHandle The playback handle has already stopped playing. 




        

<hr>



### function PlayPlayback 

_Starts or resumes a playback instance._ 
```C++
Result dalia::Engine::PlayPlayback (
    PlaybackHandle playback
) 
```



If the playback instance is paused, calling this function will resume it from the current position. If it is inactive, it will play from the start.




**Note:**

[Deferred Playback] It is safe to call this function on a playback instance that is still waiting for its sound to load. The playback will start as soon as the sound has been loaded in.




**Parameters:**


* `playback` The handle to the playback instance.



**Return value:**


* Result::Ok The playback instance was successfully set to play. 
* Result::NotInitialized The engine is not initialized. 
* Result::InvalidHandle The playback handle is not recognized. 
* Result::ExpiredHandle The playback handle has already stopped playing. 
* Result::PlaybackCorrupted The internal state of the playback instance has been corrupted. 




        

<hr>



### function RouteBus 

_Modifies the routing topology by changing the output destination of an existing bus._ 
```C++
Result dalia::Engine::RouteBus (
    const char * identifier,
    const char * parentIdentifier
) 
```



Moves a bus and all of its attached children (buses and playbacks) to a new parent bus in the mixing hierarchy.




**Note:**

[Hierarchy Protection] The mixing hierarchy must always remain a strict directed acyclic graph (DAG). If an attempt is made to route a bus into itself or into one of its own descendants, this function will return an error. Additionally, the Master bus cannot be re-routed.




**Parameters:**


* `identifier` The unique string identifier of the bus. 
* `parentIdentifier` The unique string identifier of the new parent bus (must be an existing bus).



**Return value:**


* Result::Ok The bus was successfully re-routed. 
* Result::NotInitialized The engine is not initialized. 
* Result::InvalidRouting Attempted a route that would result in a cyclic bus hierarchy, or attempted to route Master. 
* Result::BusNotFound Either the bus or the parent bus does not exist. 




        

<hr>



### function RoutePlayback 

_Routes a playback instance to output its audio into the specified mixing bus._ 
```C++
Result dalia::Engine::RoutePlayback (
    PlaybackHandle playback,
    const char * busIdentifier
) 
```



This function can be called both before and during playback.




**Parameters:**


* `playback` The handle to the playback instance. 
* `busIdentifier` The unique string identifier of the target bus.



**Return value:**


* Result::Ok The voice was successfully routed. 
* Result::NotInitialized The engine is not initialized. 
* Result::InvalidHandle The playback handle is not recognized. 
* Result::ExpiredHandle The playback handle has already stopped playing. 
* Result::BusNotFound The target bus does not exist. 




        

<hr>



### function SeekPlayback 

_Seeks a playback instance to a specified time offset._ 
```C++
Result dalia::Engine::SeekPlayback (
    PlaybackHandle playback,
    double timeInSeconds
) 
```





**Parameters:**


* `playback` The handle to the playback instance. 
* `timeInSeconds` The time offset from the start of the sound (in seconds). This value is automatically clamped between 0.0 and the length of the sound.



**Return value:**


* Result::Ok The seek was successfully performed. 
* Result::NotInitialized The engine is not initialized. 
* Result::InvalidHandle The playback handle is not recognized. 
* Result::ExpiredHandle The playback handle has already stopped playing. 




        

<hr>



### function SetBusVolumeDb 

_Sets the mixing volume of a bus in decibels (dB)._ 
```C++
Result dalia::Engine::SetBusVolumeDb (
    const char * identifier,
    float volumeDb
) 
```





**Parameters:**


* `identifier` The unique string identifier of the bus. 
* `volumeDb` The new volume in decibels (dB). A value of 0.0 represents unity gain. The volume is automatically clamped to the engine's safe operating range (between -60.0dB and +12.0dB).



**Return value:**


* Result::Ok The mixing volume of the bus was successfully updated. 
* Result::NotInitialized The engine is not initialized. 
* Result::BusNotFound The bus does not exist. 




        

<hr>



### function SetEffectParams 

_Updates the parameters of an effect instance._ 
```C++
template<typename TParams>
Result dalia::Engine::SetEffectParams (
    EffectHandle effect,
    const TParams & params
) 
```





**Note:**

[Params Typing] The params type must match the params type of the effect instance that the provided handle is referencing.




**Template parameters:**


* `TParams` The parameter struct containing the effect settings. 



**Parameters:**


* `effect` The handle to the effect instance. 
* `params` The new parameters.



**Return value:**


* Result::Ok The effect parameters were successfully updated. 
* Result::NotInitialized The engine is not initialized. 
* Result::InvalidHandle The effect handle is not recognized. 
* Result::ExpiredHandle The effect handle points to an already destroyed effect. 




        

<hr>



### function SetGlobalDopplerFactor 

_Sets the global scaling factor for the Doppler effect for the entire engine._ 
```C++
Result dalia::Engine::SetGlobalDopplerFactor (
    float globalDopplerFactor
) 
```





**Parameters:**


* `globalDopplerFactor` The global scaling factor. Clamped internally between 0.0 and 10.0.



**Return value:**


* Result::Ok The global scaling factor for the Doppler effect was successfully set. 
* Result::NotInitialized The engine is not initialized. 




        

<hr>



### function SetListener3DAttributes 

_Updates the complete 3D state of a listener._ 
```C++
Result dalia::Engine::SetListener3DAttributes (
    uint32_t listenerIndex,
    const Listener3DAttributes & attributes
) 
```





**Parameters:**


* `listenerIndex` The zero-based index of the listener. 
* `attributes` The new 3D attributes.



**Return value:**


* Result::Ok The listener 3D attributes were successfully set. 
* Result::NotInitialized The engine is not initialized. 
* Result::ListenerNotFound The listener index exceeds the allocated listener capacity. 




        

<hr>



### function SetListenerActive 

_Enables or disables a listener._ 
```C++
Result dalia::Engine::SetListenerActive (
    uint32_t listenerIndex,
    bool active
) 
```





**Parameters:**


* `listenerIndex` The zero-based index of the listener. 
* `active` True to enable the listener, false to disable it.



**Return value:**


* Result::Ok The listener was successfully enabled or disabled. 
* Result::NotInitialized The engine is not initialized. 
* Result::ListenerNotFound The listener index exceeds the allocated listener capacity. 




        

<hr>



### function SetListenerDistanceProbePosition 

_Sets the distance probe world position for a listener._ 
```C++
Result dalia::Engine::SetListenerDistanceProbePosition (
    uint32_t listenerIndex,
    const Vec3 & distanceProbePosition
) 
```





**Note:**

[Usage] This position is only used for distance attenuation by playback instances with distance mode set to FromDistanceProbe.




**Parameters:**


* `listenerIndex` The zero-based index of the listener. 
* `distanceProbePosition` The new world position.



**Return value:**


* Result::Ok The listener's distance probe world position was successfully set. 
* Result::NotInitialized The engine is not initialized. 
* Result::ListenerNotFound The listener index exceeds the allocated listener capacity. 




        

<hr>



### function SetListenerOrientation 

_Sets the facing direction of a listener._ 
```C++
Result dalia::Engine::SetListenerOrientation (
    uint32_t listenerIndex,
    const Vec3 & forward,
    const Vec3 & up
) 
```





**Parameters:**


* `listenerIndex` The zero-based index of the listener. 
* `forward` The forward-facing direction vector. Normalized internally. 
* `up` The upward-facing direction vector. Normalized internally.



**Return value:**


* Result::Ok The listener orientation was successfully set. 
* Result::NotInitialized The engine is not initialized. 
* Result::ListenerNotFound The listener index exceeds the allocated listener capacity. 




        

<hr>



### function SetListenerPosition 

_Updates the world position of a listener._ 
```C++
Result dalia::Engine::SetListenerPosition (
    uint32_t listenerIndex,
    const Vec3 & position
) 
```





**Parameters:**


* `listenerIndex` The zero-based index of the listener. 
* `position` The new world position.



**Return value:**


* Result::Ok The listener world position was successfully set. 
* Result::NotInitialized The engine is not initialized. 
* Result::ListenerNotFound The listener index exceeds the allocated listener capacity. 




        

<hr>



### function SetListenerVelocity 

_Sets the velocity of a listener._ 
```C++
Result dalia::Engine::SetListenerVelocity (
    uint32_t listenerIndex,
    const Vec3 & velocity
) 
```





**Parameters:**


* `listenerIndex` The zero-based index of the listener. 
* `velocity` The velocity vector (in m/s).



**Return value:**


* Result::Ok The listener velocity was successfully set. 
* Result::NotInitialized The engine is not initialized. 
* Result::ListenerNotFound The listener index exceeds the allocated listener capacity. 




        

<hr>



### function SetPlaybackAttenuationCurve 

_Sets the curve used to evaluate volume drop-off over distance for a playback instance._ 
```C++
Result dalia::Engine::SetPlaybackAttenuationCurve (
    PlaybackHandle playback,
    AttenuationCurve curve
) 
```





**Note:**

[Spatial only] This only affects playback instances that have spatialization enabled.




**Parameters:**


* `playback` The handle to the playback instance. 
* `curve` The attenuation curve to apply.



**Return value:**


* Result::Ok The playback attenuation curve was successfully set. 
* Result::NotInitialized The engine is not initialized. 
* Result::InvalidHandle The playback handle is not recognized. 
* Result::ExpiredHandle The playback handle has already stopped playing. 




        

<hr>



### function SetPlaybackDistanceMode 

_Defines which listener position is used when calculating the distance attenuation for a playback instance._ 
```C++
Result dalia::Engine::SetPlaybackDistanceMode (
    PlaybackHandle playback,
    DistanceMode mode
) 
```





**Note:**

[Spatial only] This only affects playback instances that have spatialization enabled.




**Parameters:**


* `playback` The handle to the playback instance. 
* `mode` The distance evaluation mode to apply.



**Return value:**


* Result::Ok The playback distance mode was successfully applied. 
* Result::NotInitialized The engine is not initialized. 
* Result::InvalidHandle The playback handle is not recognized. 
* Result::ExpiredHandle The playback handle has already stopped playing. 




        

<hr>



### function SetPlaybackDopplerFactor 

_Sets the scaling factor for the Doppler effect for a playback instance._ 
```C++
Result dalia::Engine::SetPlaybackDopplerFactor (
    PlaybackHandle playback,
    float dopplerFactor
) 
```





**Note:**

[Spatial only] This only affects playback instances that have spatialization and the Doppler effect enabled.




**Parameters:**


* `playback` The handle to the playback instance. 
* `dopplerFactor` The scaling factor. Clamped internally between 0.0 and 10.0.



**Return value:**


* Result::Ok The playback Doppler effect's scaling factor was successfully set. 
* Result::NotInitialized The engine is not initialized. 
* Result::InvalidHandle The playback handle is not recognized. 
* Result::ExpiredHandle The playback handle has already stopped playing. 




        

<hr>



### function SetPlaybackListenerMask 

_Sets the listener routing mask for a playback instance._ 
```C++
Result dalia::Engine::SetPlaybackListenerMask (
    PlaybackHandle playback,
    ListenerMask mask
) 
```



The routing mask determines which listeners are allowed to hear and evaluate the playback instance.




**Note:**

[Spatial only] This only affects playback instances that have spatialization enabled.




**Parameters:**


* `playback` The handle to the playback instance. 
* `mask` The listener routing bitmask.



**Return value:**


* Result::Ok The listener routing mask for the playback instance was successfully set. 
* Result::NotInitialized The engine is not initialized. 
* Result::InvalidHandle The playback handle is not recognized. 
* Result::ExpiredHandle The playback handle has already stopped playing. 




        

<hr>



### function SetPlaybackLooping 

_Sets the looping state a playback instance._ 
```C++
Result dalia::Engine::SetPlaybackLooping (
    PlaybackHandle playback,
    bool looping
) 
```



This method updates whether a playback instance will automatically restart upon finishing.




**Parameters:**


* `playback` The handle to the playback instance. 
* `looping` The looping state.



**Return value:**


* Result::Ok The playback instance looping state was successfully set. 
* Result::NotInitialized The engine is not initialized. 
* Result::InvalidHandle The playback handle is not recognized. 
* Result::ExpiredHandle The playback handle has already stopped playing. 




        

<hr>



### function SetPlaybackMinMaxDistance 

_Defines the distance boundaries for the attenuation curve of a playback instance._ 
```C++
Result dalia::Engine::SetPlaybackMinMaxDistance (
    PlaybackHandle playback,
    float minDistance,
    float maxDistance
) 
```





**Note:**

[Spatial only] This only affects playback instances that have spatialization enabled.




**Note:**

[Safe values] The engine internally ensures that maxDistance is never less than minDistance, and that minDistance respects the engine's absolute floor limit to prevent division-by-zero.




**Parameters:**


* `playback` The handle to the playback instance. 
* `minDistance` The radius where the volume drop-off begins. 
* `maxDistance` The radius where the volume drops to silence.



**Return value:**


* Result::Ok The playback min and max distance were successfully set. 
* Result::NotInitialized The engine is not initialized. 
* Result::InvalidHandle The playback handle is not recognized. 
* Result::ExpiredHandle The playback handle has already stopped playing. 




        

<hr>



### function SetPlaybackPosition 

_Updates the 3D world position of a playback instance._ 
```C++
Result dalia::Engine::SetPlaybackPosition (
    PlaybackHandle playback,
    const Vec3 & position
) 
```





**Note:**

[Spatial only] This only affects playback instances that have spatialization enabled.




**Parameters:**


* `playback` The handle to the playback instance. 
* `position` The new 3D world coordinates.



**Return value:**


* Result::Ok The playback world position was successfully set. 
* Result::NotInitialized The engine is not initialized. 
* Result::InvalidHandle The playback handle is not recognized. 
* Result::ExpiredHandle The playback handle has already stopped playing. 




        

<hr>



### function SetPlaybackRate 

_Sets the playback rate (speed) of a playback instance._ 
```C++
Result dalia::Engine::SetPlaybackRate (
    PlaybackHandle playback,
    float rate
) 
```





**Parameters:**


* `playback` The handle to the playback instance. 
* `rate` The new playback rate. This is clamped internally between 0.1 and 4.0 to prevent performance issues.



**Return value:**


* Result::Ok The playback rate of the playback instance was successfully set. 
* Result::NotInitialized The engine is not initialized. 
* Result::InvalidHandle The playback handle is not recognized. 
* Result::ExpiredHandle The playback handle has already stopped playing. 




        

<hr>



### function SetPlaybackSpatial 

_Enables or disables 3D spatialization for a playback instance._ 
```C++
Result dalia::Engine::SetPlaybackSpatial (
    PlaybackHandle playback,
    bool spatial
) 
```





**Parameters:**


* `playback` The handle to the playback instance. 
* `spatial` True to enable 3D spatialization, false for 2D/global playback.



**Return value:**


* Result::Ok The playback spatialization was successfully enabled or disabled. 
* Result::NotInitialized The engine is not initialized. 
* Result::InvalidHandle The playback handle is not recognized. 
* Result::ExpiredHandle The playback handle has already stopped playing. 




        

<hr>



### function SetPlaybackStereoPan 

_Sets the stereo pan of a playback instance._ 
```C++
Result dalia::Engine::SetPlaybackStereoPan (
    PlaybackHandle playback,
    float pan
) 
```





**Note:**

[Spatial] This only affects playback instances that have spatialization disabled.




**Parameters:**


* `playback` The handle to the playback instance. 
* `pan` The normalized pan position. A value of 0.0 represents an equal distribution between L and R, -1.0 is full L and 1.0 is full R.



**Return value:**


* Result::Ok The pan of the playback instance was successfully set. 
* Result::NotInitialized The engine is not initialized. 
* Result::InvalidHandle The playback handle is not recognized. 
* Result::ExpiredHandle The playback handle has already stopped playing. 




        

<hr>



### function SetPlaybackUseDoppler 

_Enables or disables the Doppler effect for a playback instance._ 
```C++
Result dalia::Engine::SetPlaybackUseDoppler (
    PlaybackHandle playback,
    bool useDoppler
) 
```





**Note:**

[Spatial only] This only affects playback instances that have spatialization enabled.




**Note:**

[Requirements] In order for the doppler effect to be calculated, both listener and playback velocity must be supplied to the engine every frame.




**Parameters:**


* `playback` The handle to the playback instance. 
* `useDoppler` True to enable Doppler frequency shifts, false to disable it.



**Return value:**


* Result::Ok The playback Doppler effect was successfully enabled or disabled. 
* Result::NotInitialized The engine is not initialized. 
* Result::InvalidHandle The playback handle is not recognized. 
* Result::ExpiredHandle The playback handle has already stopped playing. 




        

<hr>



### function SetPlaybackVelocity 

_Sets the velocity of a playback instance._ 
```C++
Result dalia::Engine::SetPlaybackVelocity (
    PlaybackHandle playback,
    const Vec3 & velocity
) 
```



Velocity is used exclusively to calculate the Doppler effect. If the Effect is disabled, this value is ignored.




**Note:**

[Spatial only] This only affects playback instances that have spatialization enabled.




**Parameters:**


* `playback` The handle to the playback instance. 
* `velocity` The velocity vector (in m/s).



**Return value:**


* Result::Ok The playback velocity was successfully set. 
* Result::NotInitialized The engine is not initialized. 
* Result::InvalidHandle The playback handle is not recognized. 
* Result::ExpiredHandle The playback handle has already stopped playing. 




        

<hr>



### function SetPlaybackVolumeDb 

_Sets the mixing volume of a playback instance in decibels (dB)._ 
```C++
Result dalia::Engine::SetPlaybackVolumeDb (
    PlaybackHandle playback,
    float volumeDb
) 
```





**Parameters:**


* `playback` The handle to the playback instance. 
* `volumeDb` The new volume in decibels (dB). A value of 0.0 represents unity gain. The volume is automatically clamped to the engine's safe operating range (between -60.0dB and +12.0dB).



**Return value:**


* Result::Ok The mixing volume of the playback instance was successfully set. 
* Result::NotInitialized The engine is not initialized. 
* Result::InvalidHandle The playback handle is not recognized. 
* Result::ExpiredHandle The playback handle has already stopped playing. 




        

<hr>



### function Shutdown 

_Halts all audio processing, terminates all background threads and frees allocated engine memory._ 
```C++
Result dalia::Engine::Shutdown () 
```





**Note:**

[Blocking Operation] This method will block the calling thread until all background threads have fully joined.




**Note:**

[Handle Invalidation] Calling this method will invalidate all handles.




**Return value:**


* Result::Ok The engine was successfully shut down. 
* Result::NotInitialized Attempted to shut down an engine that is not initialized. 
* Result::DeviceFailed The operating system failed to release the hardware device. 




        

<hr>



### function StopPlayback 

_Stops a playback instance._ 
```C++
Result dalia::Engine::StopPlayback (
    PlaybackHandle playback
) 
```



If a callback function was provided when creating the playback instance, it will be called with the exit condition ExplicitStop once the playback has been stopped.




**Note:**

[Playback Lifecycle] Successfully calling this function invalidates the provided [**PlaybackHandle**](structdalia_1_1PlaybackHandle.md). A stopped playback cannot be restarted.




**Parameters:**


* `playback` The handle to the playback instance.



**Return value:**


* Result::Ok The playback instance was successfully set to stop. 
* Result::NotInitialized The engine is not initialized. 
* Result::InvalidHandle The playback handle is not recognized. 
* Result::ExpiredHandle The playback handle has already stopped playing. 




        

<hr>



### function UnloadSound 

_Decrements the reference count of a sound and unloads it if the count reaches zero._ 
```C++
Result dalia::Engine::UnloadSound (
    SoundHandle sound
) 
```



If the reference count of a sound reaches zero, all active playbacks using the sound will be stopped with the exit condition ExplicitStop. The engine will also immediately abort all pending playback requests with the same exit condition.




**Parameters:**


* `sound` The handle to the sound asset.



**Return value:**


* Result::Ok The sound was successfully unloaded or its reference count was decremented. 
* Result::NotInitialized The engine is not initialized. 
* Result::InvalidHandle The sound handle is not recognized, or has already been freed. 




        

<hr>



### function Update 

_The per-frame tick method for the engine._ 
```C++
void dalia::Engine::Update () 
```



This method is the primary synchronization router between the API (calling) thread and the background threads. It must be called exactly once per frame inside the update loop of your main thread.




**Note:**

[Callback Execution] This method processes all events happening within the engine. It is within this method that any provided AudioEventCallback and AssetLoadCallback functions are invoked. 





        

<hr>



### function operator= 

```C++
Engine & dalia::Engine::operator= (
    const Engine &
) = delete
```




<hr>



### function ~Engine 

```C++
dalia::Engine::~Engine () 
```




<hr>

------------------------------
The documentation for this class was generated from the following file `engine/include/dalia/audio/Engine.h`

