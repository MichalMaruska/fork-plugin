# How to install the double-keyboard driver

* For now the driver is only available with my signature. So the Windows system must accept this signature.

First download the self-signed [certificate](./kbfiltr.cer), you can see it in [text](./kbfiltr.txt), then make it part of



* download the driver -- 3 files, into a separate folder/directory, and "Update the driver" of your ps/2 keyboard,
by pointing to that directory.

A reboot is needed.


# In case of trouble

If BSOD occurs, you will reboot to recovery. Then you can drop the driver:

Enter the CMD console (in Recovery):

>  dism /image:c:\ /get-drivers

scroll up if necessary and find the "kbdfiltr" driver, and its assigned name "oemN.inf" where N is some number.
Then remove it

>  dism /image:c:\ /remove-driver /driver:oem#.inf

for example oem7.inf or oem35.inf


Then reboot, and the driver will not be used anymore.

