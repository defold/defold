/**
 * OpenAL cross platform audio library
 * Copyright (C) 2009 by Konstantinos Natsakis <konstantinos.natsakis@gmail.com>
 * Copyright (C) 2010 by Chris Robinson <chris.kcat@gmail.com>
 * This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 *  License along with this library; if not, write to the
 *  Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 *  Boston, MA  02111-1307, USA.
 * Or go to http://www.gnu.org/copyleft/lgpl.html
 */

#include "config.h"

#include <string.h>

#include "alMain.h"
#include "alu.h"

#include <pulse/pulseaudio.h>

#if PA_API_VERSION == 12

#ifndef PA_CHECK_VERSION
#define PA_CHECK_VERSION(major,minor,micro)                             \
    ((PA_MAJOR > (major)) ||                                            \
     (PA_MAJOR == (major) && PA_MINOR > (minor)) ||                     \
     (PA_MAJOR == (major) && PA_MINOR == (minor) && PA_MICRO >= (micro)))
#endif

#ifdef HAVE_DYNLOAD
static void *pa_handle;
#define MAKE_FUNC(x) static typeof(x) * p##x
MAKE_FUNC(pa_context_unref);
MAKE_FUNC(pa_sample_spec_valid);
MAKE_FUNC(pa_frame_size);
MAKE_FUNC(pa_stream_drop);
MAKE_FUNC(pa_strerror);
MAKE_FUNC(pa_context_get_state);
MAKE_FUNC(pa_stream_get_state);
MAKE_FUNC(pa_threaded_mainloop_signal);
MAKE_FUNC(pa_stream_peek);
MAKE_FUNC(pa_threaded_mainloop_wait);
MAKE_FUNC(pa_threaded_mainloop_unlock);
MAKE_FUNC(pa_threaded_mainloop_in_thread);
MAKE_FUNC(pa_context_new);
MAKE_FUNC(pa_threaded_mainloop_stop);
MAKE_FUNC(pa_context_disconnect);
MAKE_FUNC(pa_threaded_mainloop_start);
MAKE_FUNC(pa_threaded_mainloop_get_api);
MAKE_FUNC(pa_context_set_state_callback);
MAKE_FUNC(pa_stream_write);
MAKE_FUNC(pa_xfree);
MAKE_FUNC(pa_stream_connect_record);
MAKE_FUNC(pa_stream_connect_playback);
MAKE_FUNC(pa_stream_readable_size);
MAKE_FUNC(pa_stream_writable_size);
MAKE_FUNC(pa_stream_is_corked);
MAKE_FUNC(pa_stream_cork);
MAKE_FUNC(pa_stream_is_suspended);
MAKE_FUNC(pa_stream_get_device_name);
MAKE_FUNC(pa_stream_get_latency);
MAKE_FUNC(pa_path_get_filename);
MAKE_FUNC(pa_get_binary_name);
MAKE_FUNC(pa_threaded_mainloop_free);
MAKE_FUNC(pa_context_errno);
MAKE_FUNC(pa_xmalloc);
MAKE_FUNC(pa_stream_unref);
MAKE_FUNC(pa_threaded_mainloop_accept);
MAKE_FUNC(pa_stream_set_write_callback);
MAKE_FUNC(pa_threaded_mainloop_new);
MAKE_FUNC(pa_context_connect);
MAKE_FUNC(pa_stream_set_buffer_attr);
MAKE_FUNC(pa_stream_get_buffer_attr);
MAKE_FUNC(pa_stream_get_sample_spec);
MAKE_FUNC(pa_stream_get_time);
MAKE_FUNC(pa_stream_set_read_callback);
MAKE_FUNC(pa_stream_set_state_callback);
MAKE_FUNC(pa_stream_set_moved_callback);
MAKE_FUNC(pa_stream_set_underflow_callback);
MAKE_FUNC(pa_stream_new_with_proplist);
MAKE_FUNC(pa_stream_disconnect);
MAKE_FUNC(pa_threaded_mainloop_lock);
MAKE_FUNC(pa_channel_map_init_auto);
MAKE_FUNC(pa_channel_map_parse);
MAKE_FUNC(pa_channel_map_snprint);
MAKE_FUNC(pa_channel_map_equal);
MAKE_FUNC(pa_context_get_server_info);
MAKE_FUNC(pa_context_get_sink_info_by_name);
MAKE_FUNC(pa_context_get_sink_info_list);
MAKE_FUNC(pa_context_get_source_info_by_name);
MAKE_FUNC(pa_context_get_source_info_list);
MAKE_FUNC(pa_operation_get_state);
MAKE_FUNC(pa_operation_unref);
MAKE_FUNC(pa_proplist_new);
MAKE_FUNC(pa_proplist_free);
MAKE_FUNC(pa_proplist_set);
#if PA_CHECK_VERSION(0,9,15)
MAKE_FUNC(pa_channel_map_superset);
MAKE_FUNC(pa_stream_set_buffer_attr_callback);
#endif
#if PA_CHECK_VERSION(0,9,16)
MAKE_FUNC(pa_stream_begin_write);
#endif
#undef MAKE_FUNC

