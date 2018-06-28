//========================================================================
// GLFW - An OpenGL framework
// File:        macosx_joystick.c
// Platform:    Mac OS X
// API Version: 2.6
// WWW:         http://glfw.sourceforge.net
//------------------------------------------------------------------------
// Copyright (c) 2002-2006 Camilla Berglund
//
// This software is provided 'as-is', without any express or implied
// warranty. In no event will the authors be held liable for any damages
// arising from the use of this software.
//
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it
// freely, subject to the following restrictions:
//
// 1. The origin of this software must not be misrepresented; you must not
//    claim that you wrote the original software. If you use this software
//    in a product, an acknowledgment in the product documentation would
//    be appreciated but is not required.
//
// 2. Altered source versions must be plainly marked as such, and must not
//    be misrepresented as being the original software.
//
// 3. This notice may not be removed or altered from any source
//    distribution.
//
//========================================================================

/*
    SDL - Simple DirectMedia Layer
    Copyright (C) 1997-2010 Sam Lantinga

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

    Sam Lantinga
    slouken@libsdl.org
*/

#include <unistd.h>
#include <ctype.h>
#include <sysexits.h>
#include <mach/mach.h>
#include <mach/mach_error.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/IOCFPlugIn.h>
#ifdef MACOS_10_0_4
#include <IOKit/hidsystem/IOHIDUsageTables.h>
#else
/* The header was moved here in Mac OS X 10.1 */
#include <Kernel/IOKit/hidsystem/IOHIDUsageTables.h>
#endif

#include <IOKit/hid/IOHIDLib.h>

#include <IOKit/hid/IOHIDKeys.h>
#include <CoreFoundation/CoreFoundation.h>
#include <Carbon/Carbon.h>      /* for NewPtrClear, DisposePtr */


#include "internal.h"
//#include "SDL_sysjoystick_c.h"
// TO DO: use HID manager to implement joystick support.

struct recElement
{
    IOHIDElementCookie cookie;  /* unique value which identifies element, will NOT change */
    long usagePage, usage;      /* HID usage */
    long min;                   /* reported min value possible */
    long max;                   /* reported max value possible */
#if 0
    /* TODO: maybe should handle the following stuff somehow? */

    long scaledMin;             /* reported scaled min value possible */
    long scaledMax;             /* reported scaled max value possible */
    long size;                  /* size in bits of data return from element */
    Boolean relative;           /* are reports relative to last report (deltas) */
    Boolean wrapping;           /* does element wrap around (one value higher than max is min) */
    Boolean nonLinear;          /* are the values reported non-linear relative to element movement */
    Boolean preferredState;     /* does element have a preferred state (such as a button) */
    Boolean nullState;          /* does element have null state */
#endif                          /* 0 */

    /* runtime variables used for auto-calibration */
    long minReport;             /* min returned value */
    long maxReport;             /* max returned value */

    struct recElement *pNext;   /* next element in list */
};
typedef struct recElement recElement;

struct joystick_hwdata
{
    io_service_t ffservice;     /* Interface for force feedback, 0 = no ff */
    io_service_t hid_service;
    IOHIDDeviceInterface **interface;   /* interface to device, NULL = no interface */

    char product[256];          /* name of product */
    long usage;                 /* usage page from IOUSBHID Parser.h which defines general usage */
    long usagePage;             /* usage within above page from IOUSBHID Parser.h which defines specific usage */

    long axes;                  /* number of axis (calculated, not reported by device) */
    long buttons;               /* number of buttons (calculated, not reported by device) */
    long hats;                  /* number of hat switches (calculated, not reported by device) */
    long elements;              /* number of total elements (shouldbe total of above) (calculated, not reported by device) */

    recElement *firstAxis;
    recElement *firstButton;
    recElement *firstHat;

    int removed;
    int uncentered;

    struct joystick_hwdata *pNext;      /* next device */
};
typedef struct joystick_hwdata recDevice;

static recDevice *gpDeviceList = NULL;

static void
HIDReportErrorNum(char *strError, long numError)
{
    printf("%s (%ld)\n", strError, numError);
}

void XSDL_SetError(const char* str)
{
    printf("%s\n", str);
}

static void HIDGetCollectionElements(CFMutableDictionaryRef deviceProperties,
                                     recDevice * pDevice);

/* returns current value for element, polling element
 * will return 0 on error conditions which should be accounted for by application
 */

static SInt32
HIDGetElementValue(recDevice * pDevice, recElement * pElement)
{
    IOReturn result = kIOReturnSuccess;
    IOHIDEventStruct hidEvent;
    hidEvent.value = 0;

    if (NULL != pDevice && NULL != pElement && NULL != pDevice->interface) {
        result =
            (*(pDevice->interface))->getElementValue(pDevice->interface,
                                                     pElement->cookie,
                                                     &hidEvent);
        if (kIOReturnSuccess == result) {
            /* record min and max for auto calibration */
            if (hidEvent.value < pElement->minReport)
                pElement->minReport = hidEvent.value;
            if (hidEvent.value > pElement->maxReport)
                pElement->maxReport = hidEvent.value;
        }
    }

    /* auto user scale */
    return hidEvent.value;
}

