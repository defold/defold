//========================================================================
// GLFW - An OpenGL framework
// Platform:    X11/GLX
// API version: 2.7
// WWW:         http://www.glfw.org/
//------------------------------------------------------------------------
// Copyright (c) 2002-2006 Marcus Geelnard
// Copyright (c) 2006-2010 Camilla Berglund <elmindreda@elmindreda.org>
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

#include "internal.h"


//========================================================================
// Note: Only Linux joystick input is supported at the moment. Other
// systems will behave as if there are no joysticks connected.
//========================================================================


//************************************************************************
//****                  GLFW internal functions                       ****
//************************************************************************

#ifdef _GLFW_USE_LINUX_JOYSTICKS

//------------------------------------------------------------------------
// Here are the Linux joystick driver v1.x interface definitions that we
// use (we do not want to rely on <linux/joystick.h>):
//------------------------------------------------------------------------

#include <linux/limits.h> // PATH_MAX
#include <sys/inotify.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <errno.h>

// Joystick event types
#define JS_EVENT_BUTTON     0x01    /* button pressed/released */
#define JS_EVENT_AXIS       0x02    /* joystick moved */
#define JS_EVENT_INIT       0x80    /* initial state of device */

// Joystick event structure
struct js_event {
    unsigned int  time;    /* (u32) event timestamp in milliseconds */
    signed short  value;   /* (s16) value */
    unsigned char type;    /* (u8)  event type */
    unsigned char number;  /* (u8)  axis/button number */
};

struct
{
    int inotify_handle;
    int watch_handle;
} g_js_connection;

// Joystick IOCTL commands
#define JSIOCGVERSION  _IOR('j', 0x01, int)   /* get driver version (u32) */
#define JSIOCGAXES     _IOR('j', 0x11, char)  /* get number of axes (u8) */
#define JSIOCGBUTTONS  _IOR('j', 0x12, char)  /* get number of buttons (u8) */
#define JSIOCGNAME(len) _IOC(_IOC_READ, 'j', 0x13, len) /* get identifier string */

#endif // _GLFW_USE_LINUX_JOYSTICKS


//========================================================================
// Initialize joystick interface
//========================================================================


int _glfwInitJoystick(int deviceIndex, int deviceId, int deviceFd, int isLegacy)
{
    static char ret_data;
    static int driver_version = 0x000800;
    int n;

    // Remember fd
    _glfwJoy[ deviceIndex ].fd = deviceFd;

    // Check that the joystick driver version is 1.0+
    ioctl( deviceFd, JSIOCGVERSION, &driver_version );
    if( driver_version < 0x010000 )
    {
        // It's an old 0.x interface (we don't support it)
        close( deviceFd );
        return 0;
    }

    if ( ioctl( deviceFd, JSIOCGNAME(DEVICE_ID_LENGTH), _glfwJoy[ deviceIndex ].DeviceId ) == -1)
    {
        perror("JSIOCGNAME(DEVICE_ID_LENGTH)");
    }

    // Get number of joystick axes
    ioctl( deviceFd, JSIOCGAXES, &ret_data );
    _glfwJoy[ deviceIndex ].NumAxes = (int) ret_data;

    // Get number of joystick buttons
    ioctl( deviceFd, JSIOCGBUTTONS, &ret_data );
    _glfwJoy[ deviceIndex ].NumButtons = (int) ret_data;

    // Allocate memory for joystick state
    _glfwJoy[ deviceIndex ].Axis =
        (float *) malloc( sizeof(float) *
                          _glfwJoy[ deviceIndex ].NumAxes );
    if( _glfwJoy[ deviceIndex ].Axis == NULL )
    {
        close( deviceFd );
        return 0;
    }
    _glfwJoy[ deviceIndex ].Button =
        (unsigned char *) malloc( sizeof(char) *
                         _glfwJoy[ deviceIndex ].NumButtons );
    if( _glfwJoy[ deviceIndex ].Button == NULL )
    {
        free( _glfwJoy[ deviceIndex ].Axis );
        close( deviceFd );
        return 0;
    }

    // Clear joystick state
    for( n = 0; n < _glfwJoy[ deviceIndex ].NumAxes; ++ n )
    {
        _glfwJoy[ deviceIndex ].Axis[ n ] = 0.0f;
    }
    for( n = 0; n < _glfwJoy[ deviceIndex ].NumButtons; ++ n )
    {
        _glfwJoy[ deviceIndex ].Button[ n ] = GLFW_RELEASE;
    }

    // The joystick is supported and connected
    _glfwJoy[ deviceIndex ].Present = GL_TRUE;

    // We only have a inotify on /dev/input/
    if (!isLegacy)
    {
        _glfwJoy[ deviceIndex ].InputId = deviceId;
    }

    return 1;
}

