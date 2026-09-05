

# Struct dalia::SoundHandle



[**ClassList**](annotated.md) **>** [**dalia**](namespacedalia.md) **>** [**SoundHandle**](structdalia_1_1SoundHandle.md)



_Handle used to manage loaded sounds. This handle expires once the sound it is referencing is unloaded._ 

* `#include <SoundControl.h>`





































## Public Functions

| Type | Name |
| ---: | :--- |
|  uint64\_t | [**GetRawId**](#function-getrawid) () const<br> |
|  [**SoundType**](namespacedalia.md#enum-soundtype) | [**GetType**](#function-gettype) () const<br> |
|  bool | [**IsValid**](#function-isvalid) () const<br> |
|   | [**SoundHandle**](#function-soundhandle-12) () = default<br> |
|   | [**SoundHandle**](#function-soundhandle-22) (uint64\_t rawId) <br> |
|  bool | [**operator!=**](#function-operator) (const [**SoundHandle**](structdalia_1_1SoundHandle.md) & other) const<br> |
|  bool | [**operator==**](#function-operator_1) (const [**SoundHandle**](structdalia_1_1SoundHandle.md) & other) const<br> |




























## Public Functions Documentation




### function GetRawId 

```C++
inline uint64_t dalia::SoundHandle::GetRawId () const
```





**Returns:**

The underlying raw id of the handle. 





        

<hr>



### function GetType 

```C++
inline SoundType dalia::SoundHandle::GetType () const
```





**Returns:**

The type of sound (stream or resident) that this handle references. 





        

<hr>



### function IsValid 

```C++
inline bool dalia::SoundHandle::IsValid () const
```





**Returns:**

true if the handle has referenced a loaded sound at some point. Otherwise, false. 





        

<hr>



### function SoundHandle [1/2]

```C++
dalia::SoundHandle::SoundHandle () = default
```




<hr>



### function SoundHandle [2/2]

```C++
inline explicit dalia::SoundHandle::SoundHandle (
    uint64_t rawId
) 
```




<hr>



### function operator!= 

```C++
inline bool dalia::SoundHandle::operator!= (
    const SoundHandle & other
) const
```




<hr>



### function operator== 

```C++
inline bool dalia::SoundHandle::operator== (
    const SoundHandle & other
) const
```




<hr>

------------------------------
The documentation for this class was generated from the following file `engine/include/dalia/audio/SoundControl.h`