static SInt32
HIDScaledCalibratedValue(recDevice * pDevice, recElement * pElement,
                         long min, long max)
{
    float deviceScale = max - min;
    float readScale = pElement->maxReport - pElement->minReport;
    SInt32 value = HIDGetElementValue(pDevice, pElement);
    if (readScale == 0)
        return value;           /* no scaling at all */
    else
        return ((value - pElement->minReport) * deviceScale / readScale) +
            min;
}


static void
HIDRemovalCallback(void *target, IOReturn result, void *refcon, void *sender)
{
    recDevice *device = (recDevice *) refcon;
    device->removed = 1;
    device->uncentered = 1;
}



/* Create and open an interface to device, required prior to extracting values or building queues.
 * Note: appliction now owns the device and must close and release it prior to exiting
 */

static IOReturn
HIDCreateOpenDeviceInterface(io_object_t hidDevice, recDevice * pDevice)
{
    IOReturn result = kIOReturnSuccess;
    HRESULT plugInResult = S_OK;
    SInt32 score = 0;
    IOCFPlugInInterface **ppPlugInInterface = NULL;

    if (NULL == pDevice->interface) {
        result =
            IOCreatePlugInInterfaceForService(hidDevice,
                                              kIOHIDDeviceUserClientTypeID,
                                              kIOCFPlugInInterfaceID,
                                              &ppPlugInInterface, &score);
        if (kIOReturnSuccess == result) {
            /* Call a method of the intermediate plug-in to create the device interface */
            plugInResult =
                (*ppPlugInInterface)->QueryInterface(ppPlugInInterface,
                                                     CFUUIDGetUUIDBytes
                                                     (kIOHIDDeviceInterfaceID),
                                                     (void *)
                                                     &(pDevice->interface));
            if (S_OK != plugInResult)
                HIDReportErrorNum
                    ("Couldn't query HID class device interface from plugInInterface",
                     plugInResult);
            (*ppPlugInInterface)->Release(ppPlugInInterface);
        } else
            HIDReportErrorNum
                ("Failed to create **plugInInterface via IOCreatePlugInInterfaceForService.",
                 result);
    }
    if (NULL != pDevice->interface) {
        result = (*(pDevice->interface))->open(pDevice->interface, 0);
        if (kIOReturnSuccess != result)
            HIDReportErrorNum
                ("Failed to open pDevice->interface via open.", result);
        else
            (*(pDevice->interface))->setRemovalCallback(pDevice->interface,
                                                        HIDRemovalCallback,
                                                        pDevice, pDevice);

    }
    return result;
}

/* Closes and releases interface to device, should be done prior to exting application
 * Note: will have no affect if device or interface do not exist
 * application will "own" the device if interface is not closed
 * (device may have to be plug and re-plugged in different location to get it working again without a restart)
 */

static IOReturn
HIDCloseReleaseInterface(recDevice * pDevice)
{
    IOReturn result = kIOReturnSuccess;

    if ((NULL != pDevice) && (NULL != pDevice->interface)) {
        /* close the interface */
        result = (*(pDevice->interface))->close(pDevice->interface);
        if (kIOReturnNotOpen == result) {
            /* do nothing as device was not opened, thus can't be closed */
        } else if (kIOReturnSuccess != result)
            HIDReportErrorNum("Failed to close IOHIDDeviceInterface.",
                              result);
        /* release the interface */
        result = (*(pDevice->interface))->Release(pDevice->interface);
        if (kIOReturnSuccess != result)
            HIDReportErrorNum("Failed to release IOHIDDeviceInterface.",
                              result);
        pDevice->interface = NULL;
    }
    return result;
}

/* extracts actual specific element information from each element CF dictionary entry */

