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

#include "window_linux.h"
#include "joystick_linux.h"
#include <dmsdk/dlib/align.h>
#include <dlib/time.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/inotify.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlib/math.h>

namespace dmPlatform
{
    struct Device
    {
        EvdevPacket m_Packet;
        int         m_Fd;
        char        m_Node[256], m_Name[256], m_Guid[33];
        bool        m_Dropped;
    };
    static Device* g_Devices[16];
    static int g_Watch = -1;
    static uint64_t g_Retry;
    static bool g_Polling;
    static FWindowGamepadEventCallback g_Callback;
    static void* g_UserData;

    static bool Bit(const uint8_t* bits, unsigned code)
    {
        return (bits[code / 8] & (1u << (code % 8))) != 0;
    }

    static bool AnyBit(const uint8_t* bits, unsigned first, unsigned last)
    {
        for (unsigned code = first; code <= last; ++code)
        {
            if (Bit(bits, code))
            {
                return true;
            }
        }

        return false;
    }

    static bool HasJoystickControls(int fd, const uint8_t* absolute, const uint8_t* keys, bool has_keys)
    {
        if (Bit(absolute, ABS_WHEEL) || Bit(absolute, ABS_THROTTLE) || Bit(absolute, ABS_RUDDER) ||
            Bit(absolute, ABS_GAS) || Bit(absolute, ABS_BRAKE))
        {
            return true;
        }

        if (!has_keys)
        {
            // Buttonless pedals can expose one generic position axis per pedal.
            bool has_axis = false;
            for (unsigned code = 0; code < ABS_CNT; ++code)
            {
                if (Bit(absolute, code))
                {
                    if (code > ABS_RZ)
                    {
                        return false;
                    }

                    has_axis = true;
                }
            }

            uint8_t properties[(INPUT_PROP_CNT + 7) / 8] = {};
            ioctl(fd, EVIOCGPROP(sizeof(properties)), properties);
            return has_axis && !Bit(properties, INPUT_PROP_ACCELEROMETER) &&
                   !Bit(properties, INPUT_PROP_DIRECT) && !Bit(properties, INPUT_PROP_POINTER);
        }

        if (Bit(absolute, ABS_X) && Bit(absolute, ABS_Y))
        {
            return true;
        }

        for (unsigned hat = ABS_HAT0X; hat <= ABS_HAT3X; hat += 2)
        {
            if (Bit(absolute, hat) && Bit(absolute, hat + 1))
            {
                return true;
            }
        }

        // Single-axis pedals and handbrakes can also report joystick buttons.
        return AnyBit(absolute, ABS_X, ABS_RZ) &&
               (AnyBit(keys, BTN_0, BTN_9) || AnyBit(keys, BTN_JOYSTICK, BTN_THUMBR) ||
                AnyBit(keys, BTN_GEAR_DOWN, BTN_GEAR_UP) || AnyBit(keys, BTN_DPAD_UP, BTN_DPAD_RIGHT) ||
                AnyBit(keys, BTN_TRIGGER_HAPPY1, BTN_TRIGGER_HAPPY40));
    }

    void EvdevGuid(const input_id& id, const char* name, char guid[33])
    {
        uint8_t bytes[16] = {};
        const uint16_t fields[] = { id.bustype, id.vendor, id.product, id.version };
        for (unsigned i = 0; i < 4; ++i)
        {
            bytes[i * 4] = fields[i] & 255;
            bytes[i * 4 + 1] = fields[i] >> 8;
        }

        if (!id.vendor || !id.product || !id.version)
        {
            memset(bytes + 4, 0, 12);
            memcpy(bytes + 4, name, dmMath::Min((size_t)11, strlen(name)));
        }

        for (unsigned i = 0; i < sizeof(bytes); ++i)
        {
            snprintf(guid + i * 2, 3, "%02x", bytes[i]);
        }
    }

