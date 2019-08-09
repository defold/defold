//========================================================================
// GLFW - An OpenGL framework
// Platform:    Cocoa/NSOpenGL
// API Version: 2.7
// WWW:         http://www.glfw.org/
//------------------------------------------------------------------------
// Copyright (c) 2009-2010 Camilla Berglund <elmindreda@elmindreda.org>
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

// NOTE: Modified source of GLFW 2.7 cocoa_joystick.m
// - Updated GLFW functions to comply with Defold changes.
// - Added connect/disconnect handling through callbacks.
// - Changed hat inputs to behave separately too regular buttons.

#include "internal.h"

#include <unistd.h>
#include <ctype.h>

#include <mach/mach.h>
#include <mach/mach_error.h>

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/IOCFPlugIn.h>
#include <IOKit/hid/IOHIDLib.h>
#include <IOKit/hid/IOHIDKeys.h>
#include <Kernel/IOKit/hidsystem/IOHIDUsageTables.h>


//------------------------------------------------------------------------
// Joystick state
//------------------------------------------------------------------------

typedef struct
{
    int present;
    int index;
    long usagePage, usage;
    char product[256];

    IOHIDDeviceInterface** interface;
    io_service_t hid_service;

    int numAxes;
    int numButtons;
    int numHats;

    CFMutableArrayRef axes;
    CFMutableArrayRef buttons;
    CFMutableArrayRef hats;

} _glfwJoystick;

static _glfwJoystick _glfwJoysticks[GLFW_JOYSTICK_LAST + 1];


typedef struct
{
    IOHIDElementCookie cookie;

    long value;

    long min;
    long max;

    long minReport;
    long maxReport;

    long usage;

} _glfwJoystickElement;


static IOHIDManagerRef gHidManager = nil;

void GetElementsCFArrayHandler( const void* value, void* parameter );

//========================================================================
// Adds an element to the specified joystick
//========================================================================

static void addJoystickElement( _glfwJoystick* joystick, CFTypeRef refElement )
{
    long elementType, usagePage, usage;

    CFTypeRef refElementType = CFDictionaryGetValue( refElement, CFSTR( kIOHIDElementTypeKey ) );
    CFTypeRef refUsagePage = CFDictionaryGetValue( refElement, CFSTR( kIOHIDElementUsagePageKey ) );
    CFTypeRef refUsage = CFDictionaryGetValue( refElement, CFSTR( kIOHIDElementUsageKey ) );

    CFMutableArrayRef elementsArray = NULL;

    if ((refElementType) && CFNumberGetValue( refElementType, kCFNumberLongType, &elementType ))
    {
        if( elementType == kIOHIDElementTypeInput_Axis ||
            elementType == kIOHIDElementTypeInput_Button ||
            elementType == kIOHIDElementTypeInput_Misc )
        {
            if (refUsagePage
             && CFNumberGetValue(refUsagePage, kCFNumberLongType, &usagePage)
             && refUsage
             && CFNumberGetValue(refUsage, kCFNumberLongType, &usage)) {

                switch( usagePage ) /* only interested in kHIDPage_GenericDesktop and kHIDPage_Button */
                {
                    case kHIDPage_GenericDesktop:
                    {
                        switch( usage )
                        {
                            case kHIDUsage_GD_X:
                            case kHIDUsage_GD_Y:
                            case kHIDUsage_GD_Z:
                            case kHIDUsage_GD_Rx:
                            case kHIDUsage_GD_Ry:
                            case kHIDUsage_GD_Rz:
                            case kHIDUsage_GD_Slider:
                            case kHIDUsage_GD_Dial:
                            case kHIDUsage_GD_Wheel:
                                joystick->numAxes++;
                                elementsArray = joystick->axes;
                                break;
                            case kHIDUsage_GD_Hatswitch:
                                joystick->numHats++;
                                elementsArray = joystick->hats;
                                break;
                        }

                        break;
                    }

                    case kHIDPage_Button:
                        joystick->numButtons++;
                        elementsArray = joystick->buttons;
                        break;
                    default:
                        break;
                }
            }


        }
        else
        {
            CFTypeRef refElementTop = CFDictionaryGetValue( refElement, CFSTR( kIOHIDElementKey ) );
            if( refElementTop )
            {
                CFTypeID type = CFGetTypeID( refElementTop );
                if( type == CFArrayGetTypeID() ) /* if element is an array */
                {
                    CFRange range = { 0, CFArrayGetCount( refElementTop ) };
                    CFArrayApplyFunction( refElementTop, range, GetElementsCFArrayHandler, joystick );
                }
            }
        }
    }

    if( elementsArray )
    {
        long number;
        CFTypeRef refType;

        _glfwJoystickElement* element = (_glfwJoystickElement*) malloc( sizeof( _glfwJoystickElement ) );

        // Make sure to input the element in the correct order
        // based on the usage.
        int j = 0;
        for( ; j < CFArrayGetCount(elementsArray);  j++ )
        {
            _glfwJoystickElement* elem =
                (_glfwJoystickElement*) CFArrayGetValueAtIndex( elementsArray, j );
            if (usage < elem->usage) {
                break;
            }
        }
        CFArrayInsertValueAtIndex( elementsArray, j, element );

        element->usage = usage;

        refType = CFDictionaryGetValue( refElement, CFSTR( kIOHIDElementCookieKey ) );
        if( refType && CFNumberGetValue( refType, kCFNumberLongType, &number ) )
        {
            element->cookie = (IOHIDElementCookie) number;
        }

        refType = CFDictionaryGetValue( refElement, CFSTR( kIOHIDElementMinKey ) );
        if( refType && CFNumberGetValue( refType, kCFNumberLongType, &number ) )
        {
            element->minReport = element->min = number;
        }

        refType = CFDictionaryGetValue( refElement, CFSTR( kIOHIDElementMaxKey ) );
        if( refType && CFNumberGetValue( refType, kCFNumberLongType, &number ) )
        {
            element->maxReport = element->max = number;
        }

    }
}


