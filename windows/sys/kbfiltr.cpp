/*--

Copyright (c) Microsoft Corporation.  All rights reserved.

    THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY
    KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
    IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR
    PURPOSE.


Module Name:

    kbfiltr.c

Abstract: This is an upper device filter driver sample for PS/2 keyboard. This
        driver layers in between the KbdClass driver and i8042prt driver and
        hooks the callback routine that moves keyboard inputs from the port
        driver to class driver. With this filter, you can remove or insert
        additional keys into the stream. This sample also creates a raw
        PDO and registers an interface so that application can talk to
        the filter driver directly without going thru the PS/2 devicestack.
        The reason for providing this additional interface is because the keyboard
        device is an exclusive secure device and it's not possible to open the
        device from usermode and send custom ioctls.

        If you want to filter keyboard inputs from all the keyboards (ps2, usb)
        plugged into the system then you can install this driver as a class filter
        and make it sit below the kbdclass filter driver by adding the service
        name of this filter driver before the kbdclass filter in the registry at
        " HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Control\Class\
        {4D36E96B-E325-11CE-BFC1-08002BE10318}\UpperFilters"


Environment:

    Kernel mode only.

--*/

#ifdef NTDD_WIN8
#define KERNEL 1
#else
#define KERNEL 1
#define DISABLE_STD_LIBRARY 1
#endif
static_assert(KERNEL == 1);

#include "kbfiltr.h"

#ifdef ALLOC_PRAGMA
#pragma alloc_text (INIT, DriverEntry)
#pragma alloc_text (PAGE, KbFilter_EvtDeviceAdd)
#pragma alloc_text (PAGE, KbFilter_EvtIoInternalDeviceControl)
#endif

#include "my-memory.h"
#include "win-environment.h"
#include "machine.h"
#include "circular.h"
extern "C" {
  extern PULONG InitSafeBootMode;
}

using machineConfig = forkNS::ForkConfiguration<USHORT, time_type, forkNS::MAX_KEYCODE >;

// experiments:
#if 1
#include "empty_last.h"
using last_event_t = empty_last_events_t<extendedEvent>;
#else
using last_event_t = circular_buffer<extendedEvent, false, kernelAllocator<extendedEvent> >;
#endif

using machineRec =  forkNS::forkingMachine<USHORT, time_type,
                                           extendedEvent, winEnvironment, extendedEvent, last_event_t >;


void KbFilter_EvtWdfTimer(IN WDFTIMER Timer);
static NTSTATUS prepare_timer(WDFDEVICE hDevice);


ULONG InstanceNo = 0;

extern "C" NTSTATUS
DriverEntry(
    IN PDRIVER_OBJECT  DriverObject,
    IN PUNICODE_STRING RegistryPath
    )
/*++

Routine Description:

    Installable driver initialization entry point.
    This entry point is called directly by the I/O system.

Arguments:

    DriverObject - pointer to the driver object

    RegistryPath - pointer to a unicode string representing the path,
                   to driver-specific key in the registry.

Return Value:

    STATUS_SUCCESS if successful,
    STATUS_UNSUCCESSFUL otherwise.

--*/
{
    WDF_DRIVER_CONFIG               config;
    NTSTATUS                        status = STATUS_SUCCESS;

    DebugPrint(("Double-keyboard Driver.\n"));
    DebugPrint(("Built %s %s\n", __DATE__, __TIME__));
    DebugPrint(("Registry root %wZ.\n", RegistryPath));

    if (*InitSafeBootMode == 0) {

        //
        // Initialize driver config to control the attributes that
        // are global to the driver. Note that framework by default
        // provides a driver unload routine. If you create any resources
        // in the DriverEntry and want to be cleaned in driver unload,
        // you can override that by manually setting the EvtDriverUnload in the
        // config structure. In general xxx_CONFIG_INIT macros are provided to
        // initialize most commonly used members.
        //

        WDF_DRIVER_CONFIG_INIT(
            &config,
            KbFilter_EvtDeviceAdd
        );

        //
        // Create a framework driver object to represent our driver.
        //
        status = WdfDriverCreate(DriverObject,
                                 RegistryPath,
                                 WDF_NO_OBJECT_ATTRIBUTES,
                                 &config,
                                 WDF_NO_HANDLE); // hDriver optional
        if (!NT_SUCCESS(status)) {
            DebugPrint(("WdfDriverCreate failed with status 0x%x\n", status));
        }
    } else {
        KdPrint(("mmc: safe mode -> skipping \n"));
    }

    return status;
}

static NTSTATUS create_machine(PDEVICE_EXTENSION filterExt, WDFDEVICE hDevice);
static NTSTATUS configure_from_registry(IN WDFDRIVER Driver,
                                        IN WDFDEVICE Device,
                                        machineRec* forking_machine);
static NTSTATUS configure_from_registry_key(IN WDFKEY hKey,
                                            machineRec* forking_machine);

NTSTATUS
KbFilter_EvtDeviceAdd(
    IN WDFDRIVER        Driver,
    IN PWDFDEVICE_INIT  DeviceInit
    )