#define pa_context_unref ppa_context_unref
#define pa_sample_spec_valid ppa_sample_spec_valid
#define pa_frame_size ppa_frame_size
#define pa_stream_drop ppa_stream_drop
#define pa_strerror ppa_strerror
#define pa_context_get_state ppa_context_get_state
#define pa_stream_get_state ppa_stream_get_state
#define pa_threaded_mainloop_signal ppa_threaded_mainloop_signal
#define pa_stream_peek ppa_stream_peek
#define pa_threaded_mainloop_wait ppa_threaded_mainloop_wait
#define pa_threaded_mainloop_unlock ppa_threaded_mainloop_unlock
#define pa_threaded_mainloop_in_thread ppa_threaded_mainloop_in_thread
#define pa_context_new ppa_context_new
#define pa_threaded_mainloop_stop ppa_threaded_mainloop_stop
#define pa_context_disconnect ppa_context_disconnect
#define pa_threaded_mainloop_start ppa_threaded_mainloop_start
#define pa_threaded_mainloop_get_api ppa_threaded_mainloop_get_api
#define pa_context_set_state_callback ppa_context_set_state_callback
#define pa_stream_write ppa_stream_write
#define pa_xfree ppa_xfree
#define pa_stream_connect_record ppa_stream_connect_record
#define pa_stream_connect_playback ppa_stream_connect_playback
#define pa_stream_readable_size ppa_stream_readable_size
#define pa_stream_writable_size ppa_stream_writable_size
#define pa_stream_is_corked ppa_stream_is_corked
#define pa_stream_cork ppa_stream_cork
#define pa_stream_is_suspended ppa_stream_is_suspended
#define pa_stream_get_device_name ppa_stream_get_device_name
#define pa_stream_get_latency ppa_stream_get_latency
#define pa_path_get_filename ppa_path_get_filename
#define pa_get_binary_name ppa_get_binary_name
#define pa_threaded_mainloop_free ppa_threaded_mainloop_free
#define pa_context_errno ppa_context_errno
#define pa_xmalloc ppa_xmalloc
#define pa_stream_unref ppa_stream_unref
#define pa_threaded_mainloop_accept ppa_threaded_mainloop_accept
#define pa_stream_set_write_callback ppa_stream_set_write_callback
#define pa_threaded_mainloop_new ppa_threaded_mainloop_new
#define pa_context_connect ppa_context_connect
#define pa_stream_set_buffer_attr ppa_stream_set_buffer_attr
#define pa_stream_get_buffer_attr ppa_stream_get_buffer_attr
#define pa_stream_get_sample_spec ppa_stream_get_sample_spec
#define pa_stream_get_time ppa_stream_get_time
#define pa_stream_set_read_callback ppa_stream_set_read_callback
#define pa_stream_set_state_callback ppa_stream_set_state_callback
#define pa_stream_set_moved_callback ppa_stream_set_moved_callback
#define pa_stream_set_underflow_callback ppa_stream_set_underflow_callback
#define pa_stream_new_with_proplist ppa_stream_new_with_proplist
#define pa_stream_disconnect ppa_stream_disconnect
#define pa_threaded_mainloop_lock ppa_threaded_mainloop_lock
#define pa_channel_map_init_auto ppa_channel_map_init_auto
#define pa_channel_map_parse ppa_channel_map_parse
#define pa_channel_map_snprint ppa_channel_map_snprint
#define pa_channel_map_equal ppa_channel_map_equal
#define pa_context_get_server_info ppa_context_get_server_info
#define pa_context_get_sink_info_by_name ppa_context_get_sink_info_by_name
#define pa_context_get_sink_info_list ppa_context_get_sink_info_list
#define pa_context_get_source_info_by_name ppa_context_get_source_info_by_name
#define pa_context_get_source_info_list ppa_context_get_source_info_list
#define pa_operation_get_state ppa_operation_get_state
#define pa_operation_unref ppa_operation_unref
#define pa_proplist_new ppa_proplist_new
#define pa_proplist_free ppa_proplist_free
#define pa_proplist_set ppa_proplist_set
#if PA_CHECK_VERSION(0,9,15)
#define pa_channel_map_superset ppa_channel_map_superset
#define pa_stream_set_buffer_attr_callback ppa_stream_set_buffer_attr_callback
#endif
#if PA_CHECK_VERSION(0,9,16)
#define pa_stream_begin_write ppa_stream_begin_write
#endif

#endif

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

typedef struct {
    char *device_name;

    const void *cap_store;
    size_t cap_len;
    size_t cap_remain;

    ALCuint last_readable;

    pa_buffer_attr attr;
    pa_sample_spec spec;

    pa_threaded_mainloop *loop;

    ALvoid *thread;
    volatile ALboolean killNow;

    pa_stream *stream;
    pa_context *context;
} pulse_data;

typedef struct {
    char *name;
    char *device_name;
} DevMap;


static DevMap *allDevNameMap;
static ALuint numDevNames;
static DevMap *allCaptureDevNameMap;
static ALuint numCaptureDevNames;
static pa_context_flags_t pulse_ctx_flags;
static pa_proplist *prop_filter;


static ALCboolean pulse_load(void)
{
    ALCboolean ret = ALC_TRUE;
#ifdef HAVE_DYNLOAD
    if(!pa_handle)
    {
#ifdef _WIN32
#define PALIB "libpulse-0.dll"
#elif defined(__APPLE__) && defined(__MACH__)
#define PALIB "libpulse.0.dylib"
#else
#define PALIB "libpulse.so.0"
#endif
        pa_handle = LoadLib(PALIB);
        if(!pa_handle)
            return ALC_FALSE;

#define LOAD_FUNC(x) do {                                                     \
    p##x = GetSymbol(pa_handle, #x);                                          \
    if(!(p##x)) {                                                             \
        ret = ALC_FALSE;                                                      \
    }                                                                         \
} while(0)
        LOAD_FUNC(pa_context_unref);
        LOAD_FUNC(pa_sample_spec_valid);
        LOAD_FUNC(pa_stream_drop);
        LOAD_FUNC(pa_frame_size);
        LOAD_FUNC(pa_strerror);
        LOAD_FUNC(pa_context_get_state);
        LOAD_FUNC(pa_stream_get_state);
        LOAD_FUNC(pa_threaded_mainloop_signal);
        LOAD_FUNC(pa_stream_peek);
        LOAD_FUNC(pa_threaded_mainloop_wait);
        LOAD_FUNC(pa_threaded_mainloop_unlock);
        LOAD_FUNC(pa_threaded_mainloop_in_thread);
        LOAD_FUNC(pa_context_new);
        LOAD_FUNC(pa_threaded_mainloop_stop);
        LOAD_FUNC(pa_context_disconnect);
        LOAD_FUNC(pa_threaded_mainloop_start);
        LOAD_FUNC(pa_threaded_mainloop_get_api);
        LOAD_FUNC(pa_context_set_state_callback);
        LOAD_FUNC(pa_stream_write);
        LOAD_FUNC(pa_xfree);
        LOAD_FUNC(pa_stream_connect_record);
        LOAD_FUNC(pa_stream_connect_playback);
        LOAD_FUNC(pa_stream_readable_size);
        LOAD_FUNC(pa_stream_writable_size);
        LOAD_FUNC(pa_stream_is_corked);
        LOAD_FUNC(pa_stream_cork);
        LOAD_FUNC(pa_stream_is_suspended);
        LOAD_FUNC(pa_stream_get_device_name);
        LOAD_FUNC(pa_stream_get_latency);
        LOAD_FUNC(pa_path_get_filename);
        LOAD_FUNC(pa_get_binary_name);
        LOAD_FUNC(pa_threaded_mainloop_free);
        LOAD_FUNC(pa_context_errno);
        LOAD_FUNC(pa_xmalloc);
        LOAD_FUNC(pa_stream_unref);
        LOAD_FUNC(pa_threaded_mainloop_accept);
        LOAD_FUNC(pa_stream_set_write_callback);
        LOAD_FUNC(pa_threaded_mainloop_new);
        LOAD_FUNC(pa_context_connect);
        LOAD_FUNC(pa_stream_set_buffer_attr);
        LOAD_FUNC(pa_stream_get_buffer_attr);
        LOAD_FUNC(pa_stream_get_sample_spec);
        LOAD_FUNC(pa_stream_get_time);
        LOAD_FUNC(pa_stream_set_read_callback);
        LOAD_FUNC(pa_stream_set_state_callback);
        LOAD_FUNC(pa_stream_set_moved_callback);
        LOAD_FUNC(pa_stream_set_underflow_callback);
        LOAD_FUNC(pa_stream_new_with_proplist);
        LOAD_FUNC(pa_stream_disconnect);
        LOAD_FUNC(pa_threaded_mainloop_lock);
        LOAD_FUNC(pa_channel_map_init_auto);
        LOAD_FUNC(pa_channel_map_parse);
        LOAD_FUNC(pa_channel_map_snprint);
        LOAD_FUNC(pa_channel_map_equal);
        LOAD_FUNC(pa_context_get_server_info);
        LOAD_FUNC(pa_context_get_sink_info_by_name);
        LOAD_FUNC(pa_context_get_sink_info_list);
        LOAD_FUNC(pa_context_get_source_info_by_name);
        LOAD_FUNC(pa_context_get_source_info_list);
        LOAD_FUNC(pa_operation_get_state);
        LOAD_FUNC(pa_operation_unref);
        LOAD_FUNC(pa_proplist_new);
        LOAD_FUNC(pa_proplist_free);
        LOAD_FUNC(pa_proplist_set);
#undef LOAD_FUNC
#define LOAD_OPTIONAL_FUNC(x) do {                                            \
    p##x = GetSymbol(pa_handle, #x);                                          \
} while(0)
#if PA_CHECK_VERSION(0,9,15)
        LOAD_OPTIONAL_FUNC(pa_channel_map_superset);
        LOAD_OPTIONAL_FUNC(pa_stream_set_buffer_attr_callback);
#endif
#if PA_CHECK_VERSION(0,9,16)
        LOAD_OPTIONAL_FUNC(pa_stream_begin_write);
#endif
#undef LOAD_OPTIONAL_FUNC

        if(ret == ALC_FALSE)
        {
            CloseLib(pa_handle);
            pa_handle = NULL;
        }
    }
