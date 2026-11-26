

# Struct dalia::FdnReverbParams



[**ClassList**](annotated.md) **>** [**dalia**](namespacedalia.md) **>** [**FdnReverbParams**](structdalia_1_1FdnReverbParams.md)



_Configuration parameters for a feedback delay network (FDN) reverb._ 

* `#include <Effect.hpp>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  float | [**dampingHz**](#variable-dampinghz)   = `2500.0f`<br>_High-frequency damping lowpass cutoff: 500 - 20000 Hz._  |
|  float | [**decayTimeSeconds**](#variable-decaytimeseconds)   = `4.0f`<br>_Time for reverb tail to decay by 60 dB (T60): 0.1 - 15.0 s._  |
|  float | [**dryMix**](#variable-drymix)   = `1.0f`<br>_Linear gain of the unprocessed input signal: 0.0 - 1.0._  |
|  bool | [**freeze**](#variable-freeze)   = `false`<br>_Sustains the current tail infinitely without new input: true/false._  |
|  float | [**lowCutHz**](#variable-lowcuthz)   = `80.0f`<br>_Sub-bass buildup low-shelf cutoff: 20 - 500 Hz._  |
|  float | [**modulation**](#variable-modulation)   = `0.5f`<br>_Tank delay LFO modulation depth: 0.0 - 1.0._  |
|  float | [**preDelayMs**](#variable-predelayms)   = `20.0f`<br>_Delay before reflections begin: 0 - 100 ms._  |
|  float | [**roomSize**](#variable-roomsize)   = `0.6f`<br>_Virtual space scale controlling echo spacing: 0.25 - 1.0._  |
|  float | [**wetMix**](#variable-wetmix)   = `0.35f`<br>_Linear gain of the processed reverb signal: 0.0 - 1.0._  |
















## Public Functions

| Type | Name |
| ---: | :--- |
|  void | [**Sanitize**](#function-sanitize) () <br>_Clamps internal parameters to safe ranges._  |




























## Public Attributes Documentation




### variable dampingHz 

_High-frequency damping lowpass cutoff: 500 - 20000 Hz._ 
```C++
float dalia::FdnReverbParams::dampingHz;
```




<hr>



### variable decayTimeSeconds 

_Time for reverb tail to decay by 60 dB (T60): 0.1 - 15.0 s._ 
```C++
float dalia::FdnReverbParams::decayTimeSeconds;
```




<hr>



### variable dryMix 

_Linear gain of the unprocessed input signal: 0.0 - 1.0._ 
```C++
float dalia::FdnReverbParams::dryMix;
```




<hr>



### variable freeze 

_Sustains the current tail infinitely without new input: true/false._ 
```C++
bool dalia::FdnReverbParams::freeze;
```




<hr>



### variable lowCutHz 

_Sub-bass buildup low-shelf cutoff: 20 - 500 Hz._ 
```C++
float dalia::FdnReverbParams::lowCutHz;
```




<hr>



### variable modulation 

_Tank delay LFO modulation depth: 0.0 - 1.0._ 
```C++
float dalia::FdnReverbParams::modulation;
```




<hr>



### variable preDelayMs 

_Delay before reflections begin: 0 - 100 ms._ 
```C++
float dalia::FdnReverbParams::preDelayMs;
```




<hr>



### variable roomSize 

_Virtual space scale controlling echo spacing: 0.25 - 1.0._ 
```C++
float dalia::FdnReverbParams::roomSize;
```




<hr>



### variable wetMix 

_Linear gain of the processed reverb signal: 0.0 - 1.0._ 
```C++
float dalia::FdnReverbParams::wetMix;
```




<hr>
## Public Functions Documentation




### function Sanitize 

_Clamps internal parameters to safe ranges._ 
```C++
void dalia::FdnReverbParams::Sanitize () 
```




<hr>

------------------------------
The documentation for this class was generated from the following file `dalia/Effect.hpp`

