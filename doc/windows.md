## Porting forking to Windows.


### Tools
* [Windows Driver Kit (WDK)](https://learn.microsoft.com/en-us/windows-hardware/drivers/download-the-wdk)
This has the instruction on how to configure VS:
* visual studio https://visualstudio.microsoft.com/vs/pricing/
* cmake (not yet)
* Alternatively ... mentioned in the WDK page: I can download EWDK  18GB !!! iso ?
* [github](https://github.com/MicrosoftDocs/windows-driver-docs)
* [samples](https://github.com/Microsoft/Windows-driver-samples)

I have WDK 10.0.26100.10

I spent hours building with this error, because I installed VS, SDK and WDK extension, but not WDK itself:
> ntddk.h missing:

WDK ... finally installing it (not the VS extension) 1.9GB

* I had to separately configure the kbfilter (apart from kbftest+kbfilter).... and select the 10.0.26100.10.

C-S-b  then built without errors.

kbfilter.sys  signed.

## Some theory:
pdo: Physical Device Object
FDO: Functional Device Object
filter-DO: filter Device Object
IoAttachDeviceToDeviceStack


CONNECT_DATA
... Pointer to an upper-level class **filter** device object (filter DO).
https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/kbdmou/ns-kbdmou-_connect_data


## what api do I target?
* [WDM](https://en.wikipedia.org/wiki/Windows_Driver_Model) stack, IRPs...
* KMDF [Kernel-Mode Driver Framework](https://en.wikipedia.org/wiki/Kernel-Mode_Driver_Framework)

3 types of drivers (in the stack) .... *filter*  .... lower-level and upper-level filter drivers??
WDF
....KMDF driver is less complicated and has less code than an equivalent WDM driver.

https://github.com/microsoft/Windows-Driver-Frameworks

https://learn.microsoft.com/en-us/samples/browse/?products=windows-wdk

another 1.58 GB why?
while opening
input > kbfilter


https://learn.microsoft.com/en-us/windows-hardware/drivers/gettingstarted/writing-a-very-small-kmdf--driver
The tip is wrong:
> If you can't add Ntddk.h, open Configuration -> C/C++ -> General -> Additional Include Directories and add C:\Program Files (x86)\Windows Kits\10\Include\<build#>\km, replacing <build#> with the appropriate directory in your WDK installation.
Wrong:  it's Resources > General > Additional include directories


SLN ... is the VS project file.
Extensions > WDK ....

VSIX ?  https://learn.microsoft.com/en-us/windows-hardware/drivers/download-the-wdk#download-icon-for-wdk-step-3-install-wdk



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


## How it works:

filter saves on Connect, and uses that to invoke "Class" driver from its "service" routine. Invoked by the
function driver.


https://learn.microsoft.com/en-us/previous-versions/windows/hardware/hid/keyboard-and-mouse-class-drivers



ACPI data -> INF file?
INF ... Hardware IDs and Compatible IDs listed in the INF
https://learn.microsoft.com/en-us/previous-versions/windows/hardware/hid/os-driver-installation




* docs:
https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/

https://learn.microsoft.com/en-us/windows-hardware/drivers/develop/building-a-driver

WDK or EWDK?



### Building on CMD:
msbuild.exe t:clean /t:build  ./kbfilter.sln /p:Configuration=Debug /p:Platform=x64

platform Win32

### Provisioning VM
https://learn.microsoft.com/en-us/windows-hardware/drivers/gettingstarted/provision-a-target-computer-wdk-8-1

* ping the Windows **guest**:

private or public?
Advanced > Sharing > File & printer Sharing !!!!
https://community.fortinet.com/t5/FortiGate/Troubleshooting-Tip-Window-10-computer-does-not-reply-to-ping/ta-p/286927


* ping from an unrelated Windows machine
  bridge vs

* I need "public" ip on.... Got it.

* Installing: WDK 10.0.26100.1742
  3.5 GB
  I disable the ARM component 3.3GB


"for installation on a separate computer" ???
# Provision:
https://learn.microsoft.com/en-us/windows-hardware/drivers/gettingstarted/provision-a-target-computer-wdk-8-1


* bcdedit /set testsigning on
  access is denied!
as admin:   The operation completed successfully.

protected by secure boot policy !!!!

so disable in the virsh.xml:
<feature enabled='no' name='secure-boot'/>
<feature enabled='no' name='enrolled-keys'/>
<loader readonly='yes' secure='no' type='pflash'>/usr/share/OVMF/OVMF_CODE_4M.fd</loader>

Unable to find 'efi' firmware that is compatible with the current configuration

restart:
virsh start windows11
virt-viewer  windows11

* Test Target Setup MSI
  didn't create any /DriverTest dir

* run:
  WDK Test Target Setup x64-x64_en-us.msi
  WDK Test Target Setup MSI

no, I don't see that. I see  Windows SDK DirectX x64 Remot ...


* oh, wdk 10.0.26100.2161 .... SDK and now WDK
  1.9 GB

Trying  to install ONLY wdk, no SDK, nor VS (compilers).



### Installing
* deploy:
https://learn.microsoft.com/en-us/windows-hardware/drivers/develop/deploying-a-driver-to-a-test-computer


*  WDTF, ?

* I see: "WDK remote user"
Hi ... signing you in


WDKRemoteUser ... it asked questions.

but:
ERROR: Task "Configuring kernel debugger settings (possible reboot)" failed to complete. ....

ERROR: Task "Configuring computer settings (possible reboot)" failed to complete. ....

the log says:

C:\DriverTest\ .... could not be retrieved...


But now I see DriverTest !!!

repeated -> worked ok.

On the next VM it worked first time.

## 

https://community.osr.com/t/filter-driver-development/54674/8
https://github.com/microsoft/Windows-driver-samples/issues/198


## links:
https://stackoverflow.com/questions/19110075/what-is-the-difference-between-pdo-and-fdo-in-windows-device-drivers


## Installing manually?

commands
* sc
* ebcdi


* how to install it:
https://learn.microsoft.com/en-us/samples/microsoft/windows-driver-samples/keyboard-input-wdf-filter-driver-kbfiltr/

## Certificate



## Fork related:
https://stackoverflow.com/questions/60411303/how-to-simulate-keyboard-input-at-driver-level-in-windows-10


** built-in filter?
https://msendpointmgr.com/2019/04/06/building-lock-down-device-part-1-keyboard-filter/
powerShell configuration

https://www.elbacom.com/kbfd/



* so what is the event on keyboard? where documented?
*
key events from th POV of application?
https://learn.microsoft.com/en-us/windows/win32/inputdev/wm-keydown?redirectedfrom=MSDN



* deploy fails:

VS > Extensions > Driver >0Test > Test Group Explorer >


## C++
https://www.youtube.com/watch?v=AsSMKL5vaXw


Did I learn about dbgview here?
https://www.youtube.com/watch?v=UlzctiY2NSg



* So ... safe mode didn't solve it.


* poolmon
ExAllocatePoolWithTag()
exFreePool()


pragma
https://learn.microsoft.com/en-us/cpp/preprocessor/alloc-text
