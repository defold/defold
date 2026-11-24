// Copyright 2020-2026 The Defold Foundation
// Copyright 2014-2020 King
// Copyright 2009-2014 Ragnar Svensson, Christian Murray
// Licensed under the Defold License version 1.0 (the "License"); you may not use
// this file except in compliance with the License.
//
// You may obtain a copy of the License, together with FAQs at
// https://www.defold.com/license
//
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>
#include "../linux/window_linux.h"
#include <linux/input.h>
#include <dirent.h>
#include <fcntl.h>
#include <stdarg.h>
#include <sys/inotify.h>
#include <sys/stat.h>
#include <unistd.h>

static char g_Directory[] = "/tmp/defold-evdev-XXXXXX";
static char g_Fifo[128];
static int g_DeviceFd = -1;
static unsigned g_Connected, g_Disconnected;
static bool g_HatOnly;
static int g_StandaloneAxis = -1;
static unsigned g_StandaloneAxisCount;
static bool g_Accelerometer;

// Only the device identity and capability ioctls are synthetic. Discovery,
// hotplug notification, nonblocking event reads and removal use kernel objects.
static void SetBit(uint8_t* bits, unsigned code)
{
    bits[code / 8] |= 1 << (code % 8);
}

extern "C"
{
    int __real_open(const char*, int, ...);
    int __wrap_open(const char* path, int flags, ...)
    {
        if (!strcmp(path, "/dev/input/event5"))
        {
            return g_DeviceFd = __real_open(g_Fifo, flags);
        }

        if (flags & O_CREAT)
        {
            va_list args;
            va_start(args, flags);
            int mode = va_arg(args, int);
            va_end(args);
            return __real_open(path, flags, mode);
        }

        return __real_open(path, flags);
    }

    int __real_inotify_add_watch(int, const char*, uint32_t);
    int __wrap_inotify_add_watch(int fd, const char* path, uint32_t flags)
    {
        return __real_inotify_add_watch(fd, !strcmp(path, "/dev/input") ? g_Directory : path, flags);
    }

    int __real_scandir(const char*, dirent***, int (*)(const dirent*), int (*)(const dirent**, const dirent**));
    int __wrap_scandir(const char* path, dirent*** entries, int (*filter)(const dirent*), int (*compare)(const dirent**, const dirent**))
    {
        return __real_scandir(!strcmp(path, "/dev/input") ? g_Directory : path, entries, filter, compare);
    }

    int __real_ioctl(int, unsigned long, ...);
    int __wrap_ioctl(int fd, unsigned long request, ...)
    {
        va_list args;
        va_start(args, request);
        void* value = va_arg(args, void*);
        va_end(args);
        if (fd != g_DeviceFd)
        {
            return __real_ioctl(fd, request, value);
        }

        memset(value, 0, _IOC_SIZE(request));
        uint8_t* bits = (uint8_t*)value;
        switch (_IOC_NR(request))
        {
            case _IOC_NR(EVIOCGPROP(0)):
            {
                if (g_Accelerometer)
                {
                    SetBit(bits, INPUT_PROP_ACCELEROMETER);
                }

                break;
            }
            case _IOC_NR(EVIOCGBIT(0, 0)):
            {
                if (g_StandaloneAxis < 0)
                {
                    SetBit(bits, EV_KEY);
                }
                SetBit(bits, EV_ABS);
                break;
            }
            case _IOC_NR(EVIOCGBIT(EV_KEY, 0)):
            {
                if (g_StandaloneAxis >= 0)
                {
                    return -1;
                }
                SetBit(bits, BTN_A);
                break;
            }
            case _IOC_NR(EVIOCGBIT(EV_ABS, 0)):
            {
                if (g_StandaloneAxis >= 0)
                {
                    for (unsigned axis = 0; axis < g_StandaloneAxisCount; ++axis)
                    {
                        SetBit(bits, g_StandaloneAxis + axis);
                    }

                    break;
                }
                SetBit(bits, g_HatOnly ? ABS_HAT0X : ABS_X);
                SetBit(bits, g_HatOnly ? ABS_HAT0Y : ABS_Y);
                break;
            }
            case _IOC_NR(EVIOCGID):
            {
                *(input_id*)value = { BUS_USB, 0x045e, 0x028e, 0x0114 };
                break;
            }
            case _IOC_NR(EVIOCGNAME(0)):
            {
                strcpy((char*)value, "Fixture");
                break;
            }
            case _IOC_NR(EVIOCGABS(ABS_X)):
            case _IOC_NR(EVIOCGABS(ABS_Y)):
            case _IOC_NR(EVIOCGABS(ABS_Z)):
            case _IOC_NR(EVIOCGABS(ABS_RX)):
            case _IOC_NR(EVIOCGABS(ABS_RY)):
            case _IOC_NR(EVIOCGABS(ABS_RZ)):
            case _IOC_NR(EVIOCGABS(ABS_WHEEL)):
            case _IOC_NR(EVIOCGABS(ABS_THROTTLE)):
            {
                ((input_absinfo*)value)->maximum = 255;
                break;
            }
        }

        return 0;
    }

}

static void Event(void*, int index, WindowGamepadEvent event)
{
    if (index == 0 && event == WINDOW_GAMEPAD_EVENT_CONNECTED)
    {
        ++g_Connected;
    }

    if (index == 0 && event == WINDOW_GAMEPAD_EVENT_DISCONNECTED)
    {
        ++g_Disconnected;
    }

    dmPlatform::GetDeviceState(0, WINDOW_DEVICE_STATE_JOYSTICK_PRESENT, index);
}

