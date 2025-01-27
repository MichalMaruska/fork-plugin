# How to install the double-keyboard driver (on Windows 10/11)

For now the driver is only available with my "signature". So the Windows system must accept this signature.

## Adding signature certificate
* First download the self-signed [certificate](./kbfiltr.cer), you can see it in [text](./kbfiltr.txt),
* Then make it part of the "Root certificates":
  Use PowerShell to invoke

```
Import-Certificate -FilePath ".\kbfiltr.cer" -CertStoreLocation cert:\CurrentUser\Root
```

It should be in as seen here:  ![Screenshot of certml](images/Screenshot-ITA-certificate-manger.png)

At this point the driver can be installed into the "DriverStore" .


But to be **used**, loaded into the kernel, you also need (as admininstrator):
```
bcdedit /set testsigning on
```
This can be later undone, with "off", if you only want to test this driver, and return to safer mode.


### Update the PS/2 keyboard driver:

* download the driver -- 3 [files](double-keyboard.zip), unzip into a separate folder/directory,
* then "Update the driver" of your  ps/2 keyboard, by pointing to that directory.

![Screenshot of keyboard drivers tree](images/Screenshot-ITA-driver-manager.png)

A reboot is needed.


## In case of trouble see [recovery tools](windows-recovery.md)


### Configuration
./kbftest from to

