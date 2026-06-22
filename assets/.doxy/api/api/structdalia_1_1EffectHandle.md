

# Struct dalia::EffectHandle



[**ClassList**](annotated.md) **>** [**dalia**](namespacedalia.md) **>** [**EffectHandle**](structdalia_1_1EffectHandle.md)



_Handle used to manage effect instances. This handle expires once the effect it is referencing has been destroyed._ 

* `#include <EffectControl.h>`





































## Public Functions

| Type | Name |
| ---: | :--- |
|   | [**EffectHandle**](#function-effecthandle-12) () = default<br> |
|   | [**EffectHandle**](#function-effecthandle-22) (uint64\_t rawId) <br> |
|  uint64\_t | [**GetRawId**](#function-getrawid) () const<br> |
|  [**EffectType**](namespacedalia.md#enum-effecttype) | [**GetType**](#function-gettype) () const<br> |
|  bool | [**IsValid**](#function-isvalid) () const<br> |
|  bool | [**operator!=**](#function-operator) (const [**EffectHandle**](structdalia_1_1EffectHandle.md) & other) const<br> |
|  bool | [**operator==**](#function-operator_1) (const [**EffectHandle**](structdalia_1_1EffectHandle.md) & other) const<br> |




























## Public Functions Documentation




### function EffectHandle [1/2]

```C++
dalia::EffectHandle::EffectHandle () = default
```




<hr>



### function EffectHandle [2/2]

```C++
inline explicit dalia::EffectHandle::EffectHandle (
    uint64_t rawId
) 
```




<hr>



### function GetRawId 

```C++
inline uint64_t dalia::EffectHandle::GetRawId () const
```





**Returns:**

The underlying raw id of the handle. 





        

<hr>



### function GetType 

```C++
inline EffectType dalia::EffectHandle::GetType () const
```





**Returns:**

The type of effect the handle is referencing. 





        

<hr>



### function IsValid 

```C++
inline bool dalia::EffectHandle::IsValid () const
```





**Returns:**

true if the handle has referenced an effect at some point. Otherwise, false. 





        

<hr>



### function operator!= 

```C++
inline bool dalia::EffectHandle::operator!= (
    const EffectHandle & other
) const
```




<hr>



### function operator== 

```C++
inline bool dalia::EffectHandle::operator== (
    const EffectHandle & other
) const
```




<hr>

------------------------------
The documentation for this class was generated from the following file `engine/include/dalia/audio/EffectControl.h`

