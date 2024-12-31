/*++

Copyright (c) Microsoft Corporation.  All rights reserved.

    THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY
    KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
    IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR
    PURPOSE.

Module Name:

    KBFTEST.C

Abstract:


Environment:

    usermode console application

--*/


#include <basetyps.h>
#include <stdlib.h>
#include <wtypes.h>
#include <initguid.h>
#include <stdio.h>
#include <string.h>
#include <conio.h>
#include <ntddkbd.h>

#pragma warning(disable:4201)

#include <setupapi.h>
#include <winioctl.h>

#pragma warning(default:4201)

#include "..\sys\public.h"

#include <array>
#include "fork_enums.h"


//-----------------------------------------------------------------------------
// 4127 -- Conditional Expression is Constant warning
//-----------------------------------------------------------------------------
#define WHILE(constant) \
__pragma(warning(disable: 4127)) while(constant); __pragma(warning(default: 4127))

DEFINE_GUID(GUID_DEVINTERFACE_KBFILTER,
0x3fb7299d, 0x6847, 0x4490, 0xb0, 0xc9, 0x99, 0xe0, 0x98, 0x6a, 0xb8, 0x86);
// {3FB7299D-6847-4490-B0C9-99E0986AB886}


bool list_kbfilter_devices(HDEVINFO &hardwareDeviceInfo,
                           SP_DEVICE_INTERFACE_DATA *pDeviceInterfaceData,
                           PSP_DEVICE_INTERFACE_DETAIL_DATA &deviceInterfaceDetailData)
{
    ULONG predictedLength = 0;
    ULONG requiredLength = 0;

    printf("\nList of KBFILTER Device Interfaces\n");
    printf("---------------------------------\n");

    int i = 0;

    //
    // Enumerate devices of toaster class
    //

    do {
        if (SetupDiEnumDeviceInterfaces (hardwareDeviceInfo,
                                         0, // No care about specific PDOs
                                         (LPGUID)&GUID_DEVINTERFACE_KBFILTER,
                                         i, //
                                         pDeviceInterfaceData)) {

            if(deviceInterfaceDetailData) {
                free (deviceInterfaceDetailData);
                deviceInterfaceDetailData = NULL;
            }

            //
            // Allocate a function class device data structure to
            // receive the information about this particular device.
            //

            //
            // First find out required length of the buffer
            //

            if(!SetupDiGetDeviceInterfaceDetail (
                    hardwareDeviceInfo,
                    pDeviceInterfaceData,
                    NULL, // probing so no output buffer yet
                    0, // probing so output buffer length of zero
                    &requiredLength,
                    NULL)) { // not interested in the specific dev-node
                if(ERROR_INSUFFICIENT_BUFFER != GetLastError()) {
                    printf("SetupDiGetDeviceInterfaceDetail failed %d\n", GetLastError());
                    SetupDiDestroyDeviceInfoList (hardwareDeviceInfo);
                    return FALSE;
                }

            }

            predictedLength = requiredLength;

            deviceInterfaceDetailData = (PSP_DEVICE_INTERFACE_DETAIL_DATA) malloc(predictedLength);

            if(deviceInterfaceDetailData) {
                deviceInterfaceDetailData->cbSize =
                                sizeof (SP_DEVICE_INTERFACE_DETAIL_DATA);
            } else {
                printf("Couldn't allocate %d bytes for device interface details.\n", predictedLength);
                SetupDiDestroyDeviceInfoList (hardwareDeviceInfo);
                return FALSE;
            }


            if (! SetupDiGetDeviceInterfaceDetail (
                       hardwareDeviceInfo,
                       pDeviceInterfaceData,
                       deviceInterfaceDetailData,
                       predictedLength,
                       &requiredLength,
                       NULL)) {
                printf("Error in SetupDiGetDeviceInterfaceDetail\n");
                SetupDiDestroyDeviceInfoList (hardwareDeviceInfo);
                free (deviceInterfaceDetailData);
                return FALSE;
            }
            printf("%d) %s\n", ++i,
                    deviceInterfaceDetailData->DevicePath);
        }
        else if (ERROR_NO_MORE_ITEMS != GetLastError()) {
            free (deviceInterfaceDetailData);
            deviceInterfaceDetailData = NULL;
            continue;
        }
        else
            break;

    } WHILE (TRUE);

    return TRUE;
}