#endif /* HAVE_DYNLOAD */
    return ret;
}

/* PulseAudio Event Callbacks */
static void context_state_callback(pa_context *context, void *pdata)
{
    pa_threaded_mainloop *loop = pdata;
    pa_context_state_t state;

    state = pa_context_get_state(context);
    if(state == PA_CONTEXT_READY || !PA_CONTEXT_IS_GOOD(state))
        pa_threaded_mainloop_signal(loop, 0);
}

static void stream_state_callback(pa_stream *stream, void *pdata)
{
    pa_threaded_mainloop *loop = pdata;
    pa_stream_state_t state;

    state = pa_stream_get_state(stream);
    if(state == PA_STREAM_READY || !PA_STREAM_IS_GOOD(state))
        pa_threaded_mainloop_signal(loop, 0);
}

static void stream_buffer_attr_callback(pa_stream *stream, void *pdata)
{
    ALCdevice *device = pdata;
    pulse_data *data = device->ExtraData;

    data->attr = *pa_stream_get_buffer_attr(stream);
    TRACE("minreq=%d, tlength=%d, prebuf=%d\n", data->attr.minreq, data->attr.tlength, data->attr.prebuf);
}

static void context_state_callback2(pa_context *context, void *pdata)
{
    ALCdevice *Device = pdata;
    pulse_data *data = Device->ExtraData;

    if(pa_context_get_state(context) == PA_CONTEXT_FAILED)
    {
        ERR("Received context failure!\n");
        aluHandleDisconnect(Device);
    }
    pa_threaded_mainloop_signal(data->loop, 0);
}

static void stream_state_callback2(pa_stream *stream, void *pdata)
{
    ALCdevice *Device = pdata;
    pulse_data *data = Device->ExtraData;

    if(pa_stream_get_state(stream) == PA_STREAM_FAILED)
    {
        ERR("Received stream failure!\n");
        aluHandleDisconnect(Device);
    }
    pa_threaded_mainloop_signal(data->loop, 0);
}

static void stream_success_callback(pa_stream *stream, int success, void *pdata)
{
    ALCdevice *Device = pdata;
    pulse_data *data = Device->ExtraData;
    (void)stream;
    (void)success;

    pa_threaded_mainloop_signal(data->loop, 0);
}

static void sink_info_callback(pa_context *context, const pa_sink_info *info, int eol, void *pdata)
{
    ALCdevice *device = pdata;
    pulse_data *data = device->ExtraData;
    char chanmap_str[256] = "";
    const struct {
        const char *str;
        enum DevFmtChannels chans;
    } chanmaps[] = {
        { "front-left,front-right,front-center,lfe,rear-left,rear-right,side-left,side-right",
          DevFmtX71 },
        { "front-left,front-right,front-center,lfe,rear-center,side-left,side-right",
          DevFmtX61 },
        { "front-left,front-right,front-center,lfe,rear-left,rear-right",
          DevFmtX51 },
        { "front-left,front-right,front-center,lfe,side-left,side-right",
          DevFmtX51Side },
        { "front-left,front-right,rear-left,rear-right", DevFmtQuad },
        { "front-left,front-right", DevFmtStereo },
        { "mono", DevFmtMono },
        { NULL, 0 }
    };
    int i;
    (void)context;

    if(eol)
    {
        pa_threaded_mainloop_signal(data->loop, 0);
        return;
    }

    for(i = 0;chanmaps[i].str;i++)
    {
        pa_channel_map map;
        if(!pa_channel_map_parse(&map, chanmaps[i].str))
            continue;

        if(pa_channel_map_equal(&info->channel_map, &map)
#if PA_CHECK_VERSION(0,9,15)
           || (pa_channel_map_superset &&
               pa_channel_map_superset(&info->channel_map, &map))
#endif
            )
        {
            device->FmtChans = chanmaps[i].chans;
            return;
        }
    }

    pa_channel_map_snprint(chanmap_str, sizeof(chanmap_str), &info->channel_map);
    ERR("Failed to find format for channel map:\n    %s\n", chanmap_str);
}

static void sink_device_callback(pa_context *context, const pa_sink_info *info, int eol, void *pdata)
{
    pa_threaded_mainloop *loop = pdata;
    void *temp;
    ALuint i;

    (void)context;

    if(eol)
    {
        pa_threaded_mainloop_signal(loop, 0);
        return;
    }

    for(i = 0;i < numDevNames;i++)
    {
        if(strcmp(info->name, allDevNameMap[i].device_name) == 0)
            return;
    }

    TRACE("Got device \"%s\", \"%s\"\n", info->description, info->name);

    temp = realloc(allDevNameMap, (numDevNames+1) * sizeof(*allDevNameMap));
    if(temp)
    {
        allDevNameMap = temp;
        allDevNameMap[numDevNames].name = strdup(info->description);
        allDevNameMap[numDevNames].device_name = strdup(info->name);
        numDevNames++;
    }
}

