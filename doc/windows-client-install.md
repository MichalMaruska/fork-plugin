# How to install the double-keyboard driver (on Windows 10/11)

For now the driver is only available with my "signature". So the Windows system must be made aware of/trust this signature.

## Adding signature certificate
* First download the self-signed [certificate](./kbfiltr.cer), you can see it in [text](./kbfiltr.txt),
* Then make it part of the "Root" certificates:
  Use PowerShell to invoke

```
Import-Certificate -FilePath ".\kbfiltr.cer" -CertStoreLocation cert:\CurrentUser\Root
```

It should be in as seen here:  ![Screenshot of certml](images/Screenshot-ITA-certificate-manger.png)

at this point the driver can be installed into the "DriverStore" .

But to be used, loaded into the kernel, you also need:
```
bcdedit /set testsigning on
```
This can be later undone, with "off", if you only want to test this driver, and return to safer mode.



### update the PS/2 keyboard driver with mine:

* download the driver -- 3 [files](double-keyboard.zip), unzip into a separate folder/directory,
* then "Update the driver" of your  ps/2 keyboard, by pointing to that directory.

![Screenshot of keyboard drivers tree](images/Screenshot-ITA-driver-manager.png)

A reboot is needed.


# In case of trouble

If **BSOD** occurs, you will reboot to recovery. Then you can remove the driver... enter the CMD console (in Recovery):

* Get the __assigned__ name: [dism](https://learn.microsoft.com/it-it/windows-hardware/manufacture/desktop/dism-driver-servicing-command-line-options-s14?view=windows-11#get-drivers)

```
  dism /image:c:\ /get-drivers /Format:Table
```

scroll up if necessary and find the "kbdfiltr" driver, and its assigned name "oemN.inf" where N is some number.

* Then remove it, the assigned oemN.inf name:

```
  dism /image:c:\ /remove-driver /driver:oem#.inf
```

for example oem7.inf or oem35.inf


Then reboot, and the driver will not be used anymore.


## If no **bsod**, but keyboard does not work on login:
This can be happen if
```
bcdedit /set testsigning off
```

Then use the onscreen keyboard to login (under the Accessibility icon), then remove the driver by ....

Activate the on-screen keyboard by:
Settings > Accessibility > on-screen keyboard.

and fix the system by removing the driver or maybe fixing:

Open terminal/CMD as admin:
```
bcdedit /set testsigning on
```
