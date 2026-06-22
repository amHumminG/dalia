

# Namespace dalia



[**Namespace List**](namespaces.md) **>** [**dalia**](namespacedalia.md)




















## Classes

| Type | Name |
| ---: | :--- |
| struct | [**BiquadParams**](structdalia_1_1BiquadParams.md) <br> |
| struct | [**EffectHandle**](structdalia_1_1EffectHandle.md) <br>_Handle used to manage effect instances. This handle expires once the effect it is referencing has been destroyed._  |
| class | [**Engine**](classdalia_1_1Engine.md) <br> |
| struct | [**EngineConfig**](structdalia_1_1EngineConfig.md) <br> |
| struct | [**Listener3DAttributes**](structdalia_1_1Listener3DAttributes.md) <br>_A snapshot of a listener's 3D state in the world._  |
| struct | [**PlaybackHandle**](structdalia_1_1PlaybackHandle.md) <br>_Handle used to manage playback instances. This handle will expire once the playback instance it is referencing stops._  |
| struct | [**SoundHandle**](structdalia_1_1SoundHandle.md) <br>_Handle used to manage loaded sounds. This handle expires once the sound it is referencing is unloaded._  |
| struct | [**Vec3**](structdalia_1_1Vec3.md) <br>_Standard 3D vector for the DALIA API._  |


## Public Types