/*++
Routine Description:

    EvtDeviceAdd is called by the framework in response to AddDevice
    call from the PnP manager. Here you can query the device properties
    using WdfFdoInitWdmGetPhysicalDevice/IoGetDeviceProperty and based
    on that, decide to create a filter device object and attach to the
    function stack.

    If you are not interested in filtering this particular instance of the
    device, you can just return STATUS_SUCCESS without creating a framework
    device.

Arguments:

    Driver - Handle to a framework driver object created in DriverEntry

    DeviceInit - Pointer to a framework-allocated WDFDEVICE_INIT structure.

Return Value:

    NTSTATUS

--*/
{
    WDF_OBJECT_ATTRIBUTES   deviceAttributes;
    NTSTATUS                status;
    WDFDEVICE               hDevice;
    WDFQUEUE                hQueue;
    PDEVICE_EXTENSION       filterExt;
    WDF_IO_QUEUE_CONFIG     ioQueueConfig;

    UNREFERENCED_PARAMETER(Driver);

    PAGED_CODE();

    DebugPrint(("Enter FilterEvtDeviceAdd \n"));

    //
    // Tell the framework that you are filter driver. Framework
    // takes care of inherting all the device flags & characterstics
    // from the lower device you are attaching to.
    //
    WdfFdoInitSetFilter(DeviceInit);

    WdfDeviceInitSetDeviceType(DeviceInit, FILE_DEVICE_KEYBOARD);

    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&deviceAttributes, DEVICE_EXTENSION);

    //
    // Create a framework device object.  This call will in turn create
    // a WDM deviceobject, attach to the lower stack and set the
    // appropriate flags and attributes.
    //
    status = WdfDeviceCreate(&DeviceInit, &deviceAttributes, &hDevice);
    if (!NT_SUCCESS(status)) {
        DebugPrint(("WdfDeviceCreate failed with status code 0x%x\n", status));
        return status;
    }

    filterExt = FilterGetData(hDevice);

    //
    // Configure the default queue to be Parallel. Do not use sequential queue
    // if this driver is going to be filtering PS2 ports because it can lead to
    // deadlock. The PS2 port driver sends a request to the top of the stack when it
    // receives an ioctl request and waits for it to be completed. If you use a
    // a sequential queue, this request will be stuck in the queue because of the 
    // outstanding ioctl request sent earlier to the port driver.
    //
    WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(&ioQueueConfig,
                             WdfIoQueueDispatchParallel);

    //
    // Framework by default creates non-power managed queues for
    // filter drivers.
    //
    ioQueueConfig.EvtIoInternalDeviceControl = KbFilter_EvtIoInternalDeviceControl;

    status = WdfIoQueueCreate(hDevice,
                            &ioQueueConfig,
                            WDF_NO_OBJECT_ATTRIBUTES,
                            WDF_NO_HANDLE // pointer to default queue
                            );
    if (!NT_SUCCESS(status)) {
        DebugPrint( ("WdfIoQueueCreate failed 0x%x\n", status));
        return status;
    }

    //
    // Create a new queue to handle IOCTLs that will be forwarded to us from
    // the rawPDO. 
    //
    WDF_IO_QUEUE_CONFIG_INIT(&ioQueueConfig,
                             WdfIoQueueDispatchParallel);

    //
    // Framework by default creates non-power managed queues for
    // filter drivers.
    //
    ioQueueConfig.EvtIoDeviceControl = KbFilter_EvtIoDeviceControlFromRawPdo;

    status = WdfIoQueueCreate(hDevice,
                            &ioQueueConfig,
                            WDF_NO_OBJECT_ATTRIBUTES,
                            &hQueue
                            );
    if (!NT_SUCCESS(status)) {
        DebugPrint( ("WdfIoQueueCreate failed 0x%x\n", status));
        return status;
    }

    filterExt->rawPdoQueue = hQueue;

    //
    // Create a RAW pdo so we can provide a sideband communication with
    // the application. Please note that not filter drivers desire to
    // produce such a communication and not all of them are contrained
    // by other filter above which prevent communication thru the device
    // interface exposed by the main stack. So use this only if absolutely
    // needed. Also look at the toaster filter driver sample for an alternate
    // approach to providing sideband communication.
    //
    status = KbFiltr_CreateRawPdo(hDevice, ++InstanceNo);

    status = prepare_timer(hDevice);

    if (!NT_SUCCESS(status)) {
        DebugPrint(("WdfTimerCreate failed 0x%x\n", status));
        return status;
    }

    status = create_machine(filterExt, hDevice);
    if (!NT_SUCCESS(status)) {
        DebugPrint(("machine creation failed 0x%x\n", status));
        return status;
    }

    KdPrint(("mmc: everything passed\n"));
    // registry:
    configure_from_registry(Driver, hDevice, (machineRec*) filterExt->machine);
#if 0
    if (*InitSafeBootMode == 0)

    Do not attach the filter device object to the device stack.
    Return success from the filter driver's AddDevice routine.
      KbFilter_EvtDeviceAdd

    #include <wdfstring.h>
    WDFSTRING              *String;
    WDFSTRING  stringHandle = NULL;

    // atts
    WDF_OBJECT_ATTRIBUTES  attributes;
    WDF_OBJECT_ATTRIBUTES_INIT(&attributes);

    NTSTATUS WdfStringCreate(PCUNICODE_STRING       UnicodeString, // copied !!!с
                             PWDF_OBJECT_ATTRIBUTES StringAttributes, // WDF_NO_OBJECT_ATTRIBUTES
                             WDFSTRING              *String);


    NTSTATUS
    WdfRegistryQueryString(WDFKEY Key, PCUNICODE_STRING ValueName,
                           // A handle to a framework string object.
                           WDFSTRING String);


    // what's the use?
    HANDLE WdfRegistryWdmGetHandle(WDFKEY Key);

    //
    NTSTATUS WdfRegistryCreateKey(WDFKEY                 ParentKey,
                                  PCUNICODE_STRING       KeyName,
                                  ACCESS_MASK            DesiredAccess,
                                  ULONG                  CreateOptions,
                                  PULONG                 CreateDisposition,
                                  PWDF_OBJECT_ATTRIBUTES KeyAttributes,
                                  WDFKEY                 *Key
                                  );

    NTSTATUS WdfRegistryQueryMultiString(WDFKEY                 Key,
                                         PCUNICODE_STRING       ValueName,
                                         PWDF_OBJECT_ATTRIBUTES StringsAttributes,
                                         WDFCOLLECTION          Collection);
    // assign:
    NTSTATUS WdfRegistryAssignMultiString([in] WDFKEY           Key,
                                          [in] PCUNICODE_STRING ValueName,
                                          [in] WDFCOLLECTION    StringsCollection
                                          );

    void WdfRegistryClose(WDFKEY Key);