static void source_device_callback(pa_context *context, const pa_source_info *info, int eol, void *pdata)
{
    pa_threaded_mainloop *loop = pdata;
    void *temp;
    ALuint i;

    (void)context;

    if(eol)
    {
        pa_threaded_mainloop_signal(loop, 0);
        return;
    }

    for(i = 0;i < numCaptureDevNames;i++)
    {
        if(strcmp(info->name, allCaptureDevNameMap[i].device_name) == 0)
            return;
    }

    TRACE("Got device \"%s\", \"%s\"\n", info->description, info->name);

    temp = realloc(allCaptureDevNameMap, (numCaptureDevNames+1) * sizeof(*allCaptureDevNameMap));
    if(temp)
    {
        allCaptureDevNameMap = temp;
        allCaptureDevNameMap[numCaptureDevNames].name = strdup(info->description);
        allCaptureDevNameMap[numCaptureDevNames].device_name = strdup(info->name);
        numCaptureDevNames++;
    }
}

static void sink_name_callback(pa_context *context, const pa_sink_info *info, int eol, void *pdata)
{
    ALCdevice *device = pdata;
    pulse_data *data = device->ExtraData;
    (void)context;

    if(eol)
    {
        pa_threaded_mainloop_signal(data->loop, 0);
        return;
    }

    free(device->DeviceName);
    device->DeviceName = strdup(info->description);
}

static void source_name_callback(pa_context *context, const pa_source_info *info, int eol, void *pdata)
{
    ALCdevice *device = pdata;
    pulse_data *data = device->ExtraData;
    (void)context;

    if(eol)
    {
        pa_threaded_mainloop_signal(data->loop, 0);
        return;
    }

    free(device->DeviceName);
    device->DeviceName = strdup(info->description);
}


static void stream_moved_callback(pa_stream *stream, void *pdata)
{
    ALCdevice *device = pdata;
    pulse_data *data = device->ExtraData;
    (void)stream;

    free(data->device_name);
    data->device_name = strdup(pa_stream_get_device_name(data->stream));

    TRACE("Stream moved to %s\n", data->device_name);
}


static pa_context *connect_context(pa_threaded_mainloop *loop, ALboolean silent)
{
    const char *name = "OpenAL Soft";
    char path_name[PATH_MAX];
    pa_context_state_t state;
    pa_context *context;
    int err;

    if(pa_get_binary_name(path_name, sizeof(path_name)))
        name = pa_path_get_filename(path_name);

    context = pa_context_new(pa_threaded_mainloop_get_api(loop), name);
    if(!context)
    {
        ERR("pa_context_new() failed\n");
        return NULL;
    }

    pa_context_set_state_callback(context, context_state_callback, loop);

    if((err=pa_context_connect(context, NULL, pulse_ctx_flags, NULL)) >= 0)
    {
        while((state=pa_context_get_state(context)) != PA_CONTEXT_READY)
        {
            if(!PA_CONTEXT_IS_GOOD(state))
            {
                err = pa_context_errno(context);
                if(err > 0)  err = -err;
                break;
            }

            pa_threaded_mainloop_wait(loop);
        }
    }
    pa_context_set_state_callback(context, NULL, NULL);

    if(err < 0)
    {
        if(!silent)
            ERR("Context did not connect: %s\n", pa_strerror(err));
        pa_context_unref(context);
        return NULL;
    }

    return context;
}

static pa_stream *connect_playback_stream(const char *device_name,
    pa_threaded_mainloop *loop, pa_context *context,
    pa_stream_flags_t flags, pa_buffer_attr *attr, pa_sample_spec *spec,
    pa_channel_map *chanmap)
{
    pa_stream_state_t state;
    pa_stream *stream;

    stream = pa_stream_new_with_proplist(context, "Playback Stream", spec, chanmap, prop_filter);
    if(!stream)
    {
        ERR("pa_stream_new_with_proplist() failed: %s\n", pa_strerror(pa_context_errno(context)));
        return NULL;
    }

    pa_stream_set_state_callback(stream, stream_state_callback, loop);

    if(pa_stream_connect_playback(stream, device_name, attr, flags, NULL, NULL) < 0)
    {
        ERR("Stream did not connect: %s\n", pa_strerror(pa_context_errno(context)));
        pa_stream_unref(stream);
        return NULL;
    }

    while((state=pa_stream_get_state(stream)) != PA_STREAM_READY)
    {
        if(!PA_STREAM_IS_GOOD(state))
        {
            ERR("Stream did not get ready: %s\n", pa_strerror(pa_context_errno(context)));
            pa_stream_unref(stream);
            return NULL;
        }

        pa_threaded_mainloop_wait(loop);
    }
    pa_stream_set_state_callback(stream, NULL, NULL);

    return stream;
}

static pa_stream *connect_record_stream(const char *device_name,
    pa_threaded_mainloop *loop, pa_context *context,
    pa_stream_flags_t flags, pa_buffer_attr *attr, pa_sample_spec *spec,
    pa_channel_map *chanmap)
{
    pa_stream_state_t state;
    pa_stream *stream;

    stream = pa_stream_new_with_proplist(context, "Capture Stream", spec, chanmap, prop_filter);
    if(!stream)
    {
        ERR("pa_stream_new_with_proplist() failed: %s\n", pa_strerror(pa_context_errno(context)));
        return NULL;
    }

    pa_stream_set_state_callback(stream, stream_state_callback, loop);

    if(pa_stream_connect_record(stream, device_name, attr, flags) < 0)
    {
        ERR("Stream did not connect: %s\n", pa_strerror(pa_context_errno(context)));
        pa_stream_unref(stream);
        return NULL;
    }

    while((state=pa_stream_get_state(stream)) != PA_STREAM_READY)
    {
        if(!PA_STREAM_IS_GOOD(state))
        {
            ERR("Stream did not get ready: %s\n", pa_strerror(pa_context_errno(context)));
            pa_stream_unref(stream);
            return NULL;
        }

        pa_threaded_mainloop_wait(loop);
    }
    pa_stream_set_state_callback(stream, NULL, NULL);

    return stream;
}


static void wait_for_operation(pa_operation *op, pa_threaded_mainloop *loop)
{
    if(op)
    {
        while(pa_operation_get_state(op) == PA_OPERATION_RUNNING)
            pa_threaded_mainloop_wait(loop);
        pa_operation_unref(op);
    }
}


