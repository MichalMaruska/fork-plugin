# Ways to recover from driver failure


### If **BSOD** occurs, you will reboot to recovery.

Then you can _remove_ the driver... enter the CMD console (in Recovery):

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


## If no **BSOD** occurs, but keyboard does not work on login:

This can happen for example if
```
bcdedit /set testsigning off
```

Then use the onscreen keyboard to login (under the Accessibility icon).

After login, remove the driver by...

1. Activate the on-screen keyboard by:
   Settings > Accessibility > on-screen keyboard.

2. fix the system by removing the driver (in device manager) or maybe fixing:

Open terminal/CMD as admin:
```
bcdedit /set testsigning on
```

## To force boot into recovery, once might try to abruptly switch off during the boot.