void _glfwInitJoysticks( void )
{
#ifdef _GLFW_USE_LINUX_JOYSTICKS
    int  k, fd, joy_count;
    char *joy_base_name, joy_dev_name[ 20 ];
#endif // _GLFW_USE_LINUX_JOYSTICKS
    int  i;

    // Start by saying that there are no sticks
    for( i = 0; i <= GLFW_JOYSTICK_LAST; ++ i )
    {
        _glfwJoy[ i ].Present = GL_FALSE;
        _glfwJoy[ i ].InputId = -1;
    }

#ifdef _GLFW_USE_LINUX_JOYSTICKS

    // From glfw 3
    g_js_connection.inotify_handle = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);

    if (g_js_connection.inotify_handle > 0)
    {
        // HACK: Register for IN_ATTRIB to get notified when udev is done
        //       This works well in practice but the true way is libudev
        g_js_connection.watch_handle = inotify_add_watch(g_js_connection.inotify_handle,
            "/dev/input", IN_CREATE | IN_ATTRIB | IN_DELETE);
    }

    // Try to open joysticks (nonblocking)
    joy_count = 0;
    for( k = 0; k <= 1 && joy_count <= GLFW_JOYSTICK_LAST; ++ k )
    {
        // Pick joystick base name
        switch( k )
        {
        case 0:
            joy_base_name = "/dev/input/js";  // USB sticks
            break;
        case 1:
            joy_base_name = "/dev/js";        // "Legacy" sticks
            break;
        default:
            continue;                         // (should never happen)
        }

        // Try to open a few of these sticks
        for( i = 0; i <= 50 && joy_count <= GLFW_JOYSTICK_LAST; ++ i )
        {
            sprintf( joy_dev_name, "%s%d", joy_base_name, i );
            fd = open( joy_dev_name, O_NONBLOCK );
            if( fd != -1 )
            {
                if (!_glfwInitJoystick(joy_count, i, fd, k != 0))
                {
                    continue;
                }

                joy_count ++;
            }
        }
    }

#endif // _GLFW_USE_LINUX_JOYSTICKS

}


//========================================================================
// Close all opened joystick handles
//========================================================================

static inline int _glfwTerminateJoystick(int ix)
{
    if( _glfwJoy[ ix ].Present )
    {
        close( _glfwJoy[ ix ].fd );
        free( _glfwJoy[ ix ].Axis );
        free( _glfwJoy[ ix ].Button );
        _glfwJoy[ ix ].Present = GL_FALSE;
        _glfwJoy[ ix ].InputId = -1;
        return 1;
    }

    return 0;
}

void _glfwTerminateJoysticks( void )
{

#ifdef _GLFW_USE_LINUX_JOYSTICKS

    int i;

    // Close any opened joysticks
    for( i = 0; i <= GLFW_JOYSTICK_LAST; ++ i )
    {
        _glfwTerminateJoystick(i);
    }

    // From glfw 3
    if (g_js_connection.inotify_handle > 0)
    {
        if (g_js_connection.watch_handle > 0)
        {
            inotify_rm_watch(g_js_connection.inotify_handle, g_js_connection.watch_handle);
        }

        close(g_js_connection.inotify_handle);
    }

#endif // _GLFW_USE_LINUX_JOYSTICKS

}


//========================================================================
// Empty joystick event queue
//========================================================================

static void pollJoystickEvents( void )
{

#ifdef _GLFW_USE_LINUX_JOYSTICKS

    struct js_event e;
    int i;

    // Get joystick events for all GLFW joysticks
    for( i = 0; i <= GLFW_JOYSTICK_LAST; ++ i )
    {
        // Is the stick present?
        if( _glfwJoy[ i ].Present )
        {
            // Read all queued events (non-blocking)
            while( read(_glfwJoy[i].fd, &e, sizeof(struct js_event)) > 0 )
            {
                // We don't care if it's an init event or not
                e.type &= ~JS_EVENT_INIT;

                // Check event type
                switch( e.type )
                {
                case JS_EVENT_AXIS:
                    _glfwJoy[ i ].Axis[ e.number ] = (float) e.value /
                                                             32767.0f;
                    // We need to change the sign for the Y axes, so that
                    // positive = up/forward, according to the GLFW spec.
                    if( e.number & 1 )
                    {
                        _glfwJoy[ i ].Axis[ e.number ] =
                            -_glfwJoy[ i ].Axis[ e.number ];
                    }
                    break;

                case JS_EVENT_BUTTON:
                    _glfwJoy[ i ].Button[ e.number ] =
                        e.value ? GLFW_PRESS : GLFW_RELEASE;
                    break;

                default:
                    break;
                }
            }
        }
    }

#endif // _GLFW_USE_LINUX_JOYSTICKS

}

