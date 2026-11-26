

# Struct dalia::FdnReverbParams



[**ClassList**](annotated.md) **>** [**dalia**](namespacedalia.md) **>** [**FdnReverbParams**](structdalia_1_1FdnReverbParams.md)



_Configuration parameters for a feedback delay network (FDN) reverb._ 

* `#include <Effect.hpp>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  float | [**dampingHz**](#variable-dampinghz)   = `2500.0f`<br> |
|  float | [**decayTimeSeconds**](#variable-decaytimeseconds)   = `4.0f`<br> |
|  float | [**dryMix**](#variable-drymix)   = `1.0f`<br> |
|  bool | [**freeze**](#variable-freeze)   = `false`<br> |
|  float | [**lowCutHz**](#variable-lowcuthz)   = `80.0f`<br> |
|  float | [**modulation**](#variable-modulation)   = `0.35f`<br> |
|  float | [**preDelayMs**](#variable-predelayms)   = `20.0f`<br> |
|  float | [**roomSize**](#variable-roomsize)   = `0.6f`<br> |
|  float | [**wetMix**](#variable-wetmix)   = `0.35f`<br> |
















## Public Functions

| Type | Name |
| ---: | :--- |
|  void | [**Sanitize**](#function-sanitize) () <br>_Clamps internal parameters to safe ranges._  |




























## Public Attributes Documentation




### variable dampingHz 

```C++
float dalia::FdnReverbParams::dampingHz;
```




<hr>



### variable decayTimeSeconds 

```C++
float dalia::FdnReverbParams::decayTimeSeconds;
```




<hr>



### variable dryMix 

```C++
float dalia::FdnReverbParams::dryMix;
```




<hr>



### variable freeze 

```C++
bool dalia::FdnReverbParams::freeze;
```




<hr>



### variable lowCutHz 

```C++
float dalia::FdnReverbParams::lowCutHz;
```




<hr>



### variable modulation 

```C++
float dalia::FdnReverbParams::modulation;
```




<hr>



### variable preDelayMs 

```C++
float dalia::FdnReverbParams::preDelayMs;
```




<hr>



### variable roomSize 

```C++
float dalia::FdnReverbParams::roomSize;
```




<hr>



### variable wetMix 

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