#endif

    return status;
}


static NTSTATUS
save_global_value(IN WDFKEY hKey,
                  PWSTR ValueNameW,
                  machineRec* forking_machine,
                  int attribute)
{
    const bool GET=false;
    UNICODE_STRING ValueName;
    RtlInitUnicodeString(&ValueName, ValueNameW);
    return WdfRegistryAssignULong(hKey, &ValueName,
                                  forking_machine->configure_global(attribute, 0, GET));
}

static NTSTATUS
restore_global_value(IN WDFKEY hKey,
                     PWSTR ValueNameW,
                     machineRec* forking_machine,
                     int attribute)
{
    const bool SET=true;
    UNICODE_STRING ValueName;
    RtlInitUnicodeString(&ValueName, ValueNameW);
    ULONG Value;
    NTSTATUS status;

    status = WdfRegistryQueryULong(hKey,
                                   &ValueName,
                                   &Value);
    if (NT_SUCCESS(status)) {
        forking_machine->configure_global(attribute, Value, SET);
    } else {
        DebugPrint(("configuration value %S not found\n", ValueNameW));
    }
    return status;
}


static NTSTATUS
configure_from_registry(IN WDFDRIVER Driver,
                        IN WDFDEVICE Device,
                        machineRec* forking_machine)
{
    UNREFERENCED_PARAMETER(Driver);
    UNREFERENCED_PARAMETER(Device);
    NTSTATUS status;
    // Find some Key:
    WDFKEY hKey;

    // PWDF_OBJECT_ATTRIBUTES KeyAttributes,
    status = WdfDriverOpenPersistentStateRegistryKey(Driver,
                                                     STANDARD_RIGHTS_ALL,
                                                     WDF_NO_OBJECT_ATTRIBUTES,
                                                     &hKey);
    if (!NT_SUCCESS(status)) {
        DebugPrint(("failed to retrieve State registry key 0x%x\n", status));
        return status;
    }

    status = configure_from_registry_key(hKey, forking_machine);
    WdfRegistryClose(hKey);
    return status;
}


static NTSTATUS
configure_from_registry_key(IN WDFKEY hKey,
                            machineRec* forking_machine)
{
    NTSTATUS status;

#if VERIFICATION_MATRIX
    restore_global_value(hKey,
                         L"overlap_limit",
                         forking_machine,
                         fork_configure_overlap_limit);
#endif
    restore_global_value(hKey,
                         L"clear_interval",
                         forking_machine,
                         fork_configure_clear_interval);

    restore_global_value(hKey,
                         L"repeat_limit",
                         forking_machine,
                         fork_configure_repeat_limit);

    restore_global_value(hKey,
                         L"repeat_consider_forks",
                         forking_machine,
                         fork_configure_repeat_consider_forks);

    restore_global_value(hKey,
                         L"debug",
                         forking_machine,
                         fork_configure_debug);


#define MAX_FORKS 16

    DECLARE_CONST_UNICODE_STRING(valueBinary, L"binary-forks");

    const bool SET=true;
    ULONG binary_values[MAX_FORKS];
    ULONG ValueType;
    ULONG ValueLengthQueried;

    status = WdfRegistryQueryValue(hKey, &valueBinary,
                                   // sizeof(binary_values)
                                   MAX_FORKS * sizeof(ULONG),
                                   binary_values,
                                   &ValueLengthQueried,
                                   &ValueType
                                  );

    if (!NT_SUCCESS(status))
    {
        DebugPrint(("WdfRegistryQueryValue failed %x\n", status));
        return status;
    } else if (ValueType != REG_BINARY)
    {
        DebugPrint(("WdfRegistryQueryValue returned unexpected Type %lu\n", ValueType));
        return STATUS_NOT_IMPLEMENTED;
    }
    for  (int i = 0; i < ValueLengthQueried / sizeof(ULONG); i++) {
        ULONG val = binary_values[i];
        USHORT key = val >> 16;
#define MAX_USHORT 65535
//         MaxValue =
        USHORT fork = val & MAX_USHORT;
        // binary_values[top++] = key << 16 | value;
        DebugPrint(("fork %u to %u\n", key, fork));
        if (key > forkNS::MAX_KEYCODE) {
            DebugPrint(("skipping overflow key %u\n", key));
            continue;
        } else {
            forking_machine->configure_key(fork_configure_key_fork, key,  fork, SET);
        }
    }

    return status;
}