static void probe_devices(ALboolean capture)
{
    pa_threaded_mainloop *loop;

    if(capture == AL_FALSE)
        allDevNameMap = malloc(sizeof(DevMap) * 1);
    else
        allCaptureDevNameMap = malloc(sizeof(DevMap) * 1);

    if((loop=pa_threaded_mainloop_new()) &&
       pa_threaded_mainloop_start(loop) >= 0)
    {
        pa_context *context;

        pa_threaded_mainloop_lock(loop);
        context = connect_context(loop, AL_FALSE);
        if(context)
        {
            pa_operation *o;

            if(capture == AL_FALSE)
            {
                pa_stream_flags_t flags;
                pa_sample_spec spec;
                pa_stream *stream;

                flags = PA_STREAM_FIX_FORMAT | PA_STREAM_FIX_RATE |
                        PA_STREAM_FIX_CHANNELS | PA_STREAM_DONT_MOVE;

                spec.format = PA_SAMPLE_S16NE;
                spec.rate = 44100;
                spec.channels = 2;

                stream = connect_playback_stream(NULL, loop, context, flags,
                                                 NULL, &spec, NULL);
                if(stream)
                {
                    o = pa_context_get_sink_info_by_name(context, pa_stream_get_device_name(stream), sink_device_callback, loop);
                    wait_for_operation(o, loop);

                    pa_stream_disconnect(stream);
                    pa_stream_unref(stream);
                    stream = NULL;
                }

                o = pa_context_get_sink_info_list(context, sink_device_callback, loop);
            }
            else
            {
                pa_stream_flags_t flags;
                pa_sample_spec spec;
                pa_stream *stream;

                flags = PA_STREAM_FIX_FORMAT | PA_STREAM_FIX_RATE |
                        PA_STREAM_FIX_CHANNELS | PA_STREAM_DONT_MOVE;

                spec.format = PA_SAMPLE_S16NE;
                spec.rate = 44100;
                spec.channels = 1;

                stream = connect_record_stream(NULL, loop, context, flags,
                                               NULL, &spec, NULL);
                if(stream)
                {
                    o = pa_context_get_source_info_by_name(context, pa_stream_get_device_name(stream), source_device_callback, loop);
                    wait_for_operation(o, loop);

                    pa_stream_disconnect(stream);
                    pa_stream_unref(stream);
                    stream = NULL;
                }

                o = pa_context_get_source_info_list(context, source_device_callback, loop);
            }
            wait_for_operation(o, loop);

            pa_context_disconnect(context);
            pa_context_unref(context);
        }
        pa_threaded_mainloop_unlock(loop);
        pa_threaded_mainloop_stop(loop);
    }
    if(loop)
        pa_threaded_mainloop_free(loop);
}


static ALuint PulseProc(ALvoid *param)
{
    ALCdevice *Device = param;
    pulse_data *data = Device->ExtraData;
    ALuint buffer_size;
    ALint update_size;
    size_t frame_size;
    ssize_t len;

    SetRTPriority();

    pa_threaded_mainloop_lock(data->loop);
    frame_size = pa_frame_size(&data->spec);
    update_size = Device->UpdateSize * frame_size;

    /* Sanitize buffer metrics, in case we actually have less than what we
     * asked for. */
    buffer_size = minu(update_size*Device->NumUpdates, data->attr.tlength);
    update_size = minu(update_size, buffer_size/2);
    do {
        len = pa_stream_writable_size(data->stream) - data->attr.tlength +
              buffer_size;
        if(len < update_size)
        {
            if(pa_stream_is_corked(data->stream) == 1)
            {
                pa_operation *o;
                o = pa_stream_cork(data->stream, 0, NULL, NULL);
                if(o) pa_operation_unref(o);
            }
            pa_threaded_mainloop_unlock(data->loop);
            Sleep(1);
            pa_threaded_mainloop_lock(data->loop);
            continue;
        }
        len -= len%update_size;

        while(len > 0)
        {
            size_t newlen = len;
            void *buf;
            pa_free_cb_t free_func = NULL;

#if PA_CHECK_VERSION(0,9,16)
            if(!pa_stream_begin_write ||
               pa_stream_begin_write(data->stream, &buf, &newlen) < 0)
#endif
            {
                buf = pa_xmalloc(newlen);
                free_func = pa_xfree;
            }

            aluMixData(Device, buf, newlen/frame_size);

            pa_stream_write(data->stream, buf, newlen, free_func, 0, PA_SEEK_RELATIVE);
            len -= newlen;
        }
    } while(!data->killNow && Device->Connected);
    pa_threaded_mainloop_unlock(data->loop);

    return 0;
}


static ALCboolean pulse_open(ALCdevice *device)
{
    pulse_data *data = pa_xmalloc(sizeof(pulse_data));
    memset(data, 0, sizeof(*data));

    if(!(data->loop = pa_threaded_mainloop_new()))
    {
        ERR("pa_threaded_mainloop_new() failed!\n");
        goto out;
    }
    if(pa_threaded_mainloop_start(data->loop) < 0)
    {
        ERR("pa_threaded_mainloop_start() failed\n");
        goto out;
    }

    pa_threaded_mainloop_lock(data->loop);
    device->ExtraData = data;

    data->context = connect_context(data->loop, AL_FALSE);
    if(!data->context)
    {
        pa_threaded_mainloop_unlock(data->loop);
        goto out;
    }
    pa_context_set_state_callback(data->context, context_state_callback2, device);

    pa_threaded_mainloop_unlock(data->loop);
    return ALC_TRUE;

out:
    if(data->loop)
    {
        pa_threaded_mainloop_stop(data->loop);
        pa_threaded_mainloop_free(data->loop);
    }

    device->ExtraData = NULL;
    pa_xfree(data);
    return ALC_FALSE;
}

static void pulse_close(ALCdevice *device)
{
    pulse_data *data = device->ExtraData;

    pa_threaded_mainloop_lock(data->loop);

    if(data->stream)
    {
        pa_stream_set_moved_callback(data->stream, NULL, NULL);
#if PA_CHECK_VERSION(0,9,15)
        if(pa_stream_set_buffer_attr_callback)
            pa_stream_set_buffer_attr_callback(data->stream, NULL, NULL);
#endif
        pa_stream_disconnect(data->stream);
        pa_stream_unref(data->stream);
    }

    pa_context_disconnect(data->context);
    pa_context_unref(data->context);

    pa_threaded_mainloop_unlock(data->loop);

    pa_threaded_mainloop_stop(data->loop);
    pa_threaded_mainloop_free(data->loop);

    free(data->device_name);

    device->ExtraData = NULL;
    pa_xfree(data);
}