static void
HIDGetElementInfo(CFTypeRef refElement, recElement * pElement)
{
    long number;
    CFTypeRef refType;

    refType = CFDictionaryGetValue(refElement, CFSTR(kIOHIDElementCookieKey));
    if (refType && CFNumberGetValue(refType, kCFNumberLongType, &number))
        pElement->cookie = (IOHIDElementCookie) number;
    refType = CFDictionaryGetValue(refElement, CFSTR(kIOHIDElementMinKey));
    if (refType && CFNumberGetValue(refType, kCFNumberLongType, &number))
        pElement->minReport = pElement->min = number;
    pElement->maxReport = pElement->min;
    refType = CFDictionaryGetValue(refElement, CFSTR(kIOHIDElementMaxKey));
    if (refType && CFNumberGetValue(refType, kCFNumberLongType, &number))
        pElement->maxReport = pElement->max = number;
/*
    TODO: maybe should handle the following stuff somehow?

    refType = CFDictionaryGetValue (refElement, CFSTR(kIOHIDElementScaledMinKey));
    if (refType && CFNumberGetValue (refType, kCFNumberLongType, &number))
        pElement->scaledMin = number;
    refType = CFDictionaryGetValue (refElement, CFSTR(kIOHIDElementScaledMaxKey));
    if (refType && CFNumberGetValue (refType, kCFNumberLongType, &number))
        pElement->scaledMax = number;
    refType = CFDictionaryGetValue (refElement, CFSTR(kIOHIDElementSizeKey));
    if (refType && CFNumberGetValue (refType, kCFNumberLongType, &number))
        pElement->size = number;
    refType = CFDictionaryGetValue (refElement, CFSTR(kIOHIDElementIsRelativeKey));
    if (refType)
        pElement->relative = CFBooleanGetValue (refType);
    refType = CFDictionaryGetValue (refElement, CFSTR(kIOHIDElementIsWrappingKey));
    if (refType)
        pElement->wrapping = CFBooleanGetValue (refType);
    refType = CFDictionaryGetValue (refElement, CFSTR(kIOHIDElementIsNonLinearKey));
    if (refType)
        pElement->nonLinear = CFBooleanGetValue (refType);
    refType = CFDictionaryGetValue (refElement, CFSTR(kIOHIDElementHasPreferedStateKey));
    if (refType)
        pElement->preferredState = CFBooleanGetValue (refType);
    refType = CFDictionaryGetValue (refElement, CFSTR(kIOHIDElementHasNullStateKey));
    if (refType)
        pElement->nullState = CFBooleanGetValue (refType);
*/
}

/* examines CF dictionary vlaue in device element hierarchy to determine if it is element of interest or a collection of more elements
 * if element of interest allocate storage, add to list and retrieve element specific info
 * if collection then pass on to deconstruction collection into additional individual elements
 */

static void
HIDAddElement(CFTypeRef refElement, recDevice * pDevice)
{
    recElement *element = NULL;
    recElement **headElement = NULL;
    long elementType, usagePage, usage;
    CFTypeRef refElementType =
        CFDictionaryGetValue(refElement, CFSTR(kIOHIDElementTypeKey));
    CFTypeRef refUsagePage =
        CFDictionaryGetValue(refElement, CFSTR(kIOHIDElementUsagePageKey));
    CFTypeRef refUsage =
        CFDictionaryGetValue(refElement, CFSTR(kIOHIDElementUsageKey));


    if ((refElementType)
        &&
        (CFNumberGetValue(refElementType, kCFNumberLongType, &elementType))) {
        /* look at types of interest */
        if ((elementType == kIOHIDElementTypeInput_Misc)
            || (elementType == kIOHIDElementTypeInput_Button)
            || (elementType == kIOHIDElementTypeInput_Axis)) {
            if (refUsagePage
                && CFNumberGetValue(refUsagePage, kCFNumberLongType,
                                    &usagePage) && refUsage
                && CFNumberGetValue(refUsage, kCFNumberLongType, &usage)) {
                switch (usagePage) {    /* only interested in kHIDPage_GenericDesktop and kHIDPage_Button */
                case kHIDPage_GenericDesktop:
                    {
                        switch (usage) {        /* look at usage to determine function */
                        case kHIDUsage_GD_X:
                        case kHIDUsage_GD_Y:
                        case kHIDUsage_GD_Z:
                        case kHIDUsage_GD_Rx:
                        case kHIDUsage_GD_Ry:
                        case kHIDUsage_GD_Rz:
                        case kHIDUsage_GD_Slider:
                        case kHIDUsage_GD_Dial:
                        case kHIDUsage_GD_Wheel:
                            element = (recElement *)
                                NewPtrClear(sizeof(recElement));
                            if (element) {
                                pDevice->axes++;
                                headElement = &(pDevice->firstAxis);
                            }
                            break;
                        case kHIDUsage_GD_Hatswitch:
                            element = (recElement *)
                                NewPtrClear(sizeof(recElement));
                            if (element) {
                                pDevice->hats++;
                                headElement = &(pDevice->firstHat);
                            }
                            break;
                        }
                    }
                    break;
                case kHIDPage_Button:
                    element = (recElement *)
                        NewPtrClear(sizeof(recElement));
                    if (element) {
                        pDevice->buttons++;
                        headElement = &(pDevice->firstButton);
                    }
                    break;
                default:
                    break;
                }
            }
        } else if (kIOHIDElementTypeCollection == elementType)
            HIDGetCollectionElements((CFMutableDictionaryRef) refElement,
                                     pDevice);
    }

    if (element && headElement) {       /* add to list */
        recElement *elementPrevious = NULL;
        recElement *elementCurrent = *headElement;
        while (elementCurrent && usage >= elementCurrent->usage) {
            elementPrevious = elementCurrent;
            elementCurrent = elementCurrent->pNext;
        }
        if (elementPrevious) {
            elementPrevious->pNext = element;
        } else {
            *headElement = element;
        }
        element->usagePage = usagePage;
        element->usage = usage;
        element->pNext = elementCurrent;
        HIDGetElementInfo(refElement, element);
        pDevice->elements++;
    }
}

