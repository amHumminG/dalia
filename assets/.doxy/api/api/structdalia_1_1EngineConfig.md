

# Struct dalia::EngineConfig



[**ClassList**](annotated.md) **>** [**dalia**](namespacedalia.md) **>** [**EngineConfig**](structdalia_1_1EngineConfig.md)



_Configuration containing all user-exposed engine settings._ 

* `#include <Engine.h>`















## Classes

| Type | Name |
| ---: | :--- |
| struct | [**Advanced**](structdalia_1_1EngineConfig_1_1Advanced.md) <br>[_**Advanced**_](structdalia_1_1EngineConfig_1_1Advanced.md) _settings. Only touch if you have very tight constraints or experience queue overflows._ |






## Public Attributes

| Type | Name |
| ---: | :--- |
|  uint32\_t | [**BiquadCapacity**](#variable-biquadcapacity)   = `32`<br> |
|  struct [**dalia::EngineConfig::Advanced**](structdalia_1_1EngineConfig_1_1Advanced.md) | [**advanced**](#variable-advanced)  <br> |
|  uint32\_t | [**busCapacity**](#variable-buscapacity)   = `64`<br> |
|  [**CoordinateSystem**](namespacedalia.md#enum-coordinatesystem) | [**coordinateSystem**](#variable-coordinatesystem)   = `CoordinateSystem::RightHanded`<br> |
|  uint32\_t | [**listenerCapacity**](#variable-listenercapacity)   = `1`<br> |
|  [**LogCallback**](namespacedalia.md#typedef-logcallback) | [**logCallback**](#variable-logcallback)   = `nullptr`<br> |
|  [**LogLevel**](namespacedalia.md#enum-loglevel) | [**logLevel**](#variable-loglevel)   = `LogLevel::Warning`<br> |
|  uint32\_t | [**residentSoundCapacity**](#variable-residentsoundcapacity)   = `256`<br> |
|  uint32\_t | [**sampleRate**](#variable-samplerate)   = `48000`<br> |
|  uint32\_t | [**streamCapacity**](#variable-streamcapacity)   = `32`<br> |
|  uint32\_t | [**streamSoundCapacity**](#variable-streamsoundcapacity)   = `256`<br> |
|  uint32\_t | [**voiceCapacity**](#variable-voicecapacity)   = `128`<br> |












































## Public Attributes Documentation




### variable BiquadCapacity 

```C++
uint32_t dalia::EngineConfig::BiquadCapacity;
```




<hr>



### variable advanced 

```C++
struct dalia::EngineConfig::Advanced dalia::EngineConfig::advanced;
```




<hr>



### variable busCapacity 

```C++
uint32_t dalia::EngineConfig::busCapacity;
```




<hr>



### variable coordinateSystem 

```C++
CoordinateSystem dalia::EngineConfig::coordinateSystem;
```




<hr>



### variable listenerCapacity 

```C++
uint32_t dalia::EngineConfig::listenerCapacity;
```




<hr>



### variable logCallback 

```C++
LogCallback dalia::EngineConfig::logCallback;
```




<hr>



### variable logLevel 

```C++
LogLevel dalia::EngineConfig::logLevel;
```




<hr>



### variable residentSoundCapacity 

```C++
uint32_t dalia::EngineConfig::residentSoundCapacity;
```




<hr>



### variable sampleRate 

```C++
uint32_t dalia::EngineConfig::sampleRate;
```




<hr>



### variable streamCapacity 

```C++
uint32_t dalia::EngineConfig::streamCapacity;
```




<hr>



### variable streamSoundCapacity 

```C++
uint32_t dalia::EngineConfig::streamSoundCapacity;
```




<hr>



### variable voiceCapacity 

```C++
uint32_t dalia::EngineConfig::voiceCapacity;
```




<hr>

------------------------------
The documentation for this class was generated from the following file `engine/include/dalia/audio/Engine.h`