/* OpenAL */
static ALCenum pulse_open_playback(ALCdevice *device, const ALCchar *device_name)
{
    const char *pulse_name = NULL;
    pa_stream_flags_t flags;
    pa_sample_spec spec;
    pulse_data *data;
    pa_operation *o;

    if(device_name)
    {
        ALuint i;

        if(!allDevNameMap)
            probe_devices(AL_FALSE);

        for(i = 0;i < numDevNames;i++)
        {
            if(strcmp(device_name, allDevNameMap[i].name) == 0)
            {
                pulse_name = allDevNameMap[i].device_name;
                break;
            }
        }
        if(i == numDevNames)
            return ALC_INVALID_VALUE;
    }

    if(pulse_open(device) == ALC_FALSE)
        return ALC_INVALID_VALUE;

    data = device->ExtraData;
    pa_threaded_mainloop_lock(data->loop);

    flags = PA_STREAM_FIX_FORMAT | PA_STREAM_FIX_RATE |
            PA_STREAM_FIX_CHANNELS;
    if(!GetConfigValueBool("pulse", "allow-moves", 0))
        flags |= PA_STREAM_DONT_MOVE;

    spec.format = PA_SAMPLE_S16NE;
    spec.rate = 44100;
    spec.channels = 2;

    data->stream = connect_playback_stream(pulse_name, data->loop, data->context,
                                           flags, NULL, &spec, NULL);
    if(!data->stream)
    {
        pa_threaded_mainloop_unlock(data->loop);
        pulse_close(device);
        return ALC_INVALID_VALUE;
    }

    data->device_name = strdup(pa_stream_get_device_name(data->stream));
    o = pa_context_get_sink_info_by_name(data->context, data->device_name,
                                         sink_name_callback, device);
    wait_for_operation(o, data->loop);

    pa_stream_set_moved_callback(data->stream, stream_moved_callback, device);

    pa_threaded_mainloop_unlock(data->loop);

    return ALC_NO_ERROR;
}

static void pulse_close_playback(ALCdevice *device)
{
    pulse_close(device);
}

static ALCboolean pulse_reset_playback(ALCdevice *device)
{
    pulse_data *data = device->ExtraData;
    pa_stream_flags_t flags = 0;
    pa_channel_map chanmap;
    const char *mapname;
    ALuint len;

    pa_threaded_mainloop_lock(data->loop);

    if(data->stream)
    {
        pa_stream_set_moved_callback(data->stream, NULL, NULL);
#if PA_CHECK_VERSION(0,9,15)
        if(pa_stream_set_buffer_attr_callback)
            pa_stream_set_buffer_attr_callback(data->stream, NULL, NULL);
#endif
        pa_stream_disconnect(data->stream);
        pa_stream_unref(data->stream);
        data->stream = NULL;
    }

    if(!(device->Flags&DEVICE_CHANNELS_REQUEST))
    {
        pa_operation *o;
        o = pa_context_get_sink_info_by_name(data->context, data->device_name, sink_info_callback, device);
        wait_for_operation(o, data->loop);
    }
    if(!(device->Flags&DEVICE_FREQUENCY_REQUEST))
        flags |= PA_STREAM_FIX_RATE;

    flags |= PA_STREAM_INTERPOLATE_TIMING | PA_STREAM_AUTO_TIMING_UPDATE;
    flags |= PA_STREAM_ADJUST_LATENCY;
    flags |= PA_STREAM_START_CORKED;
    if(!GetConfigValueBool("pulse", "allow-moves", 0))
        flags |= PA_STREAM_DONT_MOVE;

    switch(device->FmtType)
    {
        case DevFmtByte:
            device->FmtType = DevFmtUByte;
            /* fall-through */
        case DevFmtUByte:
            data->spec.format = PA_SAMPLE_U8;
            break;
        case DevFmtUShort:
            device->FmtType = DevFmtShort;
            /* fall-through */
        case DevFmtShort:
            data->spec.format = PA_SAMPLE_S16NE;
            break;
        case DevFmtUInt:
            device->FmtType = DevFmtInt;
            /* fall-through */
        case DevFmtInt:
            data->spec.format = PA_SAMPLE_S32NE;
            break;
        case DevFmtFloat:
            data->spec.format = PA_SAMPLE_FLOAT32NE;
            break;
    }
    data->spec.rate = device->Frequency;
    data->spec.channels = ChannelsFromDevFmt(device->FmtChans);

    if(pa_sample_spec_valid(&data->spec) == 0)
    {
        ERR("Invalid sample format\n");
        pa_threaded_mainloop_unlock(data->loop);
        return ALC_FALSE;
    }

    mapname = "(invalid)";
    switch(device->FmtChans)
    {
        case DevFmtMono:
            mapname = "mono";
            break;
        case DevFmtStereo:
            mapname = "front-left,front-right";
            break;
        case DevFmtQuad:
            mapname = "front-left,front-right,rear-left,rear-right";
            break;
        case DevFmtX51:
            mapname = "front-left,front-right,front-center,lfe,rear-left,rear-right";
            break;
        case DevFmtX51Side:
            mapname = "front-left,front-right,front-center,lfe,side-left,side-right";
            break;
        case DevFmtX61:
            mapname = "front-left,front-right,front-center,lfe,rear-center,side-left,side-right";
            break;
        case DevFmtX71:
            mapname = "front-left,front-right,front-center,lfe,rear-left,rear-right,side-left,side-right";
            break;
    }
    if(!pa_channel_map_parse(&chanmap, mapname))
    {
        ERR("Failed to build channel map for %s\n", DevFmtChannelsString(device->FmtChans));
        pa_threaded_mainloop_unlock(data->loop);
        return ALC_FALSE;
    }
    SetDefaultWFXChannelOrder(device);

    data->attr.fragsize = -1;
    data->attr.prebuf = 0;
    data->attr.minreq = device->UpdateSize * pa_frame_size(&data->spec);
    data->attr.tlength = data->attr.minreq * maxu(device->NumUpdates, 2);
    data->attr.maxlength = -1;

    data->stream = connect_playback_stream(data->device_name, data->loop,
                                           data->context, flags, &data->attr,
                                           &data->spec, &chanmap);
    if(!data->stream)
    {
        pa_threaded_mainloop_unlock(data->loop);
        return ALC_FALSE;
    }
    pa_stream_set_state_callback(data->stream, stream_state_callback2, device);

    data->spec = *(pa_stream_get_sample_spec(data->stream));
    if(device->Frequency != data->spec.rate)
    {
        pa_operation *o;

        /* Server updated our playback rate, so modify the buffer attribs
         * accordingly. */
        device->NumUpdates = (ALuint)((ALdouble)device->NumUpdates / device->Frequency *
                                      data->spec.rate + 0.5);
        data->attr.minreq  = device->UpdateSize * pa_frame_size(&data->spec);
        data->attr.tlength = data->attr.minreq * clampu(device->NumUpdates, 2, 16);
        data->attr.maxlength = -1;
        data->attr.prebuf  = 0;

        o = pa_stream_set_buffer_attr(data->stream, &data->attr,
                                      stream_success_callback, device);
        wait_for_operation(o, data->loop);

        device->Frequency = data->spec.rate;
    }

    pa_stream_set_moved_callback(data->stream, stream_moved_callback, device);
#if PA_CHECK_VERSION(0,9,15)
    if(pa_stream_set_buffer_attr_callback)
        pa_stream_set_buffer_attr_callback(data->stream, stream_buffer_attr_callback, device);
#endif
    stream_buffer_attr_callback(data->stream, device);

    len = data->attr.minreq / pa_frame_size(&data->spec);
    if((CPUCapFlags&CPU_CAP_SSE))
        len = (len+3)&~3;
    device->NumUpdates = (ALuint)((ALdouble)device->NumUpdates/len*device->UpdateSize + 0.5);
    device->NumUpdates = clampu(device->NumUpdates, 2, 16);
    device->UpdateSize = len;

    pa_threaded_mainloop_unlock(data->loop);
    return ALC_TRUE;
}