/* collects information from each array member in device element list (each array memeber = element) */

static void
HIDGetElementsCFArrayHandler(const void *value, void *parameter)
{
    if (CFGetTypeID(value) == CFDictionaryGetTypeID())
        HIDAddElement((CFTypeRef) value, (recDevice *) parameter);
}

/* handles retrieval of element information from arrays of elements in device IO registry information */

static void
HIDGetElements(CFTypeRef refElementCurrent, recDevice * pDevice)
{
    CFTypeID type = CFGetTypeID(refElementCurrent);
    if (type == CFArrayGetTypeID()) {   /* if element is an array */
        CFRange range = { 0, CFArrayGetCount(refElementCurrent) };
        /* CountElementsCFArrayHandler called for each array member */
        CFArrayApplyFunction(refElementCurrent, range,
                             HIDGetElementsCFArrayHandler, pDevice);
    }
}

/* handles extracting element information from element collection CF types
 * used from top level element decoding and hierarchy deconstruction to flatten device element list
 */

static void
HIDGetCollectionElements(CFMutableDictionaryRef deviceProperties,
                         recDevice * pDevice)
{
    CFTypeRef refElementTop =
        CFDictionaryGetValue(deviceProperties, CFSTR(kIOHIDElementKey));
    if (refElementTop)
        HIDGetElements(refElementTop, pDevice);
}

/* use top level element usage page and usage to discern device usage page and usage setting appropriate vlaues in device record */

static void
HIDTopLevelElementHandler(const void *value, void *parameter)
{
    CFTypeRef refCF = 0;
    if (CFGetTypeID(value) != CFDictionaryGetTypeID())
        return;
    refCF = CFDictionaryGetValue(value, CFSTR(kIOHIDElementUsagePageKey));
    if (!CFNumberGetValue
        (refCF, kCFNumberLongType, &((recDevice *) parameter)->usagePage))
        XSDL_SetError("CFNumberGetValue error retrieving pDevice->usagePage.");
    refCF = CFDictionaryGetValue(value, CFSTR(kIOHIDElementUsageKey));
    if (!CFNumberGetValue
        (refCF, kCFNumberLongType, &((recDevice *) parameter)->usage))
        XSDL_SetError("CFNumberGetValue error retrieving pDevice->usage.");
}

/* extracts device info from CF dictionary records in IO registry */

static void
HIDGetDeviceInfo(io_object_t hidDevice, CFMutableDictionaryRef hidProperties,
                 recDevice * pDevice)
{
    CFMutableDictionaryRef usbProperties = 0;
    io_registry_entry_t parent1, parent2;

    /* Mac OS X currently is not mirroring all USB properties to HID page so need to look at USB device page also
     * get dictionary for usb properties: step up two levels and get CF dictionary for USB properties
     */
    if ((KERN_SUCCESS ==
         IORegistryEntryGetParentEntry(hidDevice, kIOServicePlane, &parent1))
        && (KERN_SUCCESS ==
            IORegistryEntryGetParentEntry(parent1, kIOServicePlane, &parent2))
        && (KERN_SUCCESS ==
            IORegistryEntryCreateCFProperties(parent2, &usbProperties,
                                              kCFAllocatorDefault,
                                              kNilOptions))) {
        if (usbProperties) {
            CFTypeRef refCF = 0;
            /* get device info
             * try hid dictionary first, if fail then go to usb dictionary
             */


            /* get product name */
            refCF =
                CFDictionaryGetValue(hidProperties, CFSTR(kIOHIDProductKey));
            if (!refCF)
                refCF =
                    CFDictionaryGetValue(usbProperties,
                                         CFSTR("USB Product Name"));
            if (refCF) {
                if (!CFStringGetCString
                    (refCF, pDevice->product, 256,
                     CFStringGetSystemEncoding()))
                    XSDL_SetError
                        ("CFStringGetCString error retrieving pDevice->product.");
            }

            /* get usage page and usage */
            refCF =
                CFDictionaryGetValue(hidProperties,
                                     CFSTR(kIOHIDPrimaryUsagePageKey));
            if (refCF) {
                if (!CFNumberGetValue
                    (refCF, kCFNumberLongType, &pDevice->usagePage))
                    XSDL_SetError
                        ("CFNumberGetValue error retrieving pDevice->usagePage.");
                refCF =
                    CFDictionaryGetValue(hidProperties,
                                         CFSTR(kIOHIDPrimaryUsageKey));
                if (refCF)
                    if (!CFNumberGetValue
                        (refCF, kCFNumberLongType, &pDevice->usage))
                        XSDL_SetError
                            ("CFNumberGetValue error retrieving pDevice->usage.");
            }

            if (NULL == refCF) {        /* get top level element HID usage page or usage */
                /* use top level element instead */
                CFTypeRef refCFTopElement = 0;
                refCFTopElement =
                    CFDictionaryGetValue(hidProperties,
                                         CFSTR(kIOHIDElementKey));
                {
                    /* refCFTopElement points to an array of element dictionaries */
                    CFRange range = { 0, CFArrayGetCount(refCFTopElement) };
                    CFArrayApplyFunction(refCFTopElement, range,
                                         HIDTopLevelElementHandler, pDevice);
                }
            }

            CFRelease(usbProperties);
        } else
            XSDL_SetError
                ("IORegistryEntryCreateCFProperties failed to create usbProperties.");

        if (kIOReturnSuccess != IOObjectRelease(parent2))
            XSDL_SetError("IOObjectRelease error with parent2.");
        if (kIOReturnSuccess != IOObjectRelease(parent1))
            XSDL_SetError("IOObjectRelease error with parent1.");
    }
}