//========================================================================
// Adds an element to the specified joystick
//========================================================================

void GetElementsCFArrayHandler( const void* value, void* parameter )
{
    if( CFGetTypeID( value ) == CFDictionaryGetTypeID() )
    {
        addJoystickElement( (_glfwJoystick*) parameter, (CFTypeRef) value );
    }
}


//========================================================================
// Returns the value of the specified element of the specified joystick
//========================================================================

static long getElementValue( _glfwJoystick* joystick, _glfwJoystickElement* element )
{
    IOReturn result = kIOReturnSuccess;
    IOHIDEventStruct hidEvent;
    hidEvent.value = 0;

    if( joystick && element && joystick->interface )
    {
        result = (*(joystick->interface))->getElementValue( joystick->interface,
                                                            element->cookie,
                                                            &hidEvent );
        if( kIOReturnSuccess == result )
        {
            /* record min and max for auto calibration */
            if( hidEvent.value < element->minReport )
            {
                element->minReport = hidEvent.value;
            }
            if( hidEvent.value > element->maxReport )
            {
                element->maxReport = hidEvent.value;
            }
        }
    }

    /* auto user scale */
    return (long) hidEvent.value;
}


//========================================================================
// Removes the specified joystick
//========================================================================

static void removeJoystick( _glfwJoystick* joystick )
{
    int i;

    if( joystick->present )
    {
        joystick->present = GL_FALSE;

        for( i = 0;  i < joystick->numAxes;  i++ )
        {
            _glfwJoystickElement* axes =
                (_glfwJoystickElement*) CFArrayGetValueAtIndex( joystick->axes, i );
            free( axes );
        }
        CFArrayRemoveAllValues( joystick->axes );
        joystick->numAxes = 0;

        for( i = 0;  i < joystick->numButtons;  i++ )
        {
            _glfwJoystickElement* button =
                (_glfwJoystickElement*) CFArrayGetValueAtIndex( joystick->buttons, i );
            free( button );
        }
        CFArrayRemoveAllValues( joystick->buttons );
        joystick->numButtons = 0;

        for( i = 0;  i < joystick->numHats;  i++ )
        {
            _glfwJoystickElement* hat =
                (_glfwJoystickElement*) CFArrayGetValueAtIndex( joystick->hats, i );
            free( hat );
        }
        CFArrayRemoveAllValues( joystick->hats );
        joystick->hats = 0;

        (*(joystick->interface))->close( joystick->interface );
        (*(joystick->interface))->Release( joystick->interface );

        joystick->interface = NULL;
    }
}


