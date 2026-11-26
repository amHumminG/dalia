

# Struct dalia::BiquadParams



[**ClassList**](annotated.md) **>** [**dalia**](namespacedalia.md) **>** [**BiquadParams**](structdalia_1_1BiquadParams.md)



_Configuration parameters for a biquadratic IIR filter._ 

* `#include <Effect.hpp>`

















## Public Types

| Type | Name |
| ---: | :--- |
| enum uint8\_t | [**Type**](#enum-type)  <br>_Defines the frequency response shape of a standard biquadratic filter._  |




## Public Attributes

| Type | Name |
| ---: | :--- |
|  float | [**frequency**](#variable-frequency)   = `20000.0f`<br>_The cutoff or center frequency of the filter: 20.0 - 20000 Hz._  |
|  float | [**resonance**](#variable-resonance)   = `0.707f`<br>_The Q-factor of the filter: 0.1 - 10.0._  |
|  enum [**dalia::BiquadParams::Type**](structdalia_1_1BiquadParams.md#enum-type) | [**type**](#variable-type)   = `Type::LowPass`<br> |
















## Public Functions

| Type | Name |
| ---: | :--- |
|  void | [**Sanitize**](#function-sanitize) () <br>_Clamps internal parameters to safe ranges._  |




























## Public Types Documentation




### enum Type 

_Defines the frequency response shape of a standard biquadratic filter._ 
```C++
enum dalia::BiquadParams::Type {
    LowPass = 0,
    HighPass = 1,
    BandPass = 2
};
```




<hr>
## Public Attributes Documentation




### variable frequency 

_The cutoff or center frequency of the filter: 20.0 - 20000 Hz._ 
```C++
float dalia::BiquadParams::frequency;
```




<hr>



### variable resonance 

_The Q-factor of the filter: 0.1 - 10.0._ 
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

_Clamps internal parameters to safe ranges._ 
```C++
void dalia::BiquadParams::Sanitize () 
```




<hr>

------------------------------
The documentation for this class was generated from the following file `dalia/Effect.hpp`