bool get_keyboard_attributes(HANDLE file)
{
    KEYBOARD_ATTRIBUTES kbdattrib{0};
    ULONG bytes=0;

    //
    // Send an IOCTL to retrive the keyboard attributes
    // These are cached in the kbfiltr
    //

    if (!DeviceIoControl (file,
                          IOCTL_KBFILTR_GET_KEYBOARD_ATTRIBUTES,
                          NULL, 0,
                          &kbdattrib, sizeof(kbdattrib),
                          &bytes, NULL)) {
        printf("Retrieve Keyboard Attributes request failed:0x%x\n", GetLastError());

        return FALSE;
    }

    printf("\nKeyboard Attributes:\n"
           " KeyboardMode:          0x%x\n"
           " NumberOfFunctionKeys:  0x%x\n"
           " NumberOfIndicators:    0x%x\n"
           " NumberOfKeysTotal:     0x%x\n"
           " InputDataQueueLength:  0x%x\n",
           kbdattrib.KeyboardMode,
           kbdattrib.NumberOfFunctionKeys,
           kbdattrib.NumberOfIndicators,
           kbdattrib.NumberOfKeysTotal,
           kbdattrib.InputDataQueueLength);
    return TRUE;
}



// Send an IOCTL to request forking
bool fork_configure(HANDLE file, std::array<int,4> command)
{
    ULONG bytes=0;

    if (!DeviceIoControl (file,
                          IOCTL_KBFILTR_SET_FORK,
                          command.data(), sizeof(command),
                          NULL, 0, // sizeof(kbdattrib)
                          &bytes, NULL)) {
        printf("fork_configure request failed:0x%x\n", GetLastError());

        return FALSE;
    }
    return TRUE;
}

int
_cdecl
main(
    _In_ int argc,
    _In_ char *argv[]
    )
{
    HDEVINFO                            hardwareDeviceInfo;
    SP_DEVICE_INTERFACE_DATA            deviceInterfaceData{0};
    PSP_DEVICE_INTERFACE_DETAIL_DATA    deviceInterfaceDetailData = NULL;
    HANDLE                              file;

    //
    // Open a handle to the device interface information set of all
    // present toaster class interfaces.
    //

    hardwareDeviceInfo = SetupDiGetClassDevs (
                       (LPGUID)&GUID_DEVINTERFACE_KBFILTER,
                       NULL, // Define no enumerator (global)
                       NULL, // Define no
                       (DIGCF_PRESENT | // Only Devices present
                       DIGCF_DEVICEINTERFACE)); // Function class devices.
    if(INVALID_HANDLE_VALUE == hardwareDeviceInfo)
    {
        printf("SetupDiGetClassDevs failed: %x\n", GetLastError());
        return 0;
    }

    deviceInterfaceData.cbSize = sizeof (SP_DEVICE_INTERFACE_DATA);

    // this can assign to deviceInterfaceDetailData
    if (FALSE == list_kbfilter_devices(hardwareDeviceInfo, &deviceInterfaceData, deviceInterfaceDetailData))
        // strange way of exiting:
        return FALSE;


    SetupDiDestroyDeviceInfoList(hardwareDeviceInfo);

    if(!deviceInterfaceDetailData)
    {
        printf("No device interfaces present\n");
        return 0;
    }

    //
    // Open the last kbfilter device interface
    //

    printf("\nOpening the last interface:\n %s\n",
                    deviceInterfaceDetailData->DevicePath);

    file = CreateFile ( deviceInterfaceDetailData->DevicePath,
                        GENERIC_READ | GENERIC_WRITE,
                        0,
                        NULL, // no SECURITY_ATTRIBUTES structure
                        OPEN_EXISTING, // No special create flags
                        0, // No special attributes
                        NULL);

    if (INVALID_HANDLE_VALUE == file) {
        printf("Error in CreateFile: %x", GetLastError());
        free (deviceInterfaceDetailData);
        return 0;
    }

    if (argc == 3) {
        int keycode = atoi(argv[1]);
        int fork = atoi(argv[2]);

        if (keycode && fork)
            fork_configure(file,
                           std::array<int,4> {
                             fork_configure_key_fork << 2 | fork_LOCAL_OPTION,
                             // code, fork
                             keycode, fork,
                             // set
                             1
                           });
    }
#if 0
    if (get_keyboard_attributes(file) == FALSE) {
        free (deviceInterfaceDetailData);
        CloseHandle(file);
        return 0;
    }

    free (deviceInterfaceDetailData);
    CloseHandle(file);
#endif
    return 0;
}