    void InitEvdevPacket(EvdevPacket* p, const uint8_t* keys, const uint8_t* absolute)
    {
        memset(p, 0, sizeof(*p));
        memset(p->m_ButtonsByCode, -1, sizeof(p->m_ButtonsByCode));
        memset(p->m_AxesByCode, -1, sizeof(p->m_AxesByCode));
        for (unsigned code = BTN_MISC; code < KEY_CNT; ++code)
        {
            if (Bit(keys, code))
            {
                p->m_ButtonsByCode[code - BTN_MISC] = p->m_ButtonCount++;
            }
        }

        for (int code = 0; code < ABS_CNT; ++code)
        {
            if (!Bit(absolute, code))
            {
                continue;
            }

            if (code >= ABS_HAT0X && code <= ABS_HAT3Y)
            {
                int first = code & ~1;
                p->m_AxesByCode[first] = p->m_AxesByCode[first + 1] = p->m_HatCount++;
                code = first + 1;
            }
            else
            {
                p->m_AxesByCode[code] = p->m_AxisCount++;
            }
        }
    }

    void ApplyEvdevEvent(EvdevPacket* p, unsigned type, unsigned code, int value)
    {
        if (type == EV_KEY && code >= BTN_MISC && code < KEY_CNT)
        {
            int index = p->m_ButtonsByCode[code - BTN_MISC];
            if (index >= 0)
            {
                p->m_Buttons[index] = value != 0;
            }
        }

        if (type != EV_ABS || code >= ABS_CNT || p->m_AxesByCode[code] < 0)
        {
            return;
        }

        int index = p->m_AxesByCode[code];
        if (code >= ABS_HAT0X && code <= ABS_HAT3Y)
        {
            unsigned neg = (code & 1) ? 1 : 8;
            unsigned pos = (code & 1) ? 4 : 2;
            p->m_Hats[index] = (p->m_Hats[index] & ~(neg | pos)) | (value < 0 ? neg : value > 0 ? pos : 0);
            for (unsigned i = 0; i < 4; ++i)
            {
                p->m_Buttons[p->m_ButtonCount + index * 4 + i] = (p->m_Hats[index] >> i) & 1;
            }
        }
        else
        {
            const input_absinfo& range = p->m_Ranges[code];
            double span = (double)range.maximum - range.minimum;
            p->m_Axes[index] = span > 0 ? (float)(2.0 * ((double)value - range.minimum) / span - 1.0) : (float)value;
        }
    }

    static void Refresh(Device* d)
    {
        uint8_t keys[(KEY_CNT + 7) / 8] = {};
        if (ioctl(d->m_Fd, EVIOCGKEY(sizeof(keys)), keys) >= 0)
        {
            for (unsigned c = BTN_MISC; c < KEY_CNT; ++c)
            {
                ApplyEvdevEvent(&d->m_Packet, EV_KEY, c, Bit(keys, c));
            }
        }

        for (unsigned c = 0; c < ABS_CNT; ++c)
        {
            input_absinfo& info = d->m_Packet.m_Ranges[c];
            if (d->m_Packet.m_AxesByCode[c] >= 0 && ioctl(d->m_Fd, EVIOCGABS(c), &info) == 0)
            {
                ApplyEvdevEvent(&d->m_Packet, EV_ABS, c, info.value);
            }
        }
    }

    static void Remove(unsigned i)
    {
        Device* d = g_Devices[i];
        g_Devices[i] = 0;
        close(d->m_Fd);
        delete d;
        if (g_Callback)
        {
            g_Callback(g_UserData, i, WINDOW_GAMEPAD_EVENT_DISCONNECTED);
        }
    }