//========================================================================
// Callback for user-initiated joystick removal
//========================================================================

static void removalCallback( void* target, IOReturn result, void* refcon, void* sender )
{
    removeJoystick( (_glfwJoystick*) refcon );
}


//========================================================================
// Polls for joystick events and updates GFLW state
//========================================================================

static void pollJoystickEvents( void )
{
    int i;
    CFIndex j;

    for( i = 0;  i < GLFW_JOYSTICK_LAST + 1;  i++ )
    {
        _glfwJoystick* joystick = &_glfwJoysticks[i];

        if( joystick->present )
        {
            for( j = 0;  j < joystick->numButtons;  j++ )
            {
                _glfwJoystickElement* button =
                    (_glfwJoystickElement*) CFArrayGetValueAtIndex( joystick->buttons, j );
                button->value = getElementValue( joystick, button );
            }

            for( j = 0;  j < joystick->numAxes;  j++ )
            {
                _glfwJoystickElement* axes =
                    (_glfwJoystickElement*) CFArrayGetValueAtIndex( joystick->axes, j );
                // axes->value = getElementValue( joystick, axes );
                long value = getElementValue( joystick, axes );
                long t_min = -32768;
                long t_max = 32767;
                float deviceScale = t_max - t_min;
                float readScale = axes->maxReport - axes->minReport;
                if (readScale != 0)
                    value = ((value - axes->minReport) * deviceScale / readScale) + t_min;
                axes->value = value;
            }

            for( j = 0;  j < joystick->numHats;  j++ )
            {
                _glfwJoystickElement* hat =
                    (_glfwJoystickElement*) CFArrayGetValueAtIndex( joystick->hats, j );
                int value = getElementValue(joystick, hat) - hat->min;

                uint8_t pos = 0;
                int range = (hat->max - hat->min + 1);
                if (range == 4)         /* 4 position hatswitch - scale up value */
                    value *= 2;
                else if (range != 8)    /* Neither a 4 nor 8 positions - fall back to default position (centered) */
                    value = -1;
                switch (value) {
                    case 0:
                        pos = GLFW_HAT_UP;
                        break;
                    case 1:
                        pos = GLFW_HAT_RIGHTUP;
                        break;
                    case 2:
                        pos = GLFW_HAT_RIGHT;
                        break;
                    case 3:
                        pos = GLFW_HAT_RIGHTDOWN;
                        break;
                    case 4:
                        pos = GLFW_HAT_DOWN;
                        break;
                    case 5:
                        pos = GLFW_HAT_LEFTDOWN;
                        break;
                    case 6:
                        pos = GLFW_HAT_LEFT;
                        break;
                    case 7:
                        pos = GLFW_HAT_LEFTUP;
                        break;
                    default:
                        /* Every other value is mapped to center. We do that because some
                         * joysticks use 8 and some 15 for this value, and apparently
                         * there are even more variants out there - so we try to be generous.
                         */
                        pos = GLFW_HAT_CENTERED;
                        break;
                }
                if (pos != hat->value)
                    hat->value = pos;
            }
        }
    }
}