static ALCboolean pulse_start_playback(ALCdevice *device)
{
    pulse_data *data = device->ExtraData;

    data->thread = StartThread(PulseProc, device);
    if(!data->thread)
        return ALC_FALSE;

    return ALC_TRUE;
}

static void pulse_stop_playback(ALCdevice *device)
{
    pulse_data *data = device->ExtraData;
    pa_operation *o;

    if(!data->stream)
        return;

    data->killNow = AL_TRUE;
    if(data->thread)
    {
        StopThread(data->thread);
        data->thread = NULL;
    }
    data->killNow = AL_FALSE;

    pa_threaded_mainloop_lock(data->loop);

    o = pa_stream_cork(data->stream, 1, stream_success_callback, device);
    wait_for_operation(o, data->loop);

    pa_threaded_mainloop_unlock(data->loop);
}


static ALCenum pulse_open_capture(ALCdevice *device, const ALCchar *device_name)
{
    const char *pulse_name = NULL;
    pa_stream_flags_t flags = 0;
    pa_channel_map chanmap;
    pulse_data *data;
    pa_operation *o;
    ALuint samples;

    if(device_name)
    {
        ALuint i;

        if(!allCaptureDevNameMap)
            probe_devices(AL_TRUE);

        for(i = 0;i < numCaptureDevNames;i++)
        {
            if(strcmp(device_name, allCaptureDevNameMap[i].name) == 0)
            {
                pulse_name = allCaptureDevNameMap[i].device_name;
                break;
            }
        }
        if(i == numCaptureDevNames)
            return ALC_INVALID_VALUE;
    }

    if(pulse_open(device) == ALC_FALSE)
        return ALC_INVALID_VALUE;

    data = device->ExtraData;
    pa_threaded_mainloop_lock(data->loop);

    data->spec.rate = device->Frequency;
    data->spec.channels = ChannelsFromDevFmt(device->FmtChans);

    switch(device->FmtType)
    {
        case DevFmtUByte:
            data->spec.format = PA_SAMPLE_U8;
            break;
        case DevFmtShort:
            data->spec.format = PA_SAMPLE_S16NE;
            break;
        case DevFmtInt:
            data->spec.format = PA_SAMPLE_S32NE;
            break;
        case DevFmtFloat:
            data->spec.format = PA_SAMPLE_FLOAT32NE;
            break;
        case DevFmtByte:
        case DevFmtUShort:
        case DevFmtUInt:
            ERR("%s capture samples not supported\n", DevFmtTypeString(device->FmtType));
            pa_threaded_mainloop_unlock(data->loop);
            goto fail;
    }

    if(pa_sample_spec_valid(&data->spec) == 0)
    {
        ERR("Invalid sample format\n");
        pa_threaded_mainloop_unlock(data->loop);
        goto fail;
    }

    if(!pa_channel_map_init_auto(&chanmap, data->spec.channels, PA_CHANNEL_MAP_WAVEEX))
    {
        ERR("Couldn't build map for channel count (%d)!\n", data->spec.channels);
        pa_threaded_mainloop_unlock(data->loop);
        goto fail;
    }

    samples = device->UpdateSize * device->NumUpdates;
    samples = maxu(samples, 100 * device->Frequency / 1000);

    data->attr.minreq = -1;
    data->attr.prebuf = -1;
    data->attr.maxlength = samples * pa_frame_size(&data->spec);
    data->attr.tlength = -1;
    data->attr.fragsize = minu(samples, 50*device->Frequency/1000) *
                          pa_frame_size(&data->spec);

    flags |= PA_STREAM_START_CORKED|PA_STREAM_ADJUST_LATENCY;
    if(!GetConfigValueBool("pulse", "allow-moves", 0))
        flags |= PA_STREAM_DONT_MOVE;

    data->stream = connect_record_stream(pulse_name, data->loop, data->context,
                                         flags, &data->attr, &data->spec,
                                         &chanmap);
    if(!data->stream)
    {
        pa_threaded_mainloop_unlock(data->loop);
        goto fail;
    }
    pa_stream_set_state_callback(data->stream, stream_state_callback2, device);

    data->device_name = strdup(pa_stream_get_device_name(data->stream));
    o = pa_context_get_source_info_by_name(data->context, data->device_name,
                                           source_name_callback, device);
    wait_for_operation(o, data->loop);

    pa_stream_set_moved_callback(data->stream, stream_moved_callback, device);

    pa_threaded_mainloop_unlock(data->loop);
    return ALC_NO_ERROR;

fail:
    pulse_close(device);
    return ALC_INVALID_VALUE;
}

static void pulse_close_capture(ALCdevice *device)
{
    pulse_close(device);
}

static void pulse_start_capture(ALCdevice *device)
{
    pulse_data *data = device->ExtraData;
    pa_operation *o;

    o = pa_stream_cork(data->stream, 0, stream_success_callback, device);
    wait_for_operation(o, data->loop);
}

static void pulse_stop_capture(ALCdevice *device)
{
    pulse_data *data = device->ExtraData;
    pa_operation *o;

    o = pa_stream_cork(data->stream, 1, stream_success_callback, device);
    wait_for_operation(o, data->loop);
}