    static void Discover(const char* node)
    {
        if (strncmp(node, "event", 5) || !node[5] || node[5 + strspn(node + 5, "0123456789")])
        {
            return;
        }

        unsigned slot = 16;
        for (unsigned i = 0; i < 16; ++i)
        {
            if (g_Devices[i] && !strcmp(g_Devices[i]->m_Node, node))
            {
                return;
            }

            if (!g_Devices[i] && slot == 16)
            {
                slot = i;
            }
        }

        if (slot == 16)
        {
            return;
        }

        char path[280];
        snprintf(path, sizeof(path), "/dev/input/%s", node);
        int fd = open(path, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
        if (fd < 0)
        {
            return;
        }

        uint8_t keys[(KEY_CNT + 7) / 8] = {}, absolute[(ABS_CNT + 7) / 8] = {};
        input_id id;
        uint8_t events[(EV_CNT + 7) / 8] = {};
        if (ioctl(fd, EVIOCGBIT(0, sizeof(events)), events) < 0 || !Bit(events, EV_ABS) ||
            (Bit(events, EV_KEY) && ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(keys)), keys) < 0) ||
            ioctl(fd, EVIOCGBIT(EV_ABS, sizeof(absolute)), absolute) < 0 ||
            ioctl(fd, EVIOCGID, &id) < 0 || !HasJoystickControls(fd, absolute, keys, Bit(events, EV_KEY)))
        {
            close(fd);
            return;
        }

        Device* d = new Device();
        d->m_Fd = fd;
        snprintf(d->m_Node, sizeof(d->m_Node), "%s", node);
        if (ioctl(fd, EVIOCGNAME(sizeof(d->m_Name) - 1), d->m_Name) < 0)
        {
            strcpy(d->m_Name, "Unknown");
        }