// Raw queries discover already connected sticks before the first poll. Subsequent hotplugs report their own controls.
TEST(LinuxJoystick, HotplugAndResync)
{
    ASSERT_NE((char*)0, mkdtemp(g_Directory));
    snprintf(g_Fifo, sizeof(g_Fifo), "%s/event5", g_Directory);
    dmPlatform::SetGamepadEventCallback(0, Event, 0);
    for (unsigned cycle = 1; cycle <= 2; ++cycle)
    {
        g_HatOnly = cycle == 2;
        ASSERT_EQ(0, mkfifo(g_Fifo, 0600));
        int writer = open(g_Fifo, O_RDWR | O_NONBLOCK);
        ASSERT_GE(writer, 0);
        if (cycle > 1)
        {
            dmPlatform::LinuxPollJoysticks();
        }

        ASSERT_STREQ("Fixture", dmPlatform::GetJoystickDeviceName(0, 0));
        ASSERT_EQ(cycle, g_Connected);
        ASSERT_STREQ("030000005e0400008e02000014010000", dmPlatform::GetJoystickDeviceGuid(0, 0));
        input_event events[] = { { {}, EV_KEY, BTN_A, 1 }, { {}, EV_ABS, (uint16_t)(g_HatOnly ? ABS_HAT0X : ABS_X), g_HatOnly ? 1 : 255 }, { {}, EV_SYN, SYN_REPORT, 0 } };
        ASSERT_EQ((ssize_t)sizeof(events), write(writer, events, sizeof(events)));
        dmPlatform::LinuxPollJoysticks();
        uint8_t buttons[5];
        float axes[2];
        ASSERT_EQ(g_HatOnly ? 5u : 1u, dmPlatform::GetJoystickButtons(0, 0, buttons, sizeof(buttons)));
        ASSERT_EQ(1u, buttons[0]);
        ASSERT_EQ(g_HatOnly ? 0u : 2u, dmPlatform::GetJoystickAxes(0, 0, axes, 2));
        if (g_HatOnly)
        {
            uint8_t hat;
            ASSERT_EQ(1u, dmPlatform::GetJoystickHats(0, 0, &hat, 1));
            ASSERT_EQ(2u, hat);
            ASSERT_EQ(1u, buttons[2]);
        }
        else
        {
            ASSERT_EQ(1.0f, axes[0]);
        }
        input_event dropped[] = { { {}, EV_SYN, SYN_DROPPED, 0 }, { {}, EV_SYN, SYN_REPORT, 0 } };
        ASSERT_EQ((ssize_t)sizeof(dropped), write(writer, dropped, sizeof(dropped)));
        dmPlatform::LinuxPollJoysticks();
        dmPlatform::GetJoystickButtons(0, 0, buttons, sizeof(buttons));
        ASSERT_EQ(0u, buttons[0]);
        close(writer);
        ASSERT_EQ(0, unlink(g_Fifo));
        dmPlatform::LinuxPollJoysticks();
        ASSERT_EQ(cycle, g_Disconnected);
        ASSERT_FALSE(dmPlatform::GetDeviceState(0, WINDOW_DEVICE_STATE_JOYSTICK_PRESENT, 0));
    }

    // VRS DirectForce Pro pedals expose X/Y/Z and Heusinkveld Sim Pedals Ultimate expose RX/RY/RZ.
    const struct
    {
        int      first;
        unsigned count;
    } standalone[] = {
        { ABS_WHEEL, 1 }, { ABS_THROTTLE, 1 }, { ABS_X, 1 }, { ABS_X, 3 }, { ABS_RX, 3 }
    };
    for (unsigned i = 0; i < sizeof(standalone) / sizeof(standalone[0]); ++i)
    {
        g_StandaloneAxis = standalone[i].first;
        g_StandaloneAxisCount = standalone[i].count;
        ASSERT_EQ(0, mkfifo(g_Fifo, 0600));
        int writer = open(g_Fifo, O_RDWR | O_NONBLOCK);
        ASSERT_GE(writer, 0);
        dmPlatform::LinuxPollJoysticks();
        ASSERT_EQ(3u + i, g_Connected);
        float axes[3];
        uint8_t button;
        EXPECT_EQ(0u, dmPlatform::GetJoystickButtons(0, 0, &button, 1));
        ASSERT_EQ(g_StandaloneAxisCount, dmPlatform::GetJoystickAxes(0, 0, axes, 3));
        for (unsigned axis = 0; axis < g_StandaloneAxisCount; ++axis)
        {
            EXPECT_EQ(-1.0f, axes[axis]);
        }

        close(writer);
        ASSERT_EQ(0, unlink(g_Fifo));
        dmPlatform::LinuxPollJoysticks();
        ASSERT_EQ(3u + i, g_Disconnected);
    }
    unsigned connected = g_Connected;
    g_StandaloneAxis = ABS_X;
    g_Accelerometer = true;
    ASSERT_EQ(0, mkfifo(g_Fifo, 0600));
    dmPlatform::LinuxPollJoysticks();
    EXPECT_EQ(connected, g_Connected);
    EXPECT_FALSE(dmPlatform::GetDeviceState(0, WINDOW_DEVICE_STATE_JOYSTICK_PRESENT, 0));
    ASSERT_EQ(0, unlink(g_Fifo));
    dmPlatform::LinuxCloseJoysticks();
    ASSERT_EQ(0, rmdir(g_Directory));
}

int main(int argc, char** argv)
{
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