static recDevice *
HIDBuildDevice(io_object_t hidDevice)
{
    recDevice *pDevice = (recDevice *) NewPtrClear(sizeof(recDevice));
    if (pDevice) {
        /* get dictionary for HID properties */
        CFMutableDictionaryRef hidProperties = 0;
        kern_return_t result =
            IORegistryEntryCreateCFProperties(hidDevice, &hidProperties,
                                              kCFAllocatorDefault,
                                              kNilOptions);
        if ((result == KERN_SUCCESS) && hidProperties) {
            /* create device interface */
            result = HIDCreateOpenDeviceInterface(hidDevice, pDevice);
            if (kIOReturnSuccess == result) {
                HIDGetDeviceInfo(hidDevice, hidProperties, pDevice);    /* hidDevice used to find parents in registry tree */
                HIDGetCollectionElements(hidProperties, pDevice);
            } else {
                DisposePtr((Ptr) pDevice);
                pDevice = NULL;
            }
            CFRelease(hidProperties);
        } else {
            DisposePtr((Ptr) pDevice);
            pDevice = NULL;
        }
    }
    return pDevice;
}

/* disposes of the element list associated with a device and the memory associated with the list
 */

static void
HIDDisposeElementList(recElement ** elementList)
{
    recElement *pElement = *elementList;
    while (pElement) {
        recElement *pElementNext = pElement->pNext;
        DisposePtr((Ptr) pElement);
        pElement = pElementNext;
    }
    *elementList = NULL;
}

/* disposes of a single device, closing and releaseing interface, freeing memory fro device and elements, setting device pointer to NULL
 * all your device no longer belong to us... (i.e., you do not 'own' the device anymore)
 */

static recDevice *
HIDDisposeDevice(recDevice ** ppDevice)
{
    kern_return_t result = KERN_SUCCESS;
    recDevice *pDeviceNext = NULL;
    if (*ppDevice) {
        /* save next device prior to disposing of this device */
        pDeviceNext = (*ppDevice)->pNext;

        /* free posible io_service_t */
        if ((*ppDevice)->ffservice) {
            IOObjectRelease((*ppDevice)->ffservice);
            (*ppDevice)->ffservice = 0;
        }

        /* free element lists */
        HIDDisposeElementList(&(*ppDevice)->firstAxis);
        HIDDisposeElementList(&(*ppDevice)->firstButton);
        HIDDisposeElementList(&(*ppDevice)->firstHat);

        result = HIDCloseReleaseInterface(*ppDevice);   /* function sanity checks interface value (now application does not own device) */
        if (kIOReturnSuccess != result)
            HIDReportErrorNum
                ("HIDCloseReleaseInterface failed when trying to dipose device.",
                 result);
        DisposePtr((Ptr) * ppDevice);
        *ppDevice = NULL;
    }
    return pDeviceNext;
}

static recDevice* findGamepadByIOObj(io_object_t io_obj) {
    recDevice* device = gpDeviceList;
    int i = 0;
    while (device) {
        if (device->hid_service == io_obj) {
            return device;
        }
        device = device->pNext;
    }
    return NULL;
}

static recDevice* findLastGamepadByIOObj(io_object_t io_obj) {

    recDevice* lastDevice = gpDeviceList;
    while (lastDevice && lastDevice->pNext) {
        lastDevice = lastDevice->pNext;
    }

    return lastDevice;
}