static NTSTATUS
save_configuration_to_registry(IN WDFKEY hKey,
                               machineRec* forking_machine)
{
    NTSTATUS status = STATUS_SUCCESS;

#if VERIFICATION_MATRIX
    save_global_value(hKey,
                      L"overlap_limit",
                      forking_machine,
                      fork_configure_overlap_limit);
#endif
    save_global_value(hKey,
                      L"clear_interval",
                      forking_machine,
                      fork_configure_clear_interval);

    save_global_value(hKey,
                      L"repeat_limit",
                      forking_machine,
                      fork_configure_repeat_limit);

    save_global_value(hKey,
                      L"repeat_consider_forks",
                      forking_machine,
                      fork_configure_repeat_consider_forks);

    save_global_value(hKey,
                      L"debug",
                      forking_machine,
                      fork_configure_debug);

    // forks in binary:
    // find
    // maximum 16 forks?
    const bool GET=false;
    DECLARE_CONST_UNICODE_STRING(valueBinary, L"binary-forks");
    ULONG binary_values[MAX_FORKS];
    int top = 0;

    for(USHORT key = 0; key < forkNS::MAX_KEYCODE; key++) {
        // int -> short
        if (USHORT value = (USHORT) forking_machine->configure_key(fork_configure_key_fork, key,  0, GET);
            value != 0)
        {
            DebugPrint(("found a forked key: %u ->  %u\n", key, value));

            binary_values[top++] = key << 16 | value;
            // append USHORT USHORT
            if (top == MAX_FORKS) {
                DebugPrint(("Reached the maximum number of forks to be stored\n"));
                break;
            }
        }
    }

    if (top != 0) {
        status = WdfRegistryAssignValue(hKey, &valueBinary,
                                        REG_BINARY,
                                        top * sizeof(ULONG),
                                        binary_values);
    } else {
        DebugPrint(("Nothing to store about forked keys\n"));
    }

    return status;
}


// NonPagedPool is restricted, can fail!
void* operator ::new(size_t size) {
    const long tag = (long) 'kbfi';
    return ExAllocatePool2(POOL_FLAG_PAGED, size, tag);
}

static NTSTATUS
create_machine(PDEVICE_EXTENSION filterExt, WDFDEVICE hDevice)
{
    auto *environment = ::new(winEnvironment);
    environment->hDevice = hDevice;

    machineConfig *config = ::new machineConfig;

    auto* forking_machine = ::new(machineRec)(environment);
    forking_machine->config = config;
    filterExt->machine = forking_machine;

    forking_machine->set_debug(1);

    return  STATUS_SUCCESS;
}

static NTSTATUS
prepare_timer(WDFDEVICE hDevice)
{
    PDEVICE_EXTENSION filterExt = FilterGetData(hDevice);
    WDF_TIMER_CONFIG timerConfig;

    WDF_TIMER_CONFIG_INIT(&timerConfig,
                          KbFilter_EvtWdfTimer);

    WDF_OBJECT_ATTRIBUTES timerAttributes;
    WDF_OBJECT_ATTRIBUTES_INIT(&timerAttributes);
    timerAttributes.ParentObject = hDevice;

    return WdfTimerCreate(&timerConfig,
                          &timerAttributes,
                          &filterExt->timerHandle);
}

inline long current_time_miliseconds()
{
    LARGE_INTEGER CurrentTime;
    KeQuerySystemTime(&CurrentTime);
    // count of 100-nanosecond intervals since
    const __int64 &t = CurrentTime.QuadPart;
    return (long) ( t / (1000 * 10) % (3600 * 1000)); // inside 1 hour
}

void accept_time(long time, PDEVICE_EXTENSION devExt);

void KbFilter_EvtWdfTimer(IN WDFTIMER Timer) {

    WDFOBJECT hDevice =  WdfTimerGetParentObject(Timer);
    PDEVICE_EXTENSION devExt = FilterGetData(hDevice);

    long time = current_time_miliseconds();
    KdPrint(("%s woken up at %ld\n", __func__, time));
    accept_time(time, devExt);
}


/* Return a value requested, or 0 on error.*/
inline int subtype_n_args(int t) { return  (t & 3);}
inline int type_subtype(int t) { return (t >> 2);}

static int
machine_configure(machineRec* machine, int values[5])
{
  // assert (strcmp (PLUGIN_NAME(plugin), FORK_PLUGIN_NAME) == 0);

   const int type = values[0];

   /* fixme: why is type int?  shouldn't CARD8 be enough?
      <int type>
      <int keycode or time value>
      <keycode or time value>
      <timevalue>

      type: local & global
   */

   machine->mdb("%s: %d operands, command %d: %d %d\n", __func__, subtype_n_args(type),
                type_subtype(type), values[1], values[2]);

   switch (subtype_n_args(type)){
   case fork_GLOBAL_OPTION:
           machine->configure_global(type_subtype(type), (USHORT) values[1], true);
           break;
   case fork_LOCAL_OPTION:
           machine->configure_key(type_subtype(type), (USHORT) values[1], (USHORT) values[2], true);
           break;
   case fork_TWIN_OPTION:
           machine->configure_twins(type_subtype(type), (USHORT) values[1], (USHORT) values[2], (USHORT) values[3], true);
           break;

   default:
      machine->mdb("%s: invalid option %d\n", __func__, subtype_n_args(type));
   }
   return 0;
}


// https://learn.microsoft.com/en-us/windows-hardware/drivers/wdf/request-handlers
VOID
KbFilter_EvtIoDeviceControlFromRawPdo(
    IN WDFQUEUE      Queue,
    IN WDFREQUEST    Request,
    IN size_t        OutputBufferLength,
    IN size_t        InputBufferLength,
    IN ULONG         IoControlCode
    )
