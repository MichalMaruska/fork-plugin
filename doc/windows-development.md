# How to develop the driver



## Using a provisioned PC (either kvm/qemu simulated or real)

well, Visual Studio has the "Deploy" function.

## Using debugger?
/windows/minidump
https://learn.microsoft.com/en-us/troubleshoot/windows-client/performance/read-small-memory-dump-file
https://learn.microsoft.com/en-us/windows-hardware/drivers/debugger/debugger-download-tools
install !
As part of the WDK



## using a non-provisioned PC, which only does the minimum to install it

Always handy to have shared filesystem for quick transfer (in both directions). Samba...

follow [installation](windows-client-install.md)

### Certificate (once)
Use PowerShell to invoke

```
Import-Certificate -FilePath ".\kbfiltr.cer" -CertStoreLocation cert:\CurrentUser\Root
```

(maybe absolute path is necessary for the .cer file?)

And check in the certml tool that it's present (as one of Root certificates)
![Screenshot of certml](images/Screenshot-ITA-certificate-manger.png)






## Handy commands:
the path for the shared

Look at the log of driver installation/use at boot:
```
copy C:\Windows\inf\setupapi.dev.log \\server\share\
```

### remove old drivers
```
pnputil /enum-devices /class Keyboard
pnputil /enum-drivers /class Keyboard

pnputil /delete-driver oemXX.inf
```


## preparing the release:

certificate in text vs binary:
```
openssl x509 -text        -in kbfiltr.cer -out kbfiltr.txt

openssl x509 -outform DER -in kbfiltr.txt   -out kbfiltr.der
```


https://learn.microsoft.com/en-us/sysinternals/downloads/regjump
regjump put into Local/Microsoft\WindowsApps which is in $Env:PATH

http://www.windrvr.com/2016/01/06/build-drivers-from-the-command-line/

https://www.youtube.com/watch?v=AsSMKL5vaXw
msbuild


* enter recovery:
press power button during booting up?


PCUNICODE_STRING
p...pointer
 c ... const

## build-system

####  Stampinf
https://learn.microsoft.com/en-us/windows-hardware/drivers/develop/stampinf-properties-for-driver-projects

https://learn.microsoft.com/en-us/windows-hardware/drivers/wdf/using-inx-files-to-create-inf-files


* Zw version ensures that the caller resides in kernel mode, whereas Nt doesn't.


## Filter driver theory:
https://learn.microsoft.com/en-us/windows-hardware/drivers/gettingstarted/device-nodes-and-device-stacks

> notice that in one node, the filter driver is above the function driver, and in the other node, the filter driver is below the
> function driver.
> upper filter driver. A filter driver that is below the function driver is called a lower filter driver.


> When the drivers for a device are installed, the installer uses information in an information (INF) file to determine which
> driver is the function driver and which drivers are filters.


IRPs = I/O request packet
IRPs != IOCTL

does keyboard handle ps/2 and HID devices ?
FDO = functional device object
PDO = physical device object ?

lower vs upper filter:
https://learn.microsoft.com/en-us/windows-hardware/drivers/kernel/filter-drivers



## forums
https://community.osr.com/


## Signing!
https://community.osr.com/t/kmdf-driver-sign/59315
If you want to sign your drivers you would need an EV cert which takes a lot of vetting and is around $400 per year.

    Driver signing by EV
    Submit to Microsoft
    Microsoft sends back .cat file(If driver passed)
    Deploy or release
    Microsoft accepts only around 4 CAs.

EV cert is needed to establish your dashboard account.


* https://community.osr.com/t/how-to-sign-drivers/59316

I currently own my own software company and have an EV cert and I am trying to figure out how I can get my drivers to run outside
of Windows


The key is the dashboard account. You need the EV certificate to set up your dashboard account. Once you have that, submitting
packages for signing is easy. There is no other path.

Is your EV cert for your company?

# reboot avoidance?
https://community.osr.com/t/start-and-stop-device-in-filter-driverr/59276


* devcon (part of wdk)

devcon help
devcon help {cmd}
devcon help classfilter

devcon classes
> keyboard
devcon classfilter keyboard upper

status
stack
hwids


# keylayouts
README in

http://www.kbdedit.com/manual/low_level_modifiers.html
* Mapping KANA to VK_OEM_8 makes KANA behave like any other modifier: it is in ON state only when the modifier key is pressed
  down.
  so not VK_KANA !!! ?

* ROYA and LOYA modifiers are bound to fixed virtual codes VK_OEM_FJ_ROYA and VK_OEM_FJ_LOYA.  two distinct modifiers!

modifiers:

BASE combination (no modifier pressed) is somewhat special: it is always active, and must be the first element in the list of
active combinations.
so  .ModNumber[0] = 0 ?


6 modifier keys  shift, control, meta, kana, roya, loya !!
2^6 = 64

ALT alone, ie without CTRL, are not considered valid ....
this is  2^4  = 16

so 48 valid ....
 .... which treats position 16 as an internal delimiter value.

SHFT_INVALID  ???


### External tools
https://www.microsoft.com/en-us/download/details.aspx?id=102134
klc files ?  but see below:
https://github.com/rimas-kudelis/windows-keyboard-layouts
> I am aware that Microsoft has abandoned Microsoft Keyboard Layout Creator.


https://github.com/davidebeatrici/kli
needs
https://github.com/benhoyt/inih

https://github.com/rimas-kudelis/windows-keyboard-layouts/issues/1#issuecomment-1030586558



* MS account
https://partner.microsoft.com/dashboard/v2/home

* https://dennisbabkin.com/blog/?t=how-to-get-a-certificate-to-code-sign-windows-kernel-drivers

> Only a registered company can purchase an EV code-signing certificate.

> Your Company Website must provide some publicly available information about its line of business.

* CAB file?


* Company: $99  ... NL: 75 EUR
or 14 EUR  ?
https://partner.microsoft.com/
"Partner center"
https://partner.microsoft.com/dashboard/v2/home

Workspaces
+
https://partner.microsoft.com/dashboard/v2/account-settings/settings/programs

### Hardware
Use the work email address that you use to log in to Office 365, Microsoft Azure, or Microsoft Dynamics CRM (for example, you@yourcompany.com).

> A text or phone call helps us make sure this is you. Enter a number that isn't VoIP or toll free.

68...
068...
cannot receive SMS !!!!







https://www.advancedinstaller.com/publish-msi-exe-app-microsoft-store.html
msiexec.exe  ?
> silent installation
> offline installation  (after setup d/l-ed)
an


## Generic Windows C++
https://learn.microsoft.com/en-us/cpp/cpp/raw-pointers


exceptions & STL
https://github.com/microsoft/stl
MSVC's implementation of the C++ Standard Library. 
 StephanTLavavej

https://github.com/microsoft/wil
All of WIL can be used from user-space Windows code, and some (such as the RAII resource wrappers) can even be used in kernel mode.

Consuming WIL via NuGet/ vcpkg ?


* how to turn on debugging?

### Shell CMD
https://learn.microsoft.com/en-us/windows-server/administration/windows-commands/for

https://ss64.com/nt/delayedexpansion.html
https://stackoverflow.com/questions/10552812/defining-and-using-a-variable-in-batch-file