static void joystickAddedCallback(void* inContext, IOReturn inResult, void* inSender, IOHIDDeviceRef device)
{
    _glfwJoystick* joystick = NULL;
    for (int i = 0; i <= GLFW_JOYSTICK_LAST; ++i)
    {
        if (!_glfwJoysticks[i].present) {
            joystick = &_glfwJoysticks[i];
            break;
        }
    }

    if (!joystick) {
        printf("Couldn't find any unused joystick entries, skipping newly connected.\n");
        return;
    }

    IOCFPlugInInterface** ppPlugInInterface = NULL;
    HRESULT plugInResult = S_OK;
    SInt32 score = 0;

    CFMutableDictionaryRef hidProperties = 0;
    kern_return_t result;
    CFTypeRef refCF = 0;

    long usagePage, usage;

    io_service_t hid_device_service = IOHIDDeviceGetService(device);

    result = IORegistryEntryCreateCFProperties( hid_device_service,
                                                &hidProperties,
                                                kCFAllocatorDefault,
                                                kNilOptions );

    if( result != kIOReturnSuccess )
    {
        printf("IORegistryEntryCreateCFProperties failed\n");
        return;
    }

    // Check device type
    refCF = CFDictionaryGetValue( hidProperties, CFSTR( kIOHIDPrimaryUsagePageKey ) );
    if( refCF )
    {
        CFNumberGetValue( refCF, kCFNumberLongType, &usagePage );
        if( usagePage != kHIDPage_GenericDesktop )
        {
            // We are not interested in this device
            return;
        }
    }

    refCF = CFDictionaryGetValue( hidProperties, CFSTR( kIOHIDPrimaryUsageKey ) );
    if( refCF )
    {
        CFNumberGetValue( refCF, kCFNumberLongType, &usage );

        if( usage != kHIDUsage_GD_Joystick &&
            usage != kHIDUsage_GD_GamePad &&
            usage != kHIDUsage_GD_MultiAxisController )
        {
            // We are not interested in this device
            return;
        }
    }

    result = IOCreatePlugInInterfaceForService( hid_device_service,
                                                kIOHIDDeviceUserClientTypeID,
                                                kIOCFPlugInInterfaceID,
                                                &ppPlugInInterface,
                                                &score );

    if( kIOReturnSuccess != result )
    {
        printf("IOCreatePlugInInterfaceForService was not successfull.\n");
        return;
    }

    plugInResult = (*ppPlugInInterface)->QueryInterface( ppPlugInInterface,
                                                         CFUUIDGetUUIDBytes( kIOHIDDeviceInterfaceID ),
                                                         (void *) &(joystick->interface) );

    if( plugInResult != S_OK )
    {
        printf("QueryInterface was not successfull.\n");
        return;
    }

    (*ppPlugInInterface)->Release( ppPlugInInterface );

    (*(joystick->interface))->open( joystick->interface, 0 );
    (*(joystick->interface))->setRemovalCallback( joystick->interface,
                                                  removalCallback,
                                                  joystick,
                                                  joystick );

    // Get product string
    refCF = CFDictionaryGetValue( hidProperties, CFSTR( kIOHIDProductKey ) );
    if( refCF )
    {
        CFStringGetCString( refCF,
                            (char*) &(joystick->product),
                            256,
                            CFStringGetSystemEncoding() );
    }

    joystick->hid_service = hid_device_service;
    joystick->present = GL_TRUE;
    joystick->numAxes = 0;
    joystick->numButtons = 0;
    joystick->numHats = 0;
    joystick->axes = CFArrayCreateMutable( NULL, 0, NULL );
    joystick->buttons = CFArrayCreateMutable( NULL, 0, NULL );
    joystick->hats = CFArrayCreateMutable( NULL, 0, NULL );

    CFTypeRef refTopElement = CFDictionaryGetValue( hidProperties,
                                                    CFSTR( kIOHIDElementKey ) );
    CFTypeID type = CFGetTypeID( refTopElement );
    if( type == CFArrayGetTypeID() )
    {
        CFRange range = { 0, CFArrayGetCount( refTopElement ) };
        CFArrayApplyFunction( refTopElement,
                              range,
                              GetElementsCFArrayHandler,
                              (void*) joystick);
    }

    if (_glfwWin.gamepadCallback) {
        _glfwWin.gamepadCallback(joystick->index, 1);
    }
}

