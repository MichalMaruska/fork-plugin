* WdfDeviceOpenRegistryKey
device's hardware key or a driver's software key i
https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdfdevice/nf-wdfdevice-wdfdeviceopenregistrykey


**From Device to registry**

## Parameters
https://learn.microsoft.com/en-us/windows-hardware/drivers/develop/driver-isolation#registry-state





# Purely Registry

* WdfRegistryOpenKey
https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdfregistry/nf-wdfregistry-wdfregistryopenkey



### Strings?
* WdfStringGetUnicodeString

void WdfStringGetUnicodeString(
  [in]  WDFSTRING       String,
  [out] PUNICODE_STRING UnicodeString
);


* UNICODE_STRING
ctors: ... RtlUnicodeStringInit or RtlUnicodeStringInitEx

... it contains
** PWSTR  Buffer;



### Inf files:

Version
DestinationDirs
SourceDisksNames
SourceDisksFiles
* Manufacturer
what does it mean?
maruska = Standard,NT$ARCH$.10.0...16299
.... it takes the custom section:

%strkey%=models-section-name [,TargetOSVersion] [,TargetOSVersion] ...  (Windows XP and later versions of Windows)

NT[Architecture][.[OSMajorVersion][.[OSMinorVersion][.[ProductType][.SuiteMask]]]]
from win10:
NT[Architecture][.[OSMajorVersion][.[OSMinorVersion][.[ProductType][SuiteMask][.[BuildNumber]]]]]

TargetOSVersion: NT$ARCH$.10.0. . .16299
10 = windows10+
0  minor version

Windows 10 version 1709	16299  ???? 
Windows 10 version 1607 	14393    isn't better?

* [Manufacturer]
%FooCorp%=FooMfg, NTx86....0x80, NTamd64

leads to "INF Models section" section:
[models-section-name.TargetOSVersion]


[Standard.NT$ARCH$.10.0...16299]


## Inx files:
- $ARCH$ macro
$KMDFCOINSTALLERVERSION$
- $KMDFVERSION$
$UMDFVERSION$

"$Windows NT$"  ????

* DDInstall
https://learn.microsoft.com/en-us/windows-hardware/drivers/install/inf-ddinstall-section


keyboard.inf

[STANDARD_Inst]
Needs       = i8042prt_Inst
Needs       = KbdClass
AddReg      = STANDARD_AddReg

[KbdClass]
CopyFiles = KbdClass.CopyFiles

* Control Set ...alternatives
CurrentControlSet ...
