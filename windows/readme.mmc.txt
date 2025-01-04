
MSBuild.exe /t:clean   /t:build  .\kbfiltr.sln  /p:Configuation=Debug /p:Platform=x64

PUNICODE_STRING
PCUNICODE_STRING .. const !!!


* apis:

wdf
https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdfcontrol/nf-wdfcontrol-wdfcontroldeviceinitallocate
https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdfrequest/nf-wdfrequest-wdfrequestsend

https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdfdevice/nf-wdfdevice-wdfdevicepostevent
https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdfdevice/nf-wdfdevice-wdfdevicewdmgetdeviceobject
https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdfdevice/nf-wdfdevice-wdfdeviceinitsetdevicetype

https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdfdriver/nc-wdfdriver-evt_wdf_driver_device_add
https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdfdriver/nf-wdfdriver-wdfdrivercreate

https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdfrequest/nf-wdfrequest-wdfrequestretrieveinputbuffer
https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdfrequest/nf-wdfrequest-wdfrequestcompletewithinformation
https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdfrequest/nf-wdfrequest-wdfrequestretrieveoutputmemory
https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdfio/nc-wdfio-evt_wdf_io_queue_io_device_control
* timer
https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdftimer/nf-wdftimer-wdftimerstart
https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdftimer/nf-wdftimer-wdftimerstop

* 
https://learn.microsoft.com/en-us/windows-hardware/drivers/wdf/summary-of-framework-objects


* time:
https://learn.microsoft.com/en-us/windows-hardware/drivers/network/attaching-timestamps-to-packets
https://learn.microsoft.com/en-us/windows-hardware/drivers/network/reporting-timestamping-capabilities
https://learn.microsoft.com/en-us/windows-hardware/drivers/stream/ksproperty-clock-physicaltime
** better:
https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdm/nf-wdm-kequeryinterrupttimeprecise
https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdm/nf-wdm-kequerysystemtime-r1
https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdm/nf-wdm-kequeryinterrupttime
https://cohost.org/tstudent/post/1371615-monotonic-clocks-on

https://learn.microsoft.com/en-us/windows-hardware/drivers/stream/default-clocks
https://learn.microsoft.com/en-us/windows-hardware/drivers/stream/ksproperty-clock-time
https://learn.microsoft.com/en-us/windows-hardware/drivers/stream/time-stamps
https://learn.microsoft.com/en-us/windows-hardware/drivers/stream/master-clocks


https://learn.microsoft.com/en-us/windows-hardware/drivers/wdf/creating-device-objects-in-a-filter-driver

https://learn.microsoft.com/en-us/windows-hardware/drivers/wdf/creating-i-o-queues


