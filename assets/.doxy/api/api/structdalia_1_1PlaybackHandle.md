

# Struct dalia::PlaybackHandle



[**ClassList**](annotated.md) **>** [**dalia**](namespacedalia.md) **>** [**PlaybackHandle**](structdalia_1_1PlaybackHandle.md)



_Handle used to manage playback instances. This handle will expire once the playback instance it is referencing stops._ 

* `#include <PlaybackControl.h>`





































## Public Functions

| Type | Name |
| ---: | :--- |
|  uint64\_t | [**GetRawId**](#function-getrawid) () const<br> |
|  bool | [**IsValid**](#function-isvalid) () const<br> |
|   | [**PlaybackHandle**](#function-playbackhandle-12) () = default<br> |
|   | [**PlaybackHandle**](#function-playbackhandle-22) (uint64\_t rawId) <br> |
|  bool | [**operator!=**](#function-operator) (const [**PlaybackHandle**](structdalia_1_1PlaybackHandle.md) & other) const<br> |
|  bool | [**operator==**](#function-operator_1) (const [**PlaybackHandle**](structdalia_1_1PlaybackHandle.md) & other) const<br> |




























## Public Functions Documentation




### function GetRawId 

```C++
inline uint64_t dalia::PlaybackHandle::GetRawId () const
```





**Returns:**

The underlying raw id of the handle. 





        

<hr>



### function IsValid 

```C++
inline bool dalia::PlaybackHandle::IsValid () const
```





**Returns:**

true if the handle has referenced an active playback instance at some point. Otherwise, false. 





        

<hr>



### function PlaybackHandle [1/2]

```C++
dalia::PlaybackHandle::PlaybackHandle () = default
```




<hr>



### function PlaybackHandle [2/2]

```C++
inline explicit dalia::PlaybackHandle::PlaybackHandle (
    uint64_t rawId
) 
```




<hr>



### function operator!= 

```C++
inline bool dalia::PlaybackHandle::operator!= (
    const PlaybackHandle & other
) const
```




<hr>



### function operator== 

```C++
inline bool dalia::PlaybackHandle::operator== (
    const PlaybackHandle & other
) const
```




<hr>

------------------------------
The documentation for this class was generated from the following file `engine/include/dalia/audio/PlaybackControl.h`