/*++

Routine Description:

    This routine is the dispatch routine for device control requests.

Arguments:

    Queue - Handle to the framework queue object that is associated
            with the I/O request.
    Request - Handle to a framework request object.

    OutputBufferLength - length of the request's output buffer,
                        if an output buffer is available.
    InputBufferLength - length of the request's input buffer,
                        if an input buffer is available.

    IoControlCode - the driver-defined or system-defined I/O control code
                    (IOCTL) that is associated with the request.

Return Value:

   VOID

--*/
{
    NTSTATUS status = STATUS_SUCCESS;
    WDFDEVICE hDevice;
    WDFMEMORY outputMemory;
    PDEVICE_EXTENSION devExt;
    size_t bytesTransferred = 0;

    UNREFERENCED_PARAMETER(InputBufferLength);

    DebugPrint(("Entered KbFilter_EvtIoDeviceControlFromRawPdo\n"));

    hDevice = WdfIoQueueGetDevice(Queue);
    devExt = FilterGetData(hDevice);

    //
    // Process the ioctl and complete it when you are done.
    //

    switch (IoControlCode) {
    case IOCTL_KBFILTR_GET_KEYBOARD_ATTRIBUTES:

        //
        // Buffer is too small, fail the request
        //
        if (OutputBufferLength < sizeof(KEYBOARD_ATTRIBUTES)) {
            status = STATUS_BUFFER_TOO_SMALL;
            break;
        }

        // output buffer:
        status = WdfRequestRetrieveOutputMemory(Request, &outputMemory);

        if (!NT_SUCCESS(status)) {
            DebugPrint(("WdfRequestRetrieveOutputMemory failed %x\n", status));
            break;
        }

        status = WdfMemoryCopyFromBuffer(outputMemory,
                                    0,
                                    &devExt->KeyboardAttributes,
                                    sizeof(KEYBOARD_ATTRIBUTES));

        if (!NT_SUCCESS(status)) {
            DebugPrint(("WdfMemoryCopyFromBuffer failed %x\n", status));
            break;
        }

        bytesTransferred = sizeof(KEYBOARD_ATTRIBUTES);

        break;

    case IOCTL_KBFILTR_SET_FORK:
    {
        DebugPrint(("trying to set fork\n"));

        devExt = FilterGetData(hDevice);
        auto *forking_machine = (machineRec*) devExt->machine;

        PVOID InputBuffer;
        InputBuffer = NULL;
        // if (InputBufferLength < sizeof(ULONG))
        if (InputBufferLength > 0) {
            status = WdfRequestRetrieveInputBuffer(Request,
                                                   InputBufferLength,
                                                   &InputBuffer,
                                                   NULL);
        }

        int values[5];
        for (int i=0; i< InputBufferLength/sizeof(int); i++) {
            // *(PULONG)
            values[i] = *((int*) InputBuffer + i);
        }
        // other values should be zero.
        machine_configure(forking_machine, values);

        // todo: put into the OutputBuffer if OutputBufferLength

        WDFKEY hKey;

        // PWDF_OBJECT_ATTRIBUTES KeyAttributes,
        status = WdfDriverOpenPersistentStateRegistryKey(
            WdfGetDriver(), STANDARD_RIGHTS_ALL, WDF_NO_OBJECT_ATTRIBUTES,
            &hKey);
        if (!NT_SUCCESS(status))
          break;
        status = save_configuration_to_registry(hKey, forking_machine);
        WdfRegistryClose(hKey);
        break;
    }
    default:
      status = STATUS_NOT_IMPLEMENTED;
      break;
    }

      WdfRequestCompleteWithInformation(Request, status, bytesTransferred);

      return;
}

VOID
KbFilter_EvtIoInternalDeviceControl(
    IN WDFQUEUE      Queue,
    IN WDFREQUEST    Request,
    IN size_t        OutputBufferLength,
    IN size_t        InputBufferLength,
    IN ULONG         IoControlCode
    )
