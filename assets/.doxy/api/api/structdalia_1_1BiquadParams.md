

# Struct dalia::BiquadParams



[**ClassList**](annotated.md) **>** [**dalia**](namespacedalia.md) **>** [**BiquadParams**](structdalia_1_1BiquadParams.md)





* `#include <EffectControl.h>`

















## Public Types

| Type | Name |
| ---: | :--- |
| enum uint8\_t | [**Type**](#enum-type)  <br>_Defines the frequency response shape of a standard biquadratic filter._  |




## Public Attributes

| Type | Name |
| ---: | :--- |
|  float | [**frequency**](#variable-frequency)   = `20000.0f`<br> |
|  float | [**resonance**](#variable-resonance)   = `0.707f`<br> |
|  enum [**dalia::BiquadParams::Type**](structdalia_1_1BiquadParams.md#enum-type) | [**type**](#variable-type)   = `Type::LowPass`<br> |
















## Public Functions

| Type | Name |
| ---: | :--- |
|  void | [**Sanitize**](#function-sanitize) () <br> |




























## Public Types Documentation




### enum Type 

_Defines the frequency response shape of a standard biquadratic filter._ 
```C++
enum dalia::BiquadParams::Type {
    LowPass,
    HighPass,
    BandPass
};
```




<hr>
## Public Attributes Documentation




### variable frequency 

```C++
float dalia::BiquadParams::frequency;
```




<hr>



### variable resonance 

```C++
float dalia::BiquadParams::resonance;
```




<hr>



### variable type 

```C++
enum dalia::BiquadParams::Type dalia::BiquadParams::type;
```




<hr>
## Public Functions Documentation




### function Sanitize 

```C++
void dalia::BiquadParams::Sanitize () 
```




<hr>

------------------------------
The documentation for this class was generated from the following file `engine/include/dalia/audio/EffectControl.h`