        EvdevGuid(id, d->m_Name, d->m_Guid);
        InitEvdevPacket(&d->m_Packet, keys, absolute);
        Refresh(d);
        g_Devices[slot] = d;
        if (g_Callback)
        {
            g_Callback(g_UserData, slot, WINDOW_GAMEPAD_EVENT_CONNECTED);
        }
    }

    static void Scan()
    {
        dirent** paths = 0;
        int count = scandir("/dev/input", &paths, 0, alphasort);
        for (int i = 0; i < count; ++i)
        {
            Discover(paths[i]->d_name);
            free(paths[i]);
        }

        free(paths);
    }

    static void PollDevice(unsigned i)
    {
        Device* d = g_Devices[i];
        if (!d)
        {
            return;
        }

        input_event events[32];
        ssize_t size;
        while ((size = read(d->m_Fd, events, sizeof(events))) > 0)
        {
            for (unsigned j = 0; j < size / sizeof(*events); ++j)
            {
                const input_event& e = events[j];
                if (e.type == EV_SYN && e.code == SYN_DROPPED)
                {
                    d->m_Dropped = true;
                }
                else if (d->m_Dropped)
                {
                    if (e.type == EV_SYN && e.code == SYN_REPORT)
                    {
                        Refresh(d);
                        d->m_Dropped = false;
                    }
                }
                else
                {
                    ApplyEvdevEvent(&d->m_Packet, e.type, e.code, e.value);
                }
            }
        }

        if (size == 0 || (size < 0 && errno == ENODEV))
        {
            Remove(i);
        }
    }

    void LinuxPollJoysticks()
    {
        // HID may query this device synchronously inside its hotplug callback.
        if (g_Polling)
        {
            return;
        }

        g_Polling = true;
        if (g_Watch < 0)
        {
            uint64_t now = dmTime::GetMonotonicTime();
            if (now >= g_Retry)
            {
                g_Retry = now + 1000000;
                g_Watch = inotify_init1(IN_CLOEXEC | IN_NONBLOCK);
                if (g_Watch >= 0 && inotify_add_watch(g_Watch, "/dev/input", IN_CREATE | IN_ATTRIB | IN_MOVED_TO | IN_DELETE | IN_MOVED_FROM) < 0)
                {
                    close(g_Watch);
                    g_Watch = -1;
                }

                Scan();
            }
        }

        DM_ALIGNED(8) char buffer[4096];
        ssize_t size;
        while (g_Watch >= 0 && (size = read(g_Watch, buffer, sizeof(buffer))) > 0)
        {
            for (ssize_t at = 0; at < size;)
            {
                inotify_event* event = (inotify_event*)(buffer + at);
                at += sizeof(*event) + event->len;
                if (event->mask & (IN_IGNORED | IN_Q_OVERFLOW))
                {
                    close(g_Watch);
                    g_Watch = -1;
                    g_Retry = 0;
                    break;
                }

                if (event->mask & (IN_DELETE | IN_MOVED_FROM))
                {
                    for (unsigned i = 0; i < 16; ++i)
                    {
                        if (g_Devices[i] && !strcmp(event->name, g_Devices[i]->m_Node))
                        {
                            Remove(i);
                        }
                    }
                }
                else
                {
                    Discover(event->name);
                }
            }
        }

        for (unsigned i = 0; i < 16; ++i)
        {
            PollDevice(i);
        }

        g_Polling = false;
    }

    void LinuxCloseJoysticks()
    {
        g_Callback = 0;
        g_UserData = 0;
        for (unsigned i = 0; i < 16; ++i)
        {
            if (g_Devices[i])
            {
                Remove(i);
            }
        }

        if (g_Watch >= 0)
        {
            close(g_Watch);
        }

        g_Watch = -1;
        g_Retry = 0;
    }

    void SetGamepadEventCallback(HWindow, FWindowGamepadEventCallback cb, void* data)
    {
        g_Callback = cb;
        g_UserData = data;
    }

    bool GetDeviceState(HWindow w, WindowDeviceState state, int32_t index)
    {
        if (state == WINDOW_DEVICE_STATE_CURSOR_LOCK)
        {
            return w->m_Locked;
        }

        if (state != WINDOW_DEVICE_STATE_JOYSTICK_PRESENT || (unsigned)index >= 16)
        {
            return false;
        }

        LinuxPollJoysticks();
        return g_Devices[index] != 0;
    }

    bool GetDeviceState(HWindow w, WindowDeviceState state)
    {
        return GetDeviceState(w, state, 0);
    }

    static Device* At(unsigned i, bool poll_input = false)
    {
        if (i >= 16)
        {
            return 0;
        }

        if (g_Watch < 0)
        {
            LinuxPollJoysticks();
        }
        else if (poll_input && !g_Polling)
        {
            // Raw input reads must see evdev events that arrived since the last window poll.
            g_Polling = true;
            PollDevice(i);
            g_Polling = false;
        }

        return g_Devices[i];
    }

    const char* GetJoystickDeviceName(HWindow, uint32_t i)
    {
        Device* device = At(i);
        return device ? device->m_Name : 0;
    }

    const char* GetJoystickDeviceGuid(HWindow, uint32_t i)
    {
        Device* device = At(i);
        return device ? device->m_Guid : 0;
    }

    template <typename T>
    static unsigned Copy(T* dst, unsigned cap, const T* src, unsigned count)
    {
        unsigned n = dmMath::Min(cap, count);
        if (n)
        {
            memcpy(dst, src, sizeof(T) * n);
        }

        return n;
    }

    uint32_t GetJoystickAxes(HWindow, uint32_t i, float* dst, uint32_t cap)
    {
        Device* d = At(i, true);
        return d ? Copy(dst, cap, d->m_Packet.m_Axes, d->m_Packet.m_AxisCount) : 0;
    }

    uint32_t GetJoystickHats(HWindow, uint32_t i, uint8_t* dst, uint32_t cap)
    {
        Device* d = At(i, true);
        return d ? Copy(dst, cap, d->m_Packet.m_Hats, d->m_Packet.m_HatCount) : 0;
    }

    uint32_t GetJoystickButtons(HWindow, uint32_t i, uint8_t* dst, uint32_t cap)
    {
        Device* d = At(i, true);
        return d ? Copy(dst, cap, d->m_Packet.m_Buttons, d->m_Packet.m_ButtonCount + 4 * d->m_Packet.m_HatCount) : 0;
    }

}
