## porting forking to Windows.


### Tools
* [Windows Driver Kit (WDK)](https://learn.microsoft.com/en-us/windows-hardware/drivers/download-the-wdk)
This has the instruction on how to configure VS:
* visual studio https://visualstudio.microsoft.com/vs/pricing/
* cmake
* Alternatively ... mentioned in the WDK page: I can download EWDK  18GB !!! iso ?

another 1.58 GB why?
while opening
input > kbfilter

ntddk.h missing:
https://learn.microsoft.com/en-us/windows-hardware/drivers/gettingstarted/writing-a-very-small-kmdf--driver
The tip is wrong:
> If you can't add Ntddk.h, open Configuration -> C/C++ -> General -> Additional Include Directories and add C:\Program Files (x86)\Windows Kits\10\Include\<build#>\km, replacing <build#> with the appropriate directory in your WDK installation.
Wrong:  it's Resources > General > Additional include directories



### Explore what is available:
* git
* https://learn.microsoft.com/en-us/windows-hardware/drivers/samples/input-driver-samples
https://learn.microsoft.com/en-us/samples/microsoft/windows-driver-samples/keyboard-input-wdf-filter-driver-kbfiltr/
what is WDM vs WDF ?

* VS ?

### what is WDM vs WDF ?
https://learn.microsoft.com/en-us/windows-hardware/drivers/wdf/wdm-concepts-for-kmdf-drivers

**Filter drivers** receive, review, and possibly modify data that flows between user applications and drivers



## Steps
* clone the repo