static void joystickRemovedCallback(void* inContext, IOReturn inResult, void* inSender, IOHIDDeviceRef device) {
    io_service_t hid_device_service = IOHIDDeviceGetService(device);

    for (int i = 0; i <= GLFW_JOYSTICK_LAST; ++i)
    {
        _glfwJoystick* joystick = &_glfwJoysticks[i];
        if (joystick->present && joystick->hid_service == hid_device_service) {
            joystick->hid_service = 0;

            removeJoystick(joystick);

            if (_glfwWin.gamepadCallback) {
                _glfwWin.gamepadCallback(joystick->index, 0);
            }
            return;
        }
    }

    printf("Couldn't find any newly disconnected joystick, skipping.\n");
}

//************************************************************************
//****                  GLFW internal functions                       ****
//************************************************************************

//========================================================================
// Initialize joystick interface
//========================================================================

int _glfwInitJoysticks( void )
{
    for( int i = 0; i <= GLFW_JOYSTICK_LAST; ++ i )
    {
        _glfwJoysticks[ i ].index = i;
        _glfwJoysticks[ i ].present = GL_FALSE;
    }

    mach_port_t masterPort = 0;
    CFMutableDictionaryRef hidMatchDictionary = NULL;
    io_object_t ioHIDDeviceObject = 0;

    gHidManager = IOHIDManagerCreate(kCFAllocatorDefault, kIOHIDOptionsTypeNone);

    NSMutableArray *criterionArray = [NSMutableArray arrayWithCapacity:3];
    {
        NSMutableDictionary* criterion = [NSMutableDictionary dictionaryWithCapacity: 2];
            [criterion setObject: [NSNumber numberWithInt: kHIDPage_GenericDesktop]
                          forKey: (NSString*)CFSTR(kIOHIDDeviceUsagePageKey)];
            [criterion setObject: [NSNumber numberWithInt: kHIDUsage_GD_Joystick]
                          forKey: (NSString*)CFSTR(kIOHIDDeviceUsageKey)];
        [criterionArray addObject:criterion];
    }
    {
        NSMutableDictionary* criterion = [NSMutableDictionary dictionaryWithCapacity: 2];
            [criterion setObject: [NSNumber numberWithInt: kHIDPage_GenericDesktop]
                          forKey: (NSString*)CFSTR(kIOHIDDeviceUsagePageKey)];
            [criterion setObject: [NSNumber numberWithInt: kHIDUsage_GD_GamePad]
                          forKey: (NSString*)CFSTR(kIOHIDDeviceUsageKey)];
        [criterionArray addObject:criterion];
    }
    {
        NSMutableDictionary* criterion = [NSMutableDictionary dictionaryWithCapacity: 2];
            [criterion setObject: [NSNumber numberWithInt: kHIDPage_GenericDesktop]
                          forKey: (NSString*)CFSTR(kIOHIDDeviceUsagePageKey)];
            [criterion setObject: [NSNumber numberWithInt: kHIDUsage_GD_MultiAxisController]
                          forKey: (NSString*)CFSTR(kIOHIDDeviceUsageKey)];
        [criterionArray addObject:criterion];
    }

    IOHIDManagerSetDeviceMatchingMultiple(gHidManager, (__bridge CFArrayRef)criterionArray);
    IOHIDManagerRegisterDeviceMatchingCallback(gHidManager, joystickAddedCallback, NULL);
    IOHIDManagerRegisterDeviceRemovalCallback(gHidManager, joystickRemovedCallback, NULL);
    IOHIDManagerScheduleWithRunLoop(gHidManager, CFRunLoopGetMain(), kCFRunLoopDefaultMode);
    IOHIDManagerOpen(gHidManager, kIOHIDOptionsTypeNone);

    return GL_TRUE;
}


//========================================================================
// Close all opened joystick handles
//========================================================================

void _glfwTerminateJoysticks( void )
{
    int i;

    for( i = 0;  i < GLFW_JOYSTICK_LAST + 1;  i++ )
    {
        _glfwJoystick* joystick = &_glfwJoysticks[i];
        removeJoystick( joystick );
    }

    IOHIDManagerClose(gHidManager, kIOHIDOptionsTypeNone);
}