/*++

Routine Description:

    This routine is the dispatch routine for internal device control requests.
    There are two specific control codes that are of interest:

    IOCTL_INTERNAL_KEYBOARD_CONNECT:
        Store the old context and function pointer and replace it with our own.
        This makes life much simpler than intercepting IRPs sent by the RIT and
        modifying them on the way back up.

    IOCTL_INTERNAL_I8042_HOOK_KEYBOARD:
        Add in the necessary function pointers and context values so that we can
        alter how the ps/2 keyboard is initialized.

    NOTE:  Handling IOCTL_INTERNAL_I8042_HOOK_KEYBOARD is *NOT* necessary if
           all you want to do is filter KEYBOARD_INPUT_DATAs.  You can remove
           the handling code and all related device extension fields and
           functions to conserve space.

Arguments:

    Queue - Handle to the framework queue object that is associated
            with the I/O request.
    Request - Handle to a framework request object.

    OutputBufferLength - length of the request's output buffer,
                        if an output buffer is available.
    InputBufferLength - length of the request's input buffer,
                        if an input buffer is available.

    IoControlCode - the driver-defined or system-defined I/O control code
                    (IOCTL) that is associated with the request.

Return Value:

   VOID

--*/
{
    PDEVICE_EXTENSION               devExt;
    PINTERNAL_I8042_HOOK_KEYBOARD   hookKeyboard = NULL;
    PCONNECT_DATA                   connectData = NULL;
    NTSTATUS                        status = STATUS_SUCCESS;
    size_t                          length;
    WDFDEVICE                       hDevice;
    BOOLEAN                         forwardWithCompletionRoutine = FALSE;
    BOOLEAN                         ret = TRUE;
    WDFCONTEXT                      completionContext = WDF_NO_CONTEXT;
    WDF_REQUEST_SEND_OPTIONS        options;
    WDFMEMORY                       outputMemory;
    UNREFERENCED_PARAMETER(OutputBufferLength);
    UNREFERENCED_PARAMETER(InputBufferLength);


    PAGED_CODE();

    DebugPrint(("Entered KbFilter_EvtIoInternalDeviceControl\n"));

    hDevice = WdfIoQueueGetDevice(Queue);
    devExt = FilterGetData(hDevice);

    switch (IoControlCode) {

    //
    // Connect a keyboard class device driver to the port driver.
    //
    case IOCTL_INTERNAL_KEYBOARD_CONNECT:
        //
        // Only allow one connection.
        //
        if (devExt->UpperConnectData.ClassService != NULL) {
            status = STATUS_SHARING_VIOLATION;
            break;
        }

        //
        // Get the input buffer from the request
        // (Parameters.DeviceIoControl.Type3InputBuffer).
        //
        status = WdfRequestRetrieveInputBuffer(Request,
                                    sizeof(CONNECT_DATA),
                                    (PVOID *) &connectData,
                                    &length);
        if(!NT_SUCCESS(status)){
            DebugPrint(("WdfRequestRetrieveInputBuffer failed %x\n", status));
            break;
        }

        NT_ASSERT(length == InputBufferLength);

        devExt->UpperConnectData = *connectData;

        //
        // Hook into the report chain.  Everytime a keyboard packet is reported
        // to the system, KbFilter_ServiceCallback will be called
        //

        connectData->ClassDeviceObject = WdfDeviceWdmGetDeviceObject(hDevice);

#pragma warning(disable:4152)  //nonstandard extension, function/data pointer conversion

        connectData->ClassService = KbFilter_ServiceCallback;

#pragma warning(default:4152)

        break;

    //
    // Disconnect a keyboard class device driver from the port driver.
    //
    case IOCTL_INTERNAL_KEYBOARD_DISCONNECT:

        //
        // Clear the connection parameters in the device extension.
        //
        // devExt->UpperConnectData.ClassDeviceObject = NULL;
        // devExt->UpperConnectData.ClassService = NULL;

        status = STATUS_NOT_IMPLEMENTED;
        break;

    //
    // Attach this driver to the initialization and byte processing of the
    // i8042 (ie PS/2) keyboard.  This is only necessary if you want to do PS/2
    // specific functions, otherwise hooking the CONNECT_DATA is sufficient
    //
    case IOCTL_INTERNAL_I8042_HOOK_KEYBOARD:

        DebugPrint(("hook keyboard ioctl received!\n"));

        //
        // Get the input buffer from the request
        // (Parameters.DeviceIoControl.Type3InputBuffer)
        //
        status = WdfRequestRetrieveInputBuffer(Request,
                            sizeof(INTERNAL_I8042_HOOK_KEYBOARD),
                            (PVOID*) &hookKeyboard, // who can give us this?
                            &length);
        if(!NT_SUCCESS(status)){
            DebugPrint(("WdfRequestRetrieveInputBuffer failed %x\n", status));
            break;
        }

        NT_ASSERT(length == InputBufferLength);

        //
        // Enter our own initialization routine and record any Init routine
        // that may be above us.  Repeat for the isr hook
        //
        devExt->UpperContext = hookKeyboard->Context;

        //
        // replace old Context with our own
        //
        hookKeyboard->Context = (PVOID) devExt;

        if (hookKeyboard->InitializationRoutine) {
            devExt->UpperInitializationRoutine =
                hookKeyboard->InitializationRoutine;
        }
        hookKeyboard->InitializationRoutine =
            (PI8042_KEYBOARD_INITIALIZATION_ROUTINE)
            KbFilter_InitializationRoutine;

#if 0
        // mmc: I don't need it
        if (hookKeyboard->IsrRoutine) {
            devExt->UpperIsrHook = hookKeyboard->IsrRoutine;
        }
        hookKeyboard->IsrRoutine = (PI8042_KEYBOARD_ISR) KbFilter_IsrHook;
#endif

#if 0
        //
        // Store all of the other important stuff
        //
        devExt->IsrWritePort = hookKeyboard->IsrWritePort;
        devExt->QueueKeyboardPacket = hookKeyboard->QueueKeyboardPacket;
        devExt->CallContext = hookKeyboard->CallContext;
#endif
        status = STATUS_SUCCESS;
        break;


    case IOCTL_KEYBOARD_QUERY_ATTRIBUTES:
        forwardWithCompletionRoutine = TRUE;
        completionContext = devExt;
        break;

    //
    // Might want to capture these in the future.  For now, then pass them down
    // the stack.  These queries must be successful for the RIT to communicate
    // with the keyboard.
    //
    case IOCTL_KEYBOARD_QUERY_INDICATOR_TRANSLATION:
    case IOCTL_KEYBOARD_QUERY_INDICATORS:
    case IOCTL_KEYBOARD_SET_INDICATORS:
    case IOCTL_KEYBOARD_QUERY_TYPEMATIC:
    case IOCTL_KEYBOARD_SET_TYPEMATIC:
        break;
    }

    if (!NT_SUCCESS(status)) {
        WdfRequestComplete(Request, status);
        return;
    }

    //
    // Forward the request down. WdfDeviceGetIoTarget returns
    // the default target, which represents the device attached to us below in
    // the stack.
    //

    if (forwardWithCompletionRoutine) {

        //
        // Format the request with the output memory so the completion routine
        // can access the return data in order to cache it into the context area
        //

        status = WdfRequestRetrieveOutputMemory(Request, &outputMemory);

        if (!NT_SUCCESS(status)) {
            DebugPrint(("WdfRequestRetrieveOutputMemory failed: 0x%x\n", status));
            WdfRequestComplete(Request, status);
            return;
        }

        status = WdfIoTargetFormatRequestForInternalIoctl(WdfDeviceGetIoTarget(hDevice),
                                                         Request,
                                                         IoControlCode,
                                                         NULL,
                                                         NULL,
                                                         outputMemory,
                                                         NULL);

        if (!NT_SUCCESS(status)) {
            DebugPrint(("WdfIoTargetFormatRequestForInternalIoctl failed: 0x%x\n", status));
            WdfRequestComplete(Request, status);
            return;
        }

        //
        // Set our completion routine with a context area that we will save
        // the output data into
        //
        WdfRequestSetCompletionRoutine(Request,
                                    KbFilterRequestCompletionRoutine,
                                    completionContext);

        ret = WdfRequestSend(Request,
                             WdfDeviceGetIoTarget(hDevice),
                             WDF_NO_SEND_OPTIONS);

        if (ret == FALSE) {
            status = WdfRequestGetStatus (Request);
            DebugPrint( ("WdfRequestSend failed: 0x%x\n", status));
            WdfRequestComplete(Request, status);
        }

    }
    else
    {

        //
        // We are not interested in post processing the IRP so
        // fire and forget.
        //
        WDF_REQUEST_SEND_OPTIONS_INIT(&options,
                                      WDF_REQUEST_SEND_OPTION_SEND_AND_FORGET);

        ret = WdfRequestSend(Request, WdfDeviceGetIoTarget(hDevice), &options);

        if (ret == FALSE) {
            status = WdfRequestGetStatus (Request);
            DebugPrint(("WdfRequestSend failed: 0x%x\n", status));
            WdfRequestComplete(Request, status);
        }

    }

    return;
}