int FindJoystickIndexFromId(int inputId)
{
    int i;
    for( i = 0; i <= GLFW_JOYSTICK_LAST; ++ i )
    {
        if (_glfwJoy[i].InputId == inputId)
        {
            return i;
        }
    }

    return -1;
}


//************************************************************************
//****               Platform implementation functions                ****
//************************************************************************
void _glfwUpdateConnectionState(void)
{
    ssize_t offset = 0;
    char buffer[16384];

    if (g_js_connection.inotify_handle <= 0)
        return;

    const ssize_t size = read(g_js_connection.inotify_handle, buffer, sizeof(buffer));

    while (size > offset)
    {
        const struct inotify_event* e = (struct inotify_event*) (buffer + offset);

        offset += sizeof(struct inotify_event) + e->len;

        char path[PATH_MAX];
        snprintf(path, sizeof(path), "/dev/input/%s", e->name);

        size_t name_len = strlen(e->name);

        if (name_len > 2 && e->name[0] == 'j' && e->name[1] == 's')
        {
            char* end;
            int id = (int) strtol(e->name+2, &end, 10 );
            int ix = FindJoystickIndexFromId(id);

            if (e->mask & (IN_CREATE | IN_ATTRIB) && ix == -1)
            {
                int i, deviceIndex = -1, deviceFd;

                for( i = 0; i <= GLFW_JOYSTICK_LAST; ++ i )
                {
                    if (_glfwJoy[i].Present == GL_FALSE)
                    {
                        deviceIndex = i;
                        break;
                    }
                }

                deviceFd = open( path, O_NONBLOCK );
                if (deviceFd < 0)
                {
                    continue;
                }

                int init_res = _glfwInitJoystick(deviceIndex, id, deviceFd, 0);

                if (deviceIndex != -1 && deviceFd && init_res)
                {
                    _glfwWin.gamepadCallback(id, 1);
                }
            }
            else if (e->mask & IN_DELETE)
            {
                int index = FindJoystickIndexFromId(id);
                if (index >= 0 && _glfwTerminateJoystick(index))
                {
                    _glfwWin.gamepadCallback(id, 0);
                }
            }
        }
    }
}

//========================================================================
// Determine joystick capabilities
//========================================================================

int _glfwPlatformGetJoystickParam( int joy, int param )
{
    if (param == GLFW_PRESENT && joy == 0)
    {
        _glfwUpdateConnectionState();
    }

    // Is joystick present?
    if( !_glfwJoy[ joy ].Present )
    {
        return 0;
    }

    switch( param )
    {
    case GLFW_PRESENT:
        return GL_TRUE;

    case GLFW_AXES:
        return _glfwJoy[ joy ].NumAxes;

    case GLFW_BUTTONS:
        return _glfwJoy[ joy ].NumButtons;

    default:
        break;
    }

    return 0;
}


//========================================================================
// Get joystick axis positions
//========================================================================

int _glfwPlatformGetJoystickPos( int joy, float *pos, int numaxes )
{
    int i;

    // Is joystick present?
    if( !_glfwJoy[ joy ].Present )
    {
        return 0;
    }

    // Update joystick state
    pollJoystickEvents();

    // Does the joystick support less axes than requested?
    if( _glfwJoy[ joy ].NumAxes < numaxes )
    {
        numaxes = _glfwJoy[ joy ].NumAxes;
    }

    // Copy axis positions from internal state
    for( i = 0; i < numaxes; ++ i )
    {
        pos[ i ] = _glfwJoy[ joy ].Axis[ i ];
    }

    return numaxes;
}


//========================================================================
// Get joystick button states
//========================================================================

int _glfwPlatformGetJoystickButtons( int joy, unsigned char *buttons,
    int numbuttons )
{
    int i;

    // Is joystick present?
    if( !_glfwJoy[ joy ].Present )
    {
        return 0;
    }

    // Update joystick state
    pollJoystickEvents();

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

//========================================================================
// Get joystick hats states
//========================================================================

int _glfwPlatformGetJoystickHats( int joy, unsigned char *hats, int numhats )
{
    // No explicit hat support on Linux. (Seems to be exposed as axis instead.)
    return 0;
}

//========================================================================
// _glfwPlatformGetJoystickDeviceId() - Get joystick device id
//========================================================================

int _glfwPlatformGetJoystickDeviceId( int joy, char** device_id )
{
    // Is joystick present?
    if( !_glfwJoy[ joy ].Present )
    {
        return GL_FALSE;
    }
    else
    {
        *device_id = _glfwJoy[ joy ].DeviceId;
        return GL_TRUE;
    }
}

