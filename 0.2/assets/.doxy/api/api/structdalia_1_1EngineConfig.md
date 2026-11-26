

# Struct dalia::EngineConfig



[**ClassList**](annotated.md) **>** [**dalia**](namespacedalia.md) **>** [**EngineConfig**](structdalia_1_1EngineConfig.md)



_Configuration containing all user-exposed engine settings._ 

* `#include <Engine.hpp>`















## Classes

| Type | Name |
| ---: | :--- |
| struct | [**Advanced**](structdalia_1_1EngineConfig_1_1Advanced.md) <br>[_**Advanced**_](structdalia_1_1EngineConfig_1_1Advanced.md) _settings. Only touch if you have very tight constraints._ |






## Public Attributes

| Type | Name |
| ---: | :--- |
|  struct [**dalia::EngineConfig::Advanced**](structdalia_1_1EngineConfig_1_1Advanced.md) | [**advanced**](#variable-advanced)  <br> |
|  uint32\_t | [**biquadCapacity**](#variable-biquadcapacity)   = `32`<br>_The maximum number of biquad filters that can exist at once._  |
|  uint32\_t | [**busCapacity**](#variable-buscapacity)   = `64`<br>_The maximum number of buses that can exist at once._  |
|  [**CoordinateSystem**](namespacedalia.md#enum-coordinatesystem) | [**coordinateSystem**](#variable-coordinatesystem)   = `CoordinateSystem::RightHanded`<br>_The coordinate system used for spatial audio._  |
|  uint32\_t | [**fdnReverbCapacity**](#variable-fdnreverbcapacity)   = `32`<br>_The maximum number of FDN reverb's that can exist at once._  |
|  uint32\_t | [**listenerCapacity**](#variable-listenercapacity)   = `1`<br>_The maximum number of listeners that can exist at once (min=1 max=4)._  |
|  [**LogCallback**](namespacedalia.md#typedef-logcallback) | [**logCallback**](#variable-logcallback)   = `nullptr`<br>_Optional log sink._  |
|  [**LogLevel**](namespacedalia.md#enum-loglevel) | [**logLevel**](#variable-loglevel)   = `LogLevel::Warning`<br>_The level at which the engine will log messages._  |
|  uint32\_t | [**residentSoundCapacity**](#variable-residentsoundcapacity)   = `256`<br>_The number of resident sounds the engine is able to hold loaded at once._  |
|  uint32\_t | [**sampleRate**](#variable-samplerate)   = `48000`<br>_Internal mixing sample rate._  |
|  const char \* | [**startupOutputDeviceId**](#variable-startupoutputdeviceid)   = `"default"`<br>_Output device that should attempt to start on initialization._  |
|  uint32\_t | [**streamCapacity**](#variable-streamcapacity)   = `64`<br>_The maximum number of playback instances playing stream sounds at once._  |
|  uint32\_t | [**streamSoundCapacity**](#variable-streamsoundcapacity)   = `256`<br>_The number of stream sounds the engine is able to hold loaded at once._  |
|  uint32\_t | [**voiceCapacity**](#variable-voicecapacity)   = `128`<br>_The maximum number of playback instances that can exist at once._  |












































## Public Attributes Documentation




### variable advanced 

```C++
struct dalia::EngineConfig::Advanced dalia::EngineConfig::advanced;
```




<hr>



### variable biquadCapacity 

_The maximum number of biquad filters that can exist at once._ 
```C++
uint32_t dalia::EngineConfig::biquadCapacity;
```




<hr>



### variable busCapacity 

_The maximum number of buses that can exist at once._ 
```C++
uint32_t dalia::EngineConfig::busCapacity;
```




<hr>



### variable coordinateSystem 

_The coordinate system used for spatial audio._ 
```C++
CoordinateSystem dalia::EngineConfig::coordinateSystem;
```




<hr>



### variable fdnReverbCapacity 

_The maximum number of FDN reverb's that can exist at once._ 
```C++
uint32_t dalia::EngineConfig::fdnReverbCapacity;
```




<hr>



### variable listenerCapacity 

_The maximum number of listeners that can exist at once (min=1 max=4)._ 
```C++
uint32_t dalia::EngineConfig::listenerCapacity;
```




<hr>



### variable logCallback 

_Optional log sink._ 
```C++
LogCallback dalia::EngineConfig::logCallback;
```




<hr>



### variable logLevel 

_The level at which the engine will log messages._ 
```C++
LogLevel dalia::EngineConfig::logLevel;
```




<hr>



### variable residentSoundCapacity 

_The number of resident sounds the engine is able to hold loaded at once._ 
```C++
uint32_t dalia::EngineConfig::residentSoundCapacity;
```




<hr>



### variable sampleRate 

_Internal mixing sample rate._ 
```C++
uint32_t dalia::EngineConfig::sampleRate;
```




<hr>



### variable startupOutputDeviceId 

_Output device that should attempt to start on initialization._ 
```C++
const char* dalia::EngineConfig::startupOutputDeviceId;
```




<hr>



### variable streamCapacity 

_The maximum number of playback instances playing stream sounds at once._ 
```C++
uint32_t dalia::EngineConfig::streamCapacity;
```




<hr>



### variable streamSoundCapacity 

_The number of stream sounds the engine is able to hold loaded at once._ 
```C++
uint32_t dalia::EngineConfig::streamSoundCapacity;
```




<hr>



### variable voiceCapacity 

_The maximum number of playback instances that can exist at once._ 
```C++
uint32_t dalia::EngineConfig::voiceCapacity;
```




<hr>

------------------------------
The documentation for this class was generated from the following file `dalia/Engine.hpp`

