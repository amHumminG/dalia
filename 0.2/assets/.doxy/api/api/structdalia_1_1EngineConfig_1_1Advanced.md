

# Struct dalia::EngineConfig::Advanced



[**ClassList**](annotated.md) **>** [**dalia**](namespacedalia.md) **>** [**EngineConfig**](structdalia_1_1EngineConfig.md) **>** [**Advanced**](structdalia_1_1EngineConfig_1_1Advanced.md)



[_**Advanced**_](structdalia_1_1EngineConfig_1_1Advanced.md) _settings. Only touch if you have very tight constraints._

* `#include <Engine.hpp>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  size\_t | [**controlQueueCapacity**](#variable-controlqueuecapacity)   = `32`<br>_Control-thread messaging queue capacities (must be power of 2)._  |
|  size\_t | [**loadQueueCapacity**](#variable-loadqueuecapacity)   = `128`<br>_Load-thread messaging queue capacities (must be power of 2)._  |
|  size\_t | [**mixerQueueCapacity**](#variable-mixerqueuecapacity)   = `1024`<br>_Mixer-thread messaging queue capacities (must be power of 2)._  |
|  uint32\_t | [**periodSizeFrames**](#variable-periodsizeframes)   = `512`<br>_The number of frames per audio period._  |
|  size\_t | [**streamQueueCapacity**](#variable-streamqueuecapacity)   = `256`<br>_Stream-thread messaging queue capacity (must be power of 2)._  |












































## Public Attributes Documentation




### variable controlQueueCapacity 

_Control-thread messaging queue capacities (must be power of 2)._ 
```C++
size_t dalia::EngineConfig::Advanced::controlQueueCapacity;
```




<hr>



### variable loadQueueCapacity 

_Load-thread messaging queue capacities (must be power of 2)._ 
```C++
size_t dalia::EngineConfig::Advanced::loadQueueCapacity;
```




<hr>



### variable mixerQueueCapacity 

_Mixer-thread messaging queue capacities (must be power of 2)._ 
```C++
size_t dalia::EngineConfig::Advanced::mixerQueueCapacity;
```




<hr>



### variable periodSizeFrames 

_The number of frames per audio period._ 
```C++
uint32_t dalia::EngineConfig::Advanced::periodSizeFrames;
```




<hr>



### variable streamQueueCapacity 

_Stream-thread messaging queue capacity (must be power of 2)._ 
```C++
size_t dalia::EngineConfig::Advanced::streamQueueCapacity;
```




<hr>

------------------------------
The documentation for this class was generated from the following file `dalia/Engine.hpp`