| Type | Name |
| ---: | :--- |
| typedef std::function&lt; void(uint32\_t requestId, [**Result**](namespacedalia.md#enum-result) result)&gt; | [**AssetLoadCallback**](#typedef-assetloadcallback)  <br>_A function that, if provided when loading an asset, will be called once the asset has been successfully loaded._  |
| enum uint8\_t | [**AttenuationCurve**](#enum-attenuationcurve)  <br>_Curve used to calculate distance attenuation._  |
| enum uint8\_t | [**CoordinateSystem**](#enum-coordinatesystem)  <br> |
| enum uint8\_t | [**DistanceMode**](#enum-distancemode)  <br>_Mode used to determine what listener position to use when calculating distance attenuation._  |
| enum uint8\_t | [**EffectType**](#enum-effecttype)  <br> |
| typedef uint32\_t | [**ListenerMask**](#typedef-listenermask)  <br>_Routing mask used to target a specific listener. Can be combined to target multiple listeners using bitwise OR._  |
| typedef std::function&lt; void([**LogLevel**](namespacedalia.md#enum-loglevel) level, const char \*context, const char \*message)&gt; | [**LogCallback**](#typedef-logcallback)  <br> |
| enum int | [**LogLevel**](#enum-loglevel)  <br> |
| typedef std::function&lt; void([**PlaybackHandle**](structdalia_1_1PlaybackHandle.md) playback, [**PlaybackExitCondition**](namespacedalia.md#enum-playbackexitcondition) exitCondition)&gt; | [**PlaybackExitCallback**](#typedef-playbackexitcallback)  <br>_A function that, if provided when creating a playback instance, will be called when the playback instance is stopped._  |
| enum uint8\_t | [**PlaybackExitCondition**](#enum-playbackexitcondition)  <br>_The condition under which a playback instance was stopped._  |
| enum int | [**Result**](#enum-result)  <br> |
| enum uint8\_t | [**SoundType**](#enum-soundtype)  <br> |




## Public Attributes

| Type | Name |
| ---: | :--- |
|  constexpr uint32\_t | [**INVALID\_REQUEST\_ID**](#variable-invalid_request_id)   = `0`<br> |
|  constexpr [**EffectHandle**](structdalia_1_1EffectHandle.md) | [**InvalidEffectHandle**](#variable-invalideffecthandle)   = `{}`<br> |
|  constexpr [**PlaybackHandle**](structdalia_1_1PlaybackHandle.md) | [**InvalidPlaybackHandle**](#variable-invalidplaybackhandle)   = `{}`<br> |
|  constexpr [**ListenerMask**](namespacedalia.md#typedef-listenermask) | [**MASK\_ALL\_LISTENERS**](#variable-mask_all_listeners)   = `0xFFFFFFFF`<br> |
|  constexpr [**ListenerMask**](namespacedalia.md#typedef-listenermask) | [**MASK\_LISTENER\_0**](#variable-mask_listener_0)   = `0b00000001`<br> |
|  constexpr [**ListenerMask**](namespacedalia.md#typedef-listenermask) | [**MASK\_LISTENER\_1**](#variable-mask_listener_1)   = `0b00000010`<br> |
|  constexpr [**ListenerMask**](namespacedalia.md#typedef-listenermask) | [**MASK\_LISTENER\_2**](#variable-mask_listener_2)   = `0b00000100`<br> |
|  constexpr [**ListenerMask**](namespacedalia.md#typedef-listenermask) | [**MASK\_LISTENER\_3**](#variable-mask_listener_3)   = `0b00001000`<br> |
|  constexpr [**ListenerMask**](namespacedalia.md#typedef-listenermask) | [**MASK\_NONE**](#variable-mask_none)   = `0b00000000`<br> |
















## Public Functions

| Type | Name |
| ---: | :--- |
|  constexpr const char \* | [**GetResultString**](#function-getresultstring) (const [**Result**](namespacedalia.md#enum-result) result) <br> |
|  constexpr [**Listener3DAttributes**](structdalia_1_1Listener3DAttributes.md) | [**MakeListener3DAttributes**](#function-makelistener3dattributes) (const [**Vec3**](structdalia_1_1Vec3.md) & position, const [**Vec3**](structdalia_1_1Vec3.md) & forward, const [**Vec3**](structdalia_1_1Vec3.md) & up, const [**Vec3**](structdalia_1_1Vec3.md) & velocity) <br>_Constructs a listener transform for a listener that performs panning and attenuation logic from the same world position._  |
|  constexpr [**Listener3DAttributes**](structdalia_1_1Listener3DAttributes.md) | [**MakeListener3DAttributesSplit**](#function-makelistener3dattributessplit) (const [**Vec3**](structdalia_1_1Vec3.md) & position, const [**Vec3**](structdalia_1_1Vec3.md) & probePosition, const [**Vec3**](structdalia_1_1Vec3.md) & forward, const [**Vec3**](structdalia_1_1Vec3.md) & up, const [**Vec3**](structdalia_1_1Vec3.md) & velocity) <br>_Constructs a listener transform for a listener that performs panning and attenuation logic from two separate positions._  |
|  constexpr [**ListenerMask**](namespacedalia.md#typedef-listenermask) | [**MakeListenerMask**](#function-makelistenermask) (uint32\_t listenerIndex) <br>_Creates a routing mask targeting a specific listener._  |




























## Public Types Documentation




### typedef AssetLoadCallback 

_A function that, if provided when loading an asset, will be called once the asset has been successfully loaded._ 
```C++
using dalia::AssetLoadCallback = typedef std::function<void(uint32_t requestId, Result result)>;
```




<hr>



### enum AttenuationCurve 

_Curve used to calculate distance attenuation._ 
```C++
enum dalia::AttenuationCurve {
    InverseSquare,
    Linear,
    Quadratic
};
```




<hr>



### enum CoordinateSystem 

```C++
enum dalia::CoordinateSystem {
    RightHanded,
    LeftHanded
};
```




<hr>



### enum DistanceMode 

_Mode used to determine what listener position to use when calculating distance attenuation._ 
```C++
enum dalia::DistanceMode {
    FromListener,
    FromDistanceProbe
};
```




<hr>



### enum EffectType 

```C++
enum dalia::EffectType {
    None,
    Biquad
};
```




<hr>



### typedef ListenerMask 

_Routing mask used to target a specific listener. Can be combined to target multiple listeners using bitwise OR._ 
```C++
using dalia::ListenerMask = typedef uint32_t;
```




<hr>



### typedef LogCallback 

```C++
using dalia::LogCallback = typedef std::function<void(LogLevel level, const char* context, const char* message)>;
```




<hr>



### enum LogLevel 

```C++
enum dalia::LogLevel {
    Debug = 0,
    Info = 1,
    Warning = 2,
    Error = 3,
    Critical = 4,
    None = 5
};
```




<hr>



### typedef PlaybackExitCallback 

_A function that, if provided when creating a playback instance, will be called when the playback instance is stopped._ 
```C++
using dalia::PlaybackExitCallback = typedef std::function<void(PlaybackHandle playback, PlaybackExitCondition exitCondition)>;
```




<hr>



### enum PlaybackExitCondition 

_The condition under which a playback instance was stopped._ 
```C++
enum dalia::PlaybackExitCondition {
    None = 0,
    NaturalEnd = 1,
    ExplicitStop = 2,
    Evicted = 3,
    Error = 4
};
```




<hr>



### enum Result 

```C++
enum dalia::Result {
    Ok = 0,
    Error = -1,
    NotInitialized = -2,
    AlreadyInitialized = -3,
    InvalidHandle = -4,
    ExpiredHandle = -5,
    BusNotFound = -6,
    InvalidRouting = -7,
    InvalidEffectSlot = -8,
    ListenerNotFound = -9,
    PoolExhausted = -100,
    ResidentSoundPoolExhausted = -101,
    StreamSoundPoolExhausted = -102,
    VoicePoolExhausted = -103,
    StreamPoolExhausted = -104,
    BusPoolExhausted = -105,
    EffectPoolExhausted = -106,
    RtCommandQueueFull = -200,
    RtEventQueueFull = -201,
    IoStreamRequestQueueFull = -202,
    IoLoadRequestQueueFull = -203,
    PlaybackCorrupted = -303,
    SoundLoadError = -400,
    FileReadError = -401,
    UnsupportedFormat = -402,
    SystemError = -500,
    DeviceNotFound = -501,
    DeviceFailed = -502,
    ClientFailed = -503
};
```




<hr>



### enum SoundType 

```C++
enum dalia::SoundType {
    None = 0,
    Resident = 1,
    Stream = 2
};
```




<hr>
## Public Attributes Documentation




### variable INVALID\_REQUEST\_ID 

```C++
constexpr uint32_t dalia::INVALID_REQUEST_ID;
```




<hr>



### variable InvalidEffectHandle 

```C++
constexpr EffectHandle dalia::InvalidEffectHandle;
```




<hr>



### variable InvalidPlaybackHandle 

```C++
constexpr PlaybackHandle dalia::InvalidPlaybackHandle;
```




<hr>



### variable MASK\_ALL\_LISTENERS 

```C++
constexpr ListenerMask dalia::MASK_ALL_LISTENERS;
```




<hr>



### variable MASK\_LISTENER\_0 

```C++
constexpr ListenerMask dalia::MASK_LISTENER_0;
```




<hr>



### variable MASK\_LISTENER\_1 

```C++
constexpr ListenerMask dalia::MASK_LISTENER_1;
```




<hr>



### variable MASK\_LISTENER\_2 

```C++
constexpr ListenerMask dalia::MASK_LISTENER_2;
```




<hr>



### variable MASK\_LISTENER\_3 

```C++
constexpr ListenerMask dalia::MASK_LISTENER_3;
```




<hr>



### variable MASK\_NONE 

```C++
constexpr ListenerMask dalia::MASK_NONE;
```




<hr>
## Public Functions Documentation




### function GetResultString 

```C++
constexpr const char * dalia::GetResultString (
    const Result result
) 
```




<hr>



### function MakeListener3DAttributes 

_Constructs a listener transform for a listener that performs panning and attenuation logic from the same world position._ 
```C++
inline constexpr Listener3DAttributes dalia::MakeListener3DAttributes (
    const Vec3 & position,
    const Vec3 & forward,
    const Vec3 & up,
    const Vec3 & velocity
) 
```




<hr>



### function MakeListener3DAttributesSplit 

_Constructs a listener transform for a listener that performs panning and attenuation logic from two separate positions._ 
```C++
inline constexpr Listener3DAttributes dalia::MakeListener3DAttributesSplit (
    const Vec3 & position,
    const Vec3 & probePosition,
    const Vec3 & forward,
    const Vec3 & up,
    const Vec3 & velocity
) 
```




<hr>



### function MakeListenerMask 

_Creates a routing mask targeting a specific listener._ 
```C++
inline constexpr ListenerMask dalia::MakeListenerMask (
    uint32_t listenerIndex
) 
```



@Note[Combining masks] Multiple masks can be combined using bitwise OR.




**Parameters:**


* `listenerIndex` The zero-based index of the listener to target.



**Returns:**

A bitmask with the corresponding listener's bit enabled. 





        

<hr>

------------------------------
The documentation for this class was generated from the following file `engine/include/dalia/audio/EffectControl.h`