static int removeGamepadByIOObj(io_object_t io_obj) {
    recDevice* device = findGamepadByIOObj(io_obj);
    if (device) {
        // locate device in joy list
        int found = -1;
        for( int i = 0; i <= GLFW_JOYSTICK_LAST; ++ i )
        {
            if (_glfwJoy[i].Device == device) {
                _glfwJoy[i].Device = NULL;
                _glfwJoy[i].Present = GL_FALSE;

                if (_glfwJoy[i].Axis)
                    free(_glfwJoy[i].Axis);

                if (_glfwJoy[i].Button)
                    free(_glfwJoy[i].Button);

                _glfwJoy[i].Axis = 0;
                _glfwJoy[i].Button = 0;

                found = i;
                break;
            }
        }

        recDevice* d = gpDeviceList;
        recDevice* prev_d = NULL;
        while (d) {
            if (d->hid_service == io_obj) {
                if (prev_d) {
                    prev_d->pNext = d->pNext;
                } else {
                    gpDeviceList = d->pNext;
                }
                HIDDisposeDevice(&d);
                break;
            }
            prev_d = d;
            d = d->pNext;
        }
        // HIDDisposeDevice(&device);

        return found;
    }
    return -1;
}

static void gamepadWasAdded(void* inContext, IOReturn inResult, void* inSender, IOHIDDeviceRef device) {
    io_service_t ioservice = IOHIDDeviceGetService(device);

    // The device might already been added in the init step
    recDevice* new_device = HIDBuildDevice(ioservice);
    if (!new_device)
        return;

    new_device->ffservice = 0;
    new_device->hid_service = ioservice;

    /* Add device to the end of the list */
    recDevice* lastDevice = findLastGamepadByIOObj(ioservice);
    if (lastDevice)
        lastDevice->pNext = new_device;
    else
        gpDeviceList = new_device;

    // locate device in joy list
    int found = -1;
    for( int i = 0; i <= GLFW_JOYSTICK_LAST; ++ i )
    {
        if (!_glfwJoy[i].Present) {
            _glfwJoy[i].Device = new_device;
            _glfwJoy[i].Present = GL_TRUE;

            _glfwJoy[i].NumAxes = new_device->axes;
            _glfwJoy[i].Axis = malloc(sizeof(float) * new_device->axes);

            _glfwJoy[i].NumButtons = new_device->buttons;
            _glfwJoy[i].Button = malloc(sizeof(char) * new_device->buttons);
            found = i;
            break;
        }
    }

    if (found >= 0 && _glfwWin.gamepadCallback) {
        _glfwWin.gamepadCallback(found, 1);
    }
}

static void gamepadWasRemoved(void* inContext, IOReturn inResult, void* inSender, IOHIDDeviceRef device) {
    io_service_t ioservice = IOHIDDeviceGetService(device);

    int i = removeGamepadByIOObj(ioservice);
    if (i >= 0 && _glfwWin.gamepadCallback) {
        _glfwWin.gamepadCallback(i, 0);
    }
}

int XSDL_numjoysticks = 0;
IOHIDManagerRef gHidManager = nil;

int XSDL_SYS_JoystickInit(void)
{
    IOReturn result = kIOReturnSuccess;
    mach_port_t masterPort = 0;
    io_iterator_t hidObjectIterator = 0;
    CFMutableDictionaryRef hidMatchDictionary = NULL;
    recDevice *device, *lastDevice;
    io_object_t ioHIDDeviceObject = 0;

    XSDL_numjoysticks = 0;

    // HACK andsve
    gHidManager = IOHIDManagerCreate(kCFAllocatorDefault, kIOHIDOptionsTypeNone);

    NSMutableArray *criterionArray = [NSMutableArray arrayWithCapacity:3];
    {
        // NSMutableDictionary* criterion = [[NSMutableDictionary alloc] init];
        NSMutableDictionary* criterion = [NSMutableDictionary dictionaryWithCapacity: 2];
            [criterion setObject: [NSNumber numberWithInt: kHIDPage_GenericDesktop]
                          forKey: (NSString*)CFSTR(kIOHIDDeviceUsagePageKey)];
            [criterion setObject: [NSNumber numberWithInt: kHIDUsage_GD_Joystick]
                          forKey: (NSString*)CFSTR(kIOHIDDeviceUsageKey)];
        [criterionArray addObject:criterion];
    }
    {
        // NSMutableDictionary* criterion = [[NSMutableDictionary alloc] init];
        NSMutableDictionary* criterion = [NSMutableDictionary dictionaryWithCapacity: 2];
            [criterion setObject: [NSNumber numberWithInt: kHIDPage_GenericDesktop]
                          forKey: (NSString*)CFSTR(kIOHIDDeviceUsagePageKey)];
            [criterion setObject: [NSNumber numberWithInt: kHIDUsage_GD_GamePad]
                          forKey: (NSString*)CFSTR(kIOHIDDeviceUsageKey)];
        [criterionArray addObject:criterion];
    }
    {
        // NSMutableDictionary* criterion = [[NSMutableDictionary alloc] init];
        NSMutableDictionary* criterion = [NSMutableDictionary dictionaryWithCapacity: 2];
            [criterion setObject: [NSNumber numberWithInt: kHIDPage_GenericDesktop]
                          forKey: (NSString*)CFSTR(kIOHIDDeviceUsagePageKey)];
            [criterion setObject: [NSNumber numberWithInt: kHIDUsage_GD_MultiAxisController]
                          forKey: (NSString*)CFSTR(kIOHIDDeviceUsageKey)];
        [criterionArray addObject:criterion];
    }

    IOHIDManagerSetDeviceMatchingMultiple(gHidManager, (__bridge CFArrayRef)criterionArray);
    IOHIDManagerRegisterDeviceMatchingCallback(gHidManager, gamepadWasAdded, NULL);
    IOHIDManagerRegisterDeviceRemovalCallback(gHidManager, gamepadWasRemoved, NULL);

    IOHIDManagerScheduleWithRunLoop(gHidManager, CFRunLoopGetMain(), kCFRunLoopDefaultMode);
    IOHIDManagerOpen(gHidManager, kIOHIDOptionsTypeNone);

    NSLog(@"registered gamepad callbacks");

    return XSDL_numjoysticks;
}