//************************************************************************
//****               Platform implementation functions                ****
//************************************************************************

//========================================================================
// Determine joystick capabilities
//========================================================================

int _glfwPlatformGetJoystickParam( int joy, int param )
{
    if( !_glfwJoysticks[joy].present )
    {
        // TODO: Figure out if this is an error
        return GL_FALSE;
    }

    switch( param )
    {
        case GLFW_PRESENT:
            return GL_TRUE;

        case GLFW_AXES:
            return (int) CFArrayGetCount( _glfwJoysticks[joy].axes );

        case GLFW_BUTTONS:
            return (int) CFArrayGetCount( _glfwJoysticks[joy].buttons );

        case GLFW_HATS:
            return (int) CFArrayGetCount( _glfwJoysticks[joy].hats );

        default:
            break;
    }

    return GL_FALSE;
}


//========================================================================
// Get joystick axis positions
//========================================================================

int _glfwPlatformGetJoystickPos( int joy, float *pos, int numaxes )
{
    int i;

    if( joy < GLFW_JOYSTICK_1 || joy > GLFW_JOYSTICK_LAST )
    {
        return 0;
    }

    _glfwJoystick joystick = _glfwJoysticks[joy];

    if( !joystick.present )
    {
        // TODO: Figure out if this is an error
        return 0;
    }

    numaxes = numaxes < joystick.numAxes ? numaxes : joystick.numAxes;

    // Update joystick state
    pollJoystickEvents();

    for( i = 0;  i < numaxes;  i++ )
    {
        _glfwJoystickElement* axes =
            (_glfwJoystickElement*) CFArrayGetValueAtIndex( joystick.axes, i );

        pos[ i ] = axes->value;
        pos[ i ] = (((pos[ i ] + 32768.0f) / 32767.5f) - 1.0f);
    }

    return numaxes;
}


//========================================================================
// Get joystick button states
//========================================================================

int _glfwPlatformGetJoystickButtons( int joy, unsigned char *buttons, int numbuttons )
{
    int button;

    if( joy < GLFW_JOYSTICK_1 || joy > GLFW_JOYSTICK_LAST )
    {
        return 0;
    }

    _glfwJoystick joystick = _glfwJoysticks[joy];

    if( !joystick.present )
    {
        // TODO: Figure out if this is an error
        return 0;
    }

    // Update joystick state
    pollJoystickEvents();

    for( button = 0;  button < numbuttons && button < joystick.numButtons;  button++ )
    {
        _glfwJoystickElement* element = (_glfwJoystickElement*) CFArrayGetValueAtIndex( joystick.buttons, button );
        buttons[button] = element->value ? GLFW_PRESS : GLFW_RELEASE;
    }

    return button;
}

int _glfwPlatformGetJoystickHats( int joy, unsigned char *hats, int numhats )
{
    int i, value;

    if( joy < GLFW_JOYSTICK_1 || joy > GLFW_JOYSTICK_LAST )
    {
        return 0;
    }

    _glfwJoystick joystick = _glfwJoysticks[joy];

    // Is joystick present?
    if( !joystick.present )
    {
        return 0;
    }

    // Does the joystick support less hats than requested?
    if( joystick.numHats < numhats )
    {
        numhats = joystick.numHats;
    }

    // Update joystick state
    pollJoystickEvents();

    // Copy hat states from internal state
    for( i = 0; i < numhats; ++ i )
    {
        _glfwJoystickElement* element = (_glfwJoystickElement*) CFArrayGetValueAtIndex( joystick.hats, i );
        hats[i] = element->value;
    }

    return numhats;
}

int _glfwPlatformGetJoystickDeviceId( int joy, char** device_id )
{
    // Is joystick present?
    if( !_glfwJoysticks[ joy ].present )
    {
        return GL_FALSE;
    }
    else
    {
        *device_id = _glfwJoysticks[ joy ].product;
        return GL_TRUE;
    }
}
