

# Struct dalia::OutputDeviceInfo



[**ClassList**](annotated.md) **>** [**dalia**](namespacedalia.md) **>** [**OutputDeviceInfo**](structdalia_1_1OutputDeviceInfo.md)



_Contains metadata for an audio output device._ 

* `#include <Device.hpp>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  char | [**identifier**](#variable-identifier)  <br>_OS-level identifier._  |
|  bool | [**isDefault**](#variable-isdefault)   = `false`<br>_True if the device is set as OS default._  |
|  char | [**name**](#variable-name)  <br>_Readable device name._  |












































## Public Attributes Documentation




### variable identifier 

_OS-level identifier._ 
```C++
char dalia::OutputDeviceInfo::identifier[MAX_STR_LEN_DEVICE];
```




<hr>



### variable isDefault 

_True if the device is set as OS default._ 
```C++
bool dalia::OutputDeviceInfo::isDefault;
```




<hr>



### variable name 

_Readable device name._ 
```C++
char dalia::OutputDeviceInfo::name[MAX_STR_LEN_DEVICE];
```




<hr>

------------------------------
The documentation for this class was generated from the following file `dalia/Device.hpp`

