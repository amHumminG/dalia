

# Struct dalia::Listener3DAttributes



[**ClassList**](annotated.md) **>** [**dalia**](namespacedalia.md) **>** [**Listener3DAttributes**](structdalia_1_1Listener3DAttributes.md)



_A snapshot of a listener's 3D state in the world._ 

* `#include <Playback.hpp>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  [**Vec3**](structdalia_1_1Vec3.md) | [**distanceProbePosition**](#variable-distanceprobeposition)   = `{0.0f, 0.0f, 0.0f}`<br>_if the distance mode of a playback instance is set to FromDistanceProbe._  |
|  [**Vec3**](structdalia_1_1Vec3.md) | [**forward**](#variable-forward)   = `{0.0f, 0.0f, 1.0f}`<br>_The direction the listener is facing._  |
|  [**Vec3**](structdalia_1_1Vec3.md) | [**position**](#variable-position)   = `{0.0f, 0.0f, 0.0f}`<br>_The world position that will be used for panning logic._  |
|  [**Vec3**](structdalia_1_1Vec3.md) | [**up**](#variable-up)   = `{0.0f, 1.0f, 0.0f}`<br>_The up direction from the listener position._  |
|  [**Vec3**](structdalia_1_1Vec3.md) | [**velocity**](#variable-velocity)   = `{0.0f, 0.0f, 0.0f}`<br>_The velocity of the listener._  |












































## Public Attributes Documentation




### variable distanceProbePosition 

_if the distance mode of a playback instance is set to FromDistanceProbe._ 
```C++
Vec3 dalia::Listener3DAttributes::distanceProbePosition;
```



The world position that will be used for attenuation logic 


        

<hr>



### variable forward 

_The direction the listener is facing._ 
```C++
Vec3 dalia::Listener3DAttributes::forward;
```




<hr>



### variable position 

_The world position that will be used for panning logic._ 
```C++
Vec3 dalia::Listener3DAttributes::position;
```




<hr>



### variable up 

_The up direction from the listener position._ 
```C++
Vec3 dalia::Listener3DAttributes::up;
```




<hr>



### variable velocity 

_The velocity of the listener._ 
```C++
Vec3 dalia::Listener3DAttributes::velocity;
```




<hr>

------------------------------
The documentation for this class was generated from the following file `dalia/Playback.hpp`