#if 0
int
XSDL_PrivateJoystickAxis(SDL_Joystick * joystick, Uint8 axis, Sint16 value)
{
    int posted;

    /* Update internal joystick state */
    joystick->axes[axis] = value;

    /* Post the event, if desired */
    posted = 0;
#if 0
    if (SDL_GetEventState(SDL_JOYAXISMOTION) == SDL_ENABLE) {
        SDL_Event event;
        event.type = SDL_JOYAXISMOTION;
        event.jaxis.which = joystick->index;
        event.jaxis.axis = axis;
        event.jaxis.value = value;
        if ((SDL_EventOK == NULL)
            || (*SDL_EventOK) (SDL_EventOKParam, &event)) {
            posted = 1;
            SDL_PushEvent(&event);
        }
    }
#endif /* !SDL_EVENTS_DISABLED */
    return (posted);
}


int
XSDL_PrivateJoystickHat(SDL_Joystick * joystick, Uint8 hat, Uint8 value)
{
    int posted;

    /* Update internal joystick state */
    joystick->hats[hat] = value;

    /* Post the event, if desired */
    posted = 0;
#if 0
    if (SDL_GetEventState(SDL_JOYHATMOTION) == SDL_ENABLE) {
        SDL_Event event;
        event.jhat.type = SDL_JOYHATMOTION;
        event.jhat.which = joystick->index;
        event.jhat.hat = hat;
        event.jhat.value = value;
        if ((SDL_EventOK == NULL)
            || (*SDL_EventOK) (SDL_EventOKParam, &event)) {
            posted = 1;
            SDL_PushEvent(&event);
        }
    }
#endif /* !SDL_EVENTS_DISABLED */
    return (posted);
}
#endif


/* Function to update the state of a joystick - called as a device poll.
 * This function shouldn't update the joystick structure directly,
 * but instead should call SDL_PrivateJoystick*() to deliver events
 * and update joystick device state.
 */
void
XSDL_SYS_JoystickUpdate(int joy_index)
{
    recDevice *device = (recDevice*) _glfwJoy[joy_index].Device;
    if (!device) {
        return;
    }

    recElement *element;
    SInt32 value, range;
    int i;

    if (device->removed) {      /* device was unplugged; ignore it. */
        if (device->uncentered) {
            device->uncentered = 0;

            /* Tell the app that everything is centered/unpressed... */
            for (i = 0; i < device->axes; i++)
                _glfwJoy[joy_index].Axis[i] = 0;

            for (i = 0; i < device->buttons; i++)
                _glfwJoy[joy_index].Button[i] = GLFW_RELEASE;

            //for (i = 0; i < device->hats; i++)
              //  XSDL_PrivateJoystickHat(joystick, i, SDL_HAT_CENTERED);
        }

        return;
    }

    element = device->firstAxis;
    i = 0;
    while (element) {
        value = HIDScaledCalibratedValue(device, element, -32768, 32767);
        _glfwJoy[joy_index].Axis[i] = value;
        //if (value != joystick->axes[i])
          //  XSDL_PrivateJoystickAxis(joystick, i, value);
        element = element->pNext;
        ++i;
    }

    element = device->firstButton;
    i = 0;
    while (element) {
        value = HIDGetElementValue(device, element);
        if (value > 1)          /* handle pressure-sensitive buttons */
            value = 1;
        _glfwJoy[joy_index].Button[i] = value ? GLFW_PRESS : GLFW_RELEASE;
        //if (value != joystick->buttons[i])
          //  XSDL_PrivateJoystickButton(joystick, i, value);
        element = element->pNext;
        ++i;
    }

#if 0
    element = device->firstHat;
    i = 0;
    while (element) {
        uint8_t pos = 0;

        range = (element->max - element->min + 1);
        value = HIDGetElementValue(device, element) - element->min;
        if (range == 4)         /* 4 position hatswitch - scale up value */
            value *= 2;
        else if (range != 8)    /* Neither a 4 nor 8 positions - fall back to default position (centered) */
            value = -1;
        switch (value) {
        case 0:
            pos = SDL_HAT_UP;
            break;
        case 1:
            pos = SDL_HAT_RIGHTUP;
            break;
        case 2:
            pos = SDL_HAT_RIGHT;
            break;
        case 3:
            pos = SDL_HAT_RIGHTDOWN;
            break;
        case 4:
            pos = SDL_HAT_DOWN;
            break;
        case 5:
            pos = SDL_HAT_LEFTDOWN;
            break;
        case 6:
            pos = SDL_HAT_LEFT;
            break;
        case 7:
            pos = SDL_HAT_LEFTUP;
            break;
        default:
            /* Every other value is mapped to center. We do that because some
             * joysticks use 8 and some 15 for this value, and apparently
             * there are even more variants out there - so we try to be generous.
             */
            pos = SDL_HAT_CENTERED;
            break;
        }
        if (pos != joystick->hats[i])
            SDL_PrivateJoystickHat(joystick, i, pos);
        element = element->pNext;
        ++i;
    }
#endif

    return;
}