NTSTATUS
KbFilter_InitializationRoutine(
    IN PVOID                           InitializationContext,
    IN PVOID                           SynchFuncContext,
    IN PI8042_SYNCH_READ_PORT          ReadPort,
    IN PI8042_SYNCH_WRITE_PORT         WritePort,
    OUT PBOOLEAN                       TurnTranslationOn
    )
/*++

Routine Description:

    This routine gets called after the following has been performed on the kb
    1)  a reset
    2)  set the typematic
    3)  set the LEDs

    i8042prt specific code, if you are writing a packet only filter driver, you
    can remove this function

Arguments:

    DeviceObject - Context passed during IOCTL_INTERNAL_I8042_HOOK_KEYBOARD

    SynchFuncContext - Context to pass when calling Read/WritePort

    Read/WritePort - Functions to synchronoulsy read and write to the kb

    TurnTranslationOn - If TRUE when this function returns, i8042prt will not
                        turn on translation on the keyboard

Return Value:

    Status is returned.

--*/
{
    PDEVICE_EXTENSION   devExt;
    NTSTATUS            status = STATUS_SUCCESS;

    devExt = (PDEVICE_EXTENSION)InitializationContext;

    //
    // Do any interesting processing here.  We just call any other drivers
    // in the chain if they exist.  Make sure Translation is turned on as well
    //
    if (devExt->UpperInitializationRoutine) {
        status = (*devExt->UpperInitializationRoutine) (
                        devExt->UpperContext,
                        SynchFuncContext,
                        ReadPort,
                        WritePort,
                        TurnTranslationOn
                        );

        if (!NT_SUCCESS(status)) {
            return status;
        }
    }

    *TurnTranslationOn = TRUE;
    return status;
}

BOOLEAN
KbFilter_IsrHook(
    PVOID                  IsrContext,
    PKEYBOARD_INPUT_DATA   CurrentInput,
    POUTPUT_PACKET         CurrentOutput,
    UCHAR                  StatusByte,
    PUCHAR                 DataByte,
    PBOOLEAN               ContinueProcessing,
    PKEYBOARD_SCAN_STATE   ScanState
    )
/*++

Routine Description:

    This routine gets called at the beginning of processing of the kb interrupt.

    i8042prt specific code, if you are writing a packet only filter driver, you
    can remove this function

Arguments:

    DeviceObject - Our context passed during IOCTL_INTERNAL_I8042_HOOK_KEYBOARD

    CurrentInput - Current input packet being formulated by processing all the
                    interrupts

    CurrentOutput - Current list of bytes being written to the keyboard or the
                    i8042 port.

    StatusByte    - Byte read from I/O port 60 when the interrupt occurred

    DataByte      - Byte read from I/O port 64 when the interrupt occurred.
                    This value can be modified and i8042prt will use this value
                    if ContinueProcessing is TRUE

    ContinueProcessing - If TRUE, i8042prt will proceed with normal processing of
                         the interrupt.  If FALSE, i8042prt will return from the
                         interrupt after this function returns.  Also, if FALSE,
                         it is this functions responsibilityt to report the input
                         packet via the function provided in the hook IOCTL or via
                         queueing a DPC within this driver and calling the
                         service callback function acquired from the connect IOCTL

Return Value:

    Status is returned.

--*/
{
    PDEVICE_EXTENSION devExt;
    BOOLEAN           retVal = TRUE;

    devExt = (PDEVICE_EXTENSION)IsrContext;

    if (devExt->UpperIsrHook) {
        retVal = (*devExt->UpperIsrHook) (
                        devExt->UpperContext,
                        CurrentInput,
                        CurrentOutput,
                        StatusByte,
                        DataByte,
                        ContinueProcessing,
                        ScanState
                        );

        if (!retVal || !(*ContinueProcessing)) {
            return retVal;
        }
    }

    *ContinueProcessing = TRUE;
    return retVal;
}