static ALCenum pulse_capture_samples(ALCdevice *device, ALCvoid *buffer, ALCuint samples)
{
    pulse_data *data = device->ExtraData;
    ALCuint todo = samples * pa_frame_size(&data->spec);

    /* Capture is done in fragment-sized chunks, so we loop until we get all
     * that's available */
    data->last_readable -= todo;
    while(todo > 0)
    {
        size_t rem = todo;

        if(data->cap_len == 0)
        {
            pa_stream_state_t state;

            state = pa_stream_get_state(data->stream);
            if(!PA_STREAM_IS_GOOD(state))
            {
                aluHandleDisconnect(device);
                break;
            }
            if(pa_stream_peek(data->stream, &data->cap_store, &data->cap_len) < 0)
            {
                ERR("pa_stream_peek() failed: %s\n",
                    pa_strerror(pa_context_errno(data->context)));
                aluHandleDisconnect(device);
                break;
            }
            data->cap_remain = data->cap_len;
        }
        if(rem > data->cap_remain)
            rem = data->cap_remain;

        memcpy(buffer, data->cap_store, rem);

        buffer = (ALbyte*)buffer + rem;
        todo -= rem;

        data->cap_store = (ALbyte*)data->cap_store + rem;
        data->cap_remain -= rem;
        if(data->cap_remain == 0)
        {
            pa_stream_drop(data->stream);
            data->cap_len = 0;
        }
    }
    if(todo > 0)
        memset(buffer, ((device->FmtType==DevFmtUByte) ? 0x80 : 0), todo);

    return ALC_NO_ERROR;
}

static ALCuint pulse_available_samples(ALCdevice *device)
{
    pulse_data *data = device->ExtraData;
    size_t readable = data->cap_remain;

    if(device->Connected)
    {
        ssize_t got = pa_stream_readable_size(data->stream);
        if(got < 0)
        {
            ERR("pa_stream_readable_size() failed: %s\n", pa_strerror(got));
            aluHandleDisconnect(device);
        }
        else if((size_t)got > data->cap_len)
            readable += got - data->cap_len;
    }

    if(data->last_readable < readable)
        data->last_readable = readable;
    return data->last_readable / pa_frame_size(&data->spec);
}


static void pulse_lock(ALCdevice *device)
{
    pulse_data *data = device->ExtraData;
    pa_threaded_mainloop_lock(data->loop);
}

static void pulse_unlock(ALCdevice *device)
{
    pulse_data *data = device->ExtraData;
    pa_threaded_mainloop_unlock(data->loop);
}


static ALint64 pulse_get_latency(ALCdevice *device)
{
    pulse_data *data = device->ExtraData;
    pa_usec_t latency = 0;
    int neg;

    if(pa_stream_get_latency(data->stream, &latency, &neg) == 0)
    {
        if(neg)
            latency = 0;
        return (ALint64)minu64(latency, MAKEU64(0x7fffffff,0xffffffff)/1000) * 1000;
    }
    ERR("Failed to get stream latency!\n");
    return 0;
}


static const BackendFuncs pulse_funcs = {
    pulse_open_playback,
    pulse_close_playback,
    pulse_reset_playback,
    pulse_start_playback,
    pulse_stop_playback,
    pulse_open_capture,
    pulse_close_capture,
    pulse_start_capture,
    pulse_stop_capture,
    pulse_capture_samples,
    pulse_available_samples,
    pulse_lock,
    pulse_unlock,
    pulse_get_latency
};

ALCboolean alc_pulse_init(BackendFuncs *func_list)
{
    ALCboolean ret = ALC_FALSE;

    if(pulse_load())
    {
        pa_threaded_mainloop *loop;

        pulse_ctx_flags = 0;
        if(!GetConfigValueBool("pulse", "spawn-server", 1))
            pulse_ctx_flags |= PA_CONTEXT_NOAUTOSPAWN;

        if((loop=pa_threaded_mainloop_new()) &&
           pa_threaded_mainloop_start(loop) >= 0)
        {
            pa_context *context;

            pa_threaded_mainloop_lock(loop);
            context = connect_context(loop, AL_TRUE);
            if(context)
            {
                *func_list = pulse_funcs;
                ret = ALC_TRUE;

                /* Some libraries (Phonon, Qt) set some pulseaudio properties
                 * through environment variables, which causes all streams in
                 * the process to inherit them. This attempts to filter those
                 * properties out by setting them to 0-length data. */
                prop_filter = pa_proplist_new();
                pa_proplist_set(prop_filter, PA_PROP_MEDIA_ROLE, NULL, 0);
                pa_proplist_set(prop_filter, "phonon.streamid", NULL, 0);

                pa_context_disconnect(context);
                pa_context_unref(context);
            }
            pa_threaded_mainloop_unlock(loop);
            pa_threaded_mainloop_stop(loop);
        }
        if(loop)
            pa_threaded_mainloop_free(loop);
    }

    return ret;
}

void alc_pulse_deinit(void)
{
    ALuint i;

    for(i = 0;i < numDevNames;++i)
    {
        free(allDevNameMap[i].name);
        free(allDevNameMap[i].device_name);
    }
    free(allDevNameMap);
    allDevNameMap = NULL;
    numDevNames = 0;

    for(i = 0;i < numCaptureDevNames;++i)
    {
        free(allCaptureDevNameMap[i].name);
        free(allCaptureDevNameMap[i].device_name);
    }
    free(allCaptureDevNameMap);
    allCaptureDevNameMap = NULL;
    numCaptureDevNames = 0;

    if(prop_filter)
        pa_proplist_free(prop_filter);
    prop_filter = NULL;

    /* PulseAudio doesn't like being CloseLib'd sometimes */
}

void alc_pulse_probe(enum DevProbe type)
{
    ALuint i;

    switch(type)
    {
        case ALL_DEVICE_PROBE:
            for(i = 0;i < numDevNames;++i)
            {
                free(allDevNameMap[i].name);
                free(allDevNameMap[i].device_name);
            }
            free(allDevNameMap);
            allDevNameMap = NULL;
            numDevNames = 0;

            probe_devices(AL_FALSE);

            for(i = 0;i < numDevNames;i++)
                AppendAllDevicesList(allDevNameMap[i].name);
            break;

        case CAPTURE_DEVICE_PROBE:
            for(i = 0;i < numCaptureDevNames;++i)
            {
                free(allCaptureDevNameMap[i].name);
                free(allCaptureDevNameMap[i].device_name);
            }
            free(allCaptureDevNameMap);
            allCaptureDevNameMap = NULL;
            numCaptureDevNames = 0;

            probe_devices(AL_TRUE);

            for(i = 0;i < numCaptureDevNames;i++)
                AppendCaptureDeviceList(allCaptureDevNameMap[i].name);
            break;
    }
}

#else

#warning "Unsupported API version, backend will be unavailable!"

ALCboolean alc_pulse_init(BackendFuncs *func_list)
{ return ALC_FALSE; (void)func_list; }

void alc_pulse_deinit(void)
{ }

void alc_pulse_probe(enum DevProbe type)
{ (void)type; }

#endif