int _glfwInitJoysticks( void )
{
    int i;
    // Start by saying that there are no sticks
    for( i = 0; i <= GLFW_JOYSTICK_LAST; ++ i )
    {
        _glfwJoy[ i ].Present = GL_FALSE;
        _glfwJoy[ i ].Device = 0;
    }

    XSDL_SYS_JoystickInit();
    return GL_TRUE;
}

int _glfwTerminateJoysticks()
{
    while (NULL != gpDeviceList)
        gpDeviceList = HIDDisposeDevice(&gpDeviceList);

    IOHIDManagerClose(gHidManager, kIOHIDOptionsTypeNone);

    int i;
    for( i = 0; i <= GLFW_JOYSTICK_LAST; ++ i )
    {
        if (_glfwJoy[i].Axis)
            free(_glfwJoy[i].Axis);

        if (_glfwJoy[i].Button)
            free(_glfwJoy[i].Button);

        _glfwJoy[i].Axis = 0;
        _glfwJoy[i].Button = 0;
    }

    return GL_TRUE;
}

int _glfwPlatformGetJoystickParam( int joy, int param )
{
    if (param == GLFW_PRESENT)
    {
        if (joy < GLFW_JOYSTICK_LAST)
        {
            return _glfwJoy[ joy ].Present;
        }
        else
        {
            return GL_FALSE;
        }
    }
    else if (param == GLFW_AXES)
    {
        return _glfwJoy[ joy ].NumAxes;
    }
    else if (param == GLFW_BUTTONS)
    {
        return _glfwJoy[ joy ].NumButtons;
    }
    else
    {
        return GL_FALSE;
    }
}

int _glfwPlatformGetJoystickPos( int joy, float *pos, int numaxes )
{

    int       i;

    // Is joystick present?
    if( !_glfwJoy[ joy ].Present )
    {
        return 0;
    }


    // Update joystick state
    if (joy <= GLFW_JOYSTICK_LAST)
    {
        XSDL_SYS_JoystickUpdate(joy);
    }

    // Does the joystick support less axes than requested?
    if( _glfwJoy[ joy ].NumAxes < numaxes )
    {
        numaxes = _glfwJoy[ joy ].NumAxes;
    }

    // Copy axis positions from internal state
    for( i = 0; i < numaxes; ++ i )
    {
        pos[ i ] = _glfwJoy[ joy ].Axis[ i ];
        pos[ i ] = (((pos[ i ] + 32768.0f) / 32767.5f) - 1.0f);
    }

    return numaxes;
}

int _glfwPlatformGetJoystickButtons( int joy, unsigned char *buttons, int numbuttons )
{
    int       i;

    // Is joystick present?
    if( !_glfwJoy[ joy ].Present )
    {
        return 0;
    }

    // Update joystick state
    if (joy <= GLFW_JOYSTICK_LAST)
    {
        XSDL_SYS_JoystickUpdate(joy);
    }

    // Does the joystick support less buttons than requested?
    if( _glfwJoy[ joy ].NumButtons < numbuttons )
    {
        numbuttons = _glfwJoy[ joy ].NumButtons;
    }

    // Copy button states from internal state
    for( i = 0; i < numbuttons; ++ i )
    {
        buttons[ i ] = _glfwJoy[ joy ].Button[ i ];
    }

    return numbuttons;
}

int _glfwPlatformGetJoystickDeviceId( int joy, char** device_id )
{
    // Is joystick present?
    if( !_glfwJoy[ joy ].Present )
    {
        return GL_FALSE;
    }
    else
    {
        recDevice* d = (recDevice*) _glfwJoy[ joy ].Device;
        *device_id = d->product;
        return GL_TRUE;
    }
}