BOOLEAN
startTimer(WDFTIMER timerHandle, int miliseconds)
{
  LONGLONG DueTime = - miliseconds * 1000 * 10; // 10 microseconds, 1000 miliseconds , 100 of them.
  // negative: relative to now.

  BOOLEAN already = WdfTimerStart(timerHandle, DueTime);
  if (already) {
      KdPrint(("timer start in %ld: %s\n", DueTime, already?"already":"new"));
  }
  return already;
}

void pass_event(const extendedEvent& pevent, WDFDEVICE hDevice)
{
    PDEVICE_EXTENSION devExt = FilterGetData(hDevice);
    KEYBOARD_INPUT_DATA event;

    extended_event_to_win(pevent, event);
    ULONG InputDataConsumed = 0;

    (*(PSERVICE_CALLBACK_ROUTINE)(ULONG_PTR) devExt->UpperConnectData.ClassService)(
        devExt->UpperConnectData.ClassDeviceObject,
        &event,
        &event + 1,
        &InputDataConsumed);
}

void accept_time(long time, PDEVICE_EXTENSION devExt)
{
    auto *forking_machine = (machineRec*) devExt->machine;

    int timeout = forking_machine->accept_time(time);

    if (timeout != 0) {
        startTimer(devExt->timerHandle, timeout);
    } else {
        // can I stop an already stopped?
        WdfTimerStop(devExt->timerHandle, FALSE);
    }
}


void accept_event(PKEYBOARD_INPUT_DATA event, PDEVICE_EXTENSION devExt)
{
    auto *forking_machine = (machineRec*) devExt->machine;

    if (event->Flags & (KEY_E0 | KEY_E1)) {
        DebugPrint(("%s we see E0/E1 flag %u\n", __func__, event->Flags));
    }

    if (event->Flags > 1) {
        KdPrint(("%s we lose information about flags %u\n", __func__, event->Flags));
    }

    extendedEvent ev;
    win_event_to_extended(*event, ev, current_time_miliseconds());

    // KdPrint(("%s passing \n", __func__));
    int timeout = forking_machine->accept_event(ev);
    if (timeout != 0) {
        startTimer(devExt->timerHandle, timeout);
    } else {
        // can I stop an already stopped?
        WdfTimerStop(devExt->timerHandle, FALSE);
    }
}


VOID
KbFilter_ServiceCallback(
    IN PDEVICE_OBJECT  DeviceObject,
    IN PKEYBOARD_INPUT_DATA InputDataStart,
    IN PKEYBOARD_INPUT_DATA InputDataEnd,
    IN OUT PULONG InputDataConsumed
    )
/*++

Routine Description:

    Called when there are keyboard packets to report to the Win32 subsystem.
    You can do anything you like to the packets.  For instance:

    o Drop a packet altogether
    o Mutate the contents of a packet
    o Insert packets into the stream

Arguments:

    DeviceObject - Context passed during the connect IOCTL

    InputDataStart - First packet to be reported

    InputDataEnd - One past the last packet to be reported.  Total number of
                   packets is equal to InputDataEnd - InputDataStart

    InputDataConsumed - Set to the total number of packets consumed by the RIT
                        (via the function pointer we replaced in the connect
                        IOCTL)

Return Value:

    Status is returned.

--*/
{
    PDEVICE_EXTENSION   devExt;
    WDFDEVICE   hDevice;

    hDevice = WdfWdmDeviceGetWdfDeviceHandle(DeviceObject);

    devExt = FilterGetData(hDevice);

    for (PKEYBOARD_INPUT_DATA event = InputDataStart; event != InputDataEnd; event++) {
        accept_event(event, devExt);
    }

    *InputDataConsumed = (ULONG) (InputDataEnd - InputDataStart);
}

VOID
KbFilterRequestCompletionRoutine(
    WDFREQUEST                  Request,
    WDFIOTARGET                 Target,
    PWDF_REQUEST_COMPLETION_PARAMS CompletionParams,
    WDFCONTEXT                  Context
   )
/*++

Routine Description:

    Completion Routine

Arguments:

    Target - Target handle
    Request - Request handle
    Params - request completion params
    Context - Driver supplied context


Return Value:

    VOID

--*/
{
    WDFMEMORY   buffer = CompletionParams->Parameters.Ioctl.Output.Buffer;
    NTSTATUS    status = CompletionParams->IoStatus.Status;

    UNREFERENCED_PARAMETER(Target);

    //
    // Save the keyboard attributes in our context area so that we can return
    // them to the app later.
    //
    if (NT_SUCCESS(status) &&
        CompletionParams->Type == WdfRequestTypeDeviceControlInternal &&
        CompletionParams->Parameters.Ioctl.IoControlCode == IOCTL_KEYBOARD_QUERY_ATTRIBUTES) {

        if( CompletionParams->Parameters.Ioctl.Output.Length >= sizeof(KEYBOARD_ATTRIBUTES)) {

            status = WdfMemoryCopyToBuffer(buffer,
                                           CompletionParams->Parameters.Ioctl.Output.Offset,
                                           &((PDEVICE_EXTENSION)Context)->KeyboardAttributes,
                                            sizeof(KEYBOARD_ATTRIBUTES)
                                          );
        }
    }

    WdfRequestComplete(Request, status);

    return;
}


