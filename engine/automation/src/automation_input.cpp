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

#include "automation_private.h"
#include <hid.h>

#include <dmsdk/dlib/utf8.h>

#include <stdint.h>
#include <string.h>

namespace dmAutomation
{
    static const float INPUT_VISUALIZATION_SECONDS = 1.0f;
    static const float DEFAULT_CONTROLLER_LEASE_SECONDS = 5.0f;
    static const float INPUT_COMPLETION_GRACE_SECONDS = DEFAULT_CONTROLLER_LEASE_SECONDS;

    static bool ParseSpecialKey(const char* keys, uint32_t index, char* name, uint32_t name_size, uint32_t* next_index);

    static const char* NormalizedId(const char* value, const char* fallback)
    {
        return IsEmpty(value) ? fallback : value;
    }

    const char* InputStateName(InputState state)
    {
        switch (state)
        {
            case INPUT_STATE_ACCEPTED: return "accepted";
            case INPUT_STATE_STARTED: return "started";
            case INPUT_STATE_RELEASED: return "released";
            case INPUT_STATE_CANCELLED: return "cancelled";
            case INPUT_STATE_FAILED: return "failed";
        }
        return "failed";
    }

    const char* InputDeviceName(InputDevice device)
    {
        switch (device)
        {
            case INPUT_DEVICE_AUTO: return "auto";
            case INPUT_DEVICE_MOUSE: return "mouse";
            case INPUT_DEVICE_TOUCH: return "touch";
        }
        return "auto";
    }

    bool ParseInputDevice(const char* value, InputDevice* device)
    {
        if (IsEmpty(value) || StringsEqual(value, "auto"))
        {
            *device = INPUT_DEVICE_AUTO;
            return true;
        }
        if (StringsEqual(value, "mouse"))
        {
            *device = INPUT_DEVICE_MOUSE;
            return true;
        }
        if (StringsEqual(value, "touch"))
        {
            *device = INPUT_DEVICE_TOUCH;
            return true;
        }
        return false;
    }

    bool ParseInputEasing(const char* value, InputEasing* easing)
    {
        if (IsEmpty(value) || StringsEqual(value, "linear"))
        {
            *easing = INPUT_EASING_LINEAR;
            return true;
        }
        if (StringsEqual(value, "ease_in"))
        {
            *easing = INPUT_EASING_EASE_IN;
            return true;
        }
        if (StringsEqual(value, "ease_out"))
        {
            *easing = INPUT_EASING_EASE_OUT;
            return true;
        }
        if (StringsEqual(value, "ease_in_out"))
        {
            *easing = INPUT_EASING_EASE_IN_OUT;
            return true;
        }
        return false;
    }

    bool IsInputDeviceSupported(InputDevice device)
    {
        if (!g_AutomationBridge.m_HidContext) return false;
        if (device == INPUT_DEVICE_AUTO) return IsInputDeviceSupported(ResolveInputDevice(device));
        if (device == INPUT_DEVICE_MOUSE) return dmHID::IsMouseConnected(dmHID::GetMouse(g_AutomationBridge.m_HidContext, 0));
        return device == INPUT_DEVICE_TOUCH && dmHID::IsTouchInputEnabled(g_AutomationBridge.m_HidContext);
    }

    InputDevice ResolveInputDevice(InputDevice device)
    {
        if (device != INPUT_DEVICE_AUTO)
        {
            return device;
        }
#if defined(DM_PLATFORM_IOS) || defined(ANDROID) || defined(DM_PLATFORM_SWITCH)
        return INPUT_DEVICE_TOUCH;
#else
        return INPUT_DEVICE_MOUSE;
#endif
    }

    void FreeInputReceipt(InputReceipt* receipt)
    {
        if (!receipt)
        {
            return;
        }
        FreeString(&receipt->m_Kind);
        FreeString(&receipt->m_Reason);
        FreeString(&receipt->m_ClientId);
        FreeString(&receipt->m_SessionId);
        FreeString(&receipt->m_RequestId);
        memset(receipt, 0, sizeof(*receipt));
    }

    void FreeInputEvent(InputEvent* event)
    {
        if (!event)
        {
            return;
        }
        FreeString(&event->m_Target);
        FreeString(&event->m_EndTarget);
        FreeInputReceipt(&event->m_Receipt);
        ArrayFree(&event->m_Points);
        FreeString(&event->m_Keys);
        memset(event, 0, sizeof(*event));
    }

    static bool ReceiptSetReason(InputReceipt* receipt, const char* reason)
    {
        return SetString(&receipt->m_Reason, IsEmpty(reason) ? "" : reason);
    }

    static bool InitReceipt(InputReceipt* receipt, const char* kind, const char* client_id, const char* session_id,
                            const char* request_id, uint64_t scene_sequence, InputDevice device, uint32_t pointer_id)
    {
        memset(receipt, 0, sizeof(*receipt));
        receipt->m_Id = g_AutomationBridge.m_NextInputId++;
        receipt->m_State = INPUT_STATE_ACCEPTED;
        receipt->m_AcceptedFrame = g_AutomationBridge.m_Frame;
        receipt->m_AcceptedTime = dmTime::GetMonotonicTime();
        receipt->m_SceneSequence = scene_sequence;
        receipt->m_Device = ResolveInputDevice(device);
        receipt->m_PointerId = pointer_id;
        return SetString(&receipt->m_Kind, kind) &&
               SetString(&receipt->m_ClientId, NormalizedId(client_id, "anonymous")) &&
               SetString(&receipt->m_SessionId, NormalizedId(session_id, "default")) &&
               SetString(&receipt->m_RequestId, NormalizedId(request_id, ""));
    }

    static void RefreshKeyCompletionDeadline(InputEvent* event, uint64_t now)
    {
        if (!event || event->m_Type != INPUT_EVENT_KEYS)
        {
            return;
        }
        // The finite operation, not the caller's heartbeat lease, owns the controller
        // through release. Refreshing this deadline only when the event makes structural
        // progress protects long text/key sequences without keeping a stalled event alive.
        // The per-token hold covers work until the next structural progress point; grace
        // covers frame overhead and lets the release update run at the exact 60s boundary.
        float protected_seconds = event->m_HoldAfter + INPUT_COMPLETION_GRACE_SECONDS;
        event->m_CompletionDeadline = now + (uint64_t)(protected_seconds * 1000000.0f);
    }

    static bool CanQueueInputEvent()
    {
        return g_AutomationBridge.m_InputEvents.Size() < MAX_INPUT_EVENTS;
    }

    static void MoveReceiptToHistory(InputEvent* event)
    {
        if (g_AutomationBridge.m_InputHistory.Size() >= MAX_INPUT_HISTORY)
        {
            FreeInputReceipt(&g_AutomationBridge.m_InputHistory.Begin()[0]);
            ArrayErase(&g_AutomationBridge.m_InputHistory, 0);
        }
        if (ArrayPush(&g_AutomationBridge.m_InputHistory, &event->m_Receipt))
        {
            memset(&event->m_Receipt, 0, sizeof(event->m_Receipt));
        }
    }

    static void RemoveFinishedEvent(uint32_t index)
    {
        InputEvent* event = &g_AutomationBridge.m_InputEvents.Begin()[index];
        dmHID::HKeyboard keyboard = dmHID::GetKeyboard(g_AutomationBridge.m_HidContext, 0);
        for (uint32_t i = 0; i < event->m_ModifierCount; ++i) dmHID::SetKey(keyboard, event->m_Modifiers[i], false);
        MoveReceiptToHistory(event);
        FreeInputEvent(event);
        ArrayErase(&g_AutomationBridge.m_InputEvents, index);
        if (index == 0 && g_AutomationBridge.m_InputEvents.Size() > 0)
        {
            RefreshKeyCompletionDeadline(&g_AutomationBridge.m_InputEvents.Begin()[0], dmTime::GetMonotonicTime());
        }
    }

    bool IsInputController(const char* client_id, const char* session_id)
    {
        return StringsEqual(g_AutomationBridge.m_ControllerClientId, NormalizedId(client_id, "anonymous")) &&
               StringsEqual(g_AutomationBridge.m_ControllerSessionId, NormalizedId(session_id, "default"));
    }

    static uint64_t EffectiveControllerLeaseDeadline()
    {
        uint64_t deadline = g_AutomationBridge.m_ControllerLeaseDeadline;
        if (g_AutomationBridge.m_InputEvents.Size() == 0)
        {
            return deadline;
        }
        const InputEvent* event = &g_AutomationBridge.m_InputEvents.Begin()[0];
        if (event->m_Type == INPUT_EVENT_KEYS &&
            IsInputController(event->m_Receipt.m_ClientId, event->m_Receipt.m_SessionId) &&
            event->m_CompletionDeadline > deadline)
        {
            deadline = event->m_CompletionDeadline;
        }
        return deadline;
    }

    bool AcquireInputController(const char* client_id, const char* session_id, float lease, const char** error)
    {
        uint64_t now = dmTime::GetMonotonicTime();
        const char* normalized_client = NormalizedId(client_id, "anonymous");
        const char* normalized_session = NormalizedId(session_id, "default");
        if (!IsEmpty(g_AutomationBridge.m_ControllerClientId) && EffectiveControllerLeaseDeadline() <= now)
        {
            FlushInput(g_AutomationBridge.m_ControllerClientId, g_AutomationBridge.m_ControllerSessionId, true, "controller_lease_expired");
            FreeString(&g_AutomationBridge.m_ControllerClientId);
            FreeString(&g_AutomationBridge.m_ControllerSessionId);
        }
        if (!IsEmpty(g_AutomationBridge.m_ControllerClientId) && !IsInputController(normalized_client, normalized_session))
        {
            *error = "another client/session holds the input controller lease";
            return false;
        }
        if (!SetString(&g_AutomationBridge.m_ControllerClientId, normalized_client) ||
            !SetString(&g_AutomationBridge.m_ControllerSessionId, normalized_session))
        {
            *error = "unable to store input controller identity";
            return false;
        }
        float lease_seconds = lease > 0.0f ? lease : DEFAULT_CONTROLLER_LEASE_SECONDS;
        lease_seconds = ClampFloat(lease_seconds, 0.1f, MAX_INPUT_DURATION);
        g_AutomationBridge.m_ControllerLeaseDeadline = now + (uint64_t)(lease_seconds * 1000000.0f);
        return true;
    }

    void ReleaseInputController(const char* client_id, const char* session_id)
    {
        if (!IsInputController(client_id, session_id))
        {
            return;
        }
        FreeString(&g_AutomationBridge.m_ControllerClientId);
        FreeString(&g_AutomationBridge.m_ControllerSessionId);
        g_AutomationBridge.m_ControllerLeaseDeadline = 0;
    }

    static float RequestedPathDuration(const dmArray<InputPoint>* points, InputPathMode path_mode)
    {
        float total = 0.0f;
        if (path_mode == INPUT_PATH_SAMPLED)
        {
            for (uint32_t i = 1; i < points->Size(); ++i)
            {
                total += points->Begin()[i].m_Duration;
            }
        }
        else if (points->Size() > 1)
        {
            total = points->Begin()[points->Size() - 1].m_Duration;
        }
        return total;
    }

    // Copy validated chord modifiers into a freshly-initialized event (see the
    // InputEvent field comment; parse/validation happens in ParseModifierList).
    static void SetEventModifiers(InputEvent* event, const dmHID::Key* modifiers, uint32_t modifier_count)
    {
        uint32_t count = modifier_count > MAX_INPUT_MODIFIERS ? MAX_INPUT_MODIFIERS : modifier_count;
        for (uint32_t i = 0; i < count; ++i)
        {
            event->m_Modifiers[i] = modifiers[i];
        }
        event->m_ModifierCount = (uint8_t)count;
    }

    bool AddMouseInput(const dmArray<InputPoint>* points, InputPathMode path_mode, float hold_before, float hold_after,
                       InputDevice device, uint32_t pointer_id, bool visualize, const char* kind,
                       const dmHID::Key* modifiers, uint32_t modifier_count,
                       const char* client_id, const char* session_id, const char* request_id,
                       uint64_t scene_sequence, float lease, bool pointer_open, InputReceipt** receipt)
    {
        if (!points || points->Size() == 0 || points->Size() > MAX_INPUT_PATH_POINTS || !CanQueueInputEvent())
        {
            return false;
        }
        InputDevice resolved = ResolveInputDevice(device);
        if (!IsInputDeviceSupported(resolved))
        {
            return false;
        }

        InputEvent event;
        memset(&event, 0, sizeof(event));
        event.m_Type = pointer_open ? INPUT_EVENT_POINTER : INPUT_EVENT_MOUSE;
        event.m_PathMode = path_mode;
        event.m_HoldBefore = MaxFloat(0.0f, hold_before);
        event.m_HoldAfter = MaxFloat(0.0f, hold_after);
        event.m_MouseButton = dmHID::MOUSE_BUTTON_LEFT;
        event.m_Visualize = visualize;
        event.m_ActiveKey = dmHID::MAX_KEY_COUNT;
        SetEventModifiers(&event, modifiers, modifier_count);
        event.m_PointerOpen = pointer_open;
        event.m_LeaseDeadline = pointer_open ? dmTime::GetMonotonicTime() + (uint64_t)(ClampFloat(lease, 0.1f, MAX_INPUT_DURATION) * 1000000.0f) : 0;
        if (!InitReceipt(&event.m_Receipt, kind, client_id, session_id, request_id, scene_sequence, resolved, pointer_id) ||
            !ArraySetCount(&event.m_Points, points->Size()))
        {
            FreeInputEvent(&event);
            return false;
        }
        memcpy(event.m_Points.Begin(), points->Begin(), sizeof(InputPoint) * points->Size());
        event.m_Receipt.m_RequestedDuration = event.m_HoldBefore + RequestedPathDuration(points, path_mode) + event.m_HoldAfter;
        event.m_Receipt.m_ModifierCount = event.m_ModifierCount; // echoed in receipts so callers can detect a bridge without modifier support
        if (!ArrayPush(&g_AutomationBridge.m_InputEvents, &event))
        {
            FreeInputEvent(&event);
            return false;
        }
        memset(&event, 0, sizeof(event)); // Queue now owns the event fields.
        *receipt = &g_AutomationBridge.m_InputEvents.Begin()[g_AutomationBridge.m_InputEvents.Size() - 1].m_Receipt;
        return true;
    }

    bool AddKeyInput(const char* keys, bool parse_special_keys, float key_hold, float requested_duration,
                     const dmHID::Key* modifiers, uint32_t modifier_count,
                     const char* client_id, const char* session_id, const char* request_id,
                     uint64_t scene_sequence, InputReceipt** receipt)
    {
        if (IsEmpty(keys) || !CanQueueInputEvent())
        {
            return false;
        }
        InputEvent event;
        memset(&event, 0, sizeof(event));
        event.m_Type = INPUT_EVENT_KEYS;
        event.m_Keys = DuplicateString(keys);
        event.m_ActiveKey = dmHID::MAX_KEY_COUNT;
        SetEventModifiers(&event, modifiers, modifier_count);
        event.m_ParseSpecialKeys = parse_special_keys;
        event.m_HoldAfter = key_hold; // per special-key hold in seconds; 0 = single-update tap
        if (!event.m_Keys || !InitReceipt(&event.m_Receipt, "key", client_id, session_id, request_id,
                                          scene_sequence, INPUT_DEVICE_AUTO, 0))
        {
            FreeInputEvent(&event);
            return false;
        }
        event.m_Receipt.m_RequestedDuration = requested_duration;
        event.m_Receipt.m_ModifierCount = event.m_ModifierCount; // echoed so callers can detect a bridge without modifier support
        RefreshKeyCompletionDeadline(&event, dmTime::GetMonotonicTime());
        if (!ArrayPush(&g_AutomationBridge.m_InputEvents, &event))
        {
            FreeInputEvent(&event);
            return false;
        }
        memset(&event, 0, sizeof(event)); // Queue now owns the event fields.
        *receipt = &g_AutomationBridge.m_InputEvents.Begin()[g_AutomationBridge.m_InputEvents.Size() - 1].m_Receipt;
        return true;
    }

    static InputEvent* FindQueuedEvent(uint64_t input_id, uint32_t* index)
    {
        for (uint32_t i = 0; i < g_AutomationBridge.m_InputEvents.Size(); ++i)
        {
            if (g_AutomationBridge.m_InputEvents.Begin()[i].m_Receipt.m_Id == input_id)
            {
                if (index) *index = i;
                return &g_AutomationBridge.m_InputEvents.Begin()[i];
            }
        }
        return 0;
    }

    const InputReceipt* FindInputReceipt(uint64_t input_id)
    {
        InputEvent* event = FindQueuedEvent(input_id, 0);
        if (event)
        {
            return &event->m_Receipt;
        }
        for (uint32_t i = 0; i < g_AutomationBridge.m_InputHistory.Size(); ++i)
        {
            if (g_AutomationBridge.m_InputHistory.Begin()[i].m_Id == input_id)
            {
                return &g_AutomationBridge.m_InputHistory.Begin()[i];
            }
        }
        return 0;
    }

    static void RefreshPointerLease(InputEvent* event, float lease)
    {
        event->m_LeaseDeadline = dmTime::GetMonotonicTime() + (uint64_t)(ClampFloat(lease, 0.1f, MAX_INPUT_DURATION) * 1000000.0f);
    }

    bool AppendPointerMove(uint64_t input_id, const InputPoint* point, float lease, const char** error)
    {
        InputEvent* event = FindQueuedEvent(input_id, 0);
        if (!event || !event->m_PointerOpen)
        {
            *error = "pointer session was not found or is already closed";
            return false;
        }
        if (!point || event->m_Points.Size() >= MAX_INPUT_PATH_POINTS)
        {
            *error = "pointer path has reached the maximum point count";
            return false;
        }
        uint32_t previous_count = event->m_Points.Size();
        if (!ArrayPush(&event->m_Points, point))
        {
            *error = "unable to append pointer path point";
            return false;
        }
        event->m_Receipt.m_RequestedDuration += point->m_Duration;
        if (event->m_Phase == 4)
        {
            event->m_Segment = previous_count - 1;
            event->m_Elapsed = 0.0f;
            event->m_Phase = 2;
        }
        RefreshPointerLease(event, lease);
        return true;
    }

    bool AppendPointerHold(uint64_t input_id, float duration, float lease, const char** error)
    {
        InputEvent* event = FindQueuedEvent(input_id, 0);
        if (!event || event->m_Points.Size() == 0)
        {
            *error = "pointer session was not found or is already closed";
            return false;
        }
        InputPoint point = event->m_Points.Begin()[event->m_Points.Size() - 1];
        point.m_Duration = duration;
        point.m_Easing = INPUT_EASING_LINEAR;
        return AppendPointerMove(input_id, &point, lease, error);
    }

    bool ReleasePointer(uint64_t input_id, const char** error)
    {
        InputEvent* event = FindQueuedEvent(input_id, 0);
        if (!event || !event->m_PointerOpen)
        {
            *error = "pointer session was not found or is already closed";
            return false;
        }
        event->m_PointerOpen = false;
        event->m_ReleaseRequested = true;
        return true;
    }

    static void AppendUInt64(StringBuffer* out, uint64_t value)
    {
        char buffer[32];
        dmSnPrintf(buffer, sizeof(buffer), "%llu", (unsigned long long)value);
        StringBufferAppend(out, buffer);
    }

    static void AppendOptionalFrame(StringBuffer* out, uint64_t value)
    {
        if (value == 0)
        {
            StringBufferAppend(out, "null");
        }
        else
        {
            AppendUInt64(out, value);
        }
    }

    void AppendInputReceiptJson(StringBuffer* out, const InputReceipt* receipt, uint32_t queue_position)
    {
        StringBufferAppend(out, "{\"input_id\":");
        AppendUInt64(out, receipt->m_Id);
        StringBufferAppend(out, ",\"kind\":");
        AppendJsonString(out, receipt->m_Kind);
        StringBufferAppend(out, ",\"state\":");
        AppendJsonString(out, InputStateName(receipt->m_State));
        StringBufferAppend(out, ",\"queue_position\":");
        AppendNumber(out, (double)queue_position);
        StringBufferAppend(out, ",\"accepted_frame\":");
        AppendOptionalFrame(out, receipt->m_AcceptedFrame);
        StringBufferAppend(out, ",\"start_frame\":");
        AppendOptionalFrame(out, receipt->m_StartFrame);
        StringBufferAppend(out, ",\"release_frame\":");
        AppendOptionalFrame(out, receipt->m_ReleaseFrame);
        StringBufferAppend(out, ",\"delivered_frame\":");
        AppendOptionalFrame(out, receipt->m_DeliveredFrame);
        StringBufferAppend(out, ",\"accepted_time_us\":");
        AppendUInt64(out, receipt->m_AcceptedTime);
        StringBufferAppend(out, ",\"start_time_us\":");
        AppendOptionalFrame(out, receipt->m_StartTime);
        StringBufferAppend(out, ",\"release_time_us\":");
        AppendOptionalFrame(out, receipt->m_ReleaseTime);
        StringBufferAppend(out, ",\"requested_duration\":");
        AppendNumber(out, receipt->m_RequestedDuration);
        StringBufferAppend(out, ",\"actual_duration\":");
        AppendNumber(out, receipt->m_StartTime && receipt->m_ReleaseTime ? (double)(receipt->m_ReleaseTime - receipt->m_StartTime) / 1000000.0 : 0.0);
        StringBufferAppend(out, ",\"reason\":");
        if (IsEmpty(receipt->m_Reason)) StringBufferAppend(out, "null"); else AppendJsonString(out, receipt->m_Reason);
        StringBufferAppend(out, ",\"client_id\":");
        AppendJsonString(out, receipt->m_ClientId);
        StringBufferAppend(out, ",\"session_id\":");
        AppendJsonString(out, receipt->m_SessionId);
        StringBufferAppend(out, ",\"request_id\":");
        AppendJsonString(out, receipt->m_RequestId);
        StringBufferAppend(out, ",\"engine_instance_id\":");
        AppendJsonString(out, g_AutomationBridge.m_EngineInstanceId);
        StringBufferAppend(out, ",\"scene_sequence\":");
        AppendUInt64(out, receipt->m_SceneSequence);
        StringBufferAppend(out, ",\"engine_frame\":");
        AppendUInt64(out, g_AutomationBridge.m_Frame);
        StringBufferAppend(out, ",\"device\":");
        AppendJsonString(out, InputDeviceName(receipt->m_Device));
        StringBufferAppend(out, ",\"pointer_id\":");
        AppendNumber(out, (double)receipt->m_PointerId);
        StringBufferAppend(out, ",\"modifier_count\":");
        AppendNumber(out, (double)receipt->m_ModifierCount);
        StringBufferAppendChar(out, '}');
    }

    bool GetNodeCenter(const char* id, float* x, float* y, const char** error)
    {
        const Node* node = FindNodeById(id);
        if (!node)
        {
            *error = "element id was not found";
            return false;
        }
        if (!node->m_Bounds.m_Valid)
        {
            *error = "element has no screen bounds";
            return false;
        }
        *x = node->m_Bounds.m_CX;
        *y = node->m_Bounds.m_CY;
        return true;
    }

    dmHID::Key KeyFromName(const char* name)
    {
        struct KeyMap { const char* m_Name; dmHID::Key m_Key; };
        static const KeyMap keys[] = {
            {"KEY_SPACE", dmHID::KEY_SPACE}, {"KEY_ESC", dmHID::KEY_ESC}, {"KEY_ESCAPE", dmHID::KEY_ESC},
            {"KEY_UP", dmHID::KEY_UP}, {"KEY_DOWN", dmHID::KEY_DOWN}, {"KEY_LEFT", dmHID::KEY_LEFT},
            {"KEY_RIGHT", dmHID::KEY_RIGHT}, {"KEY_TAB", dmHID::KEY_TAB}, {"KEY_ENTER", dmHID::KEY_ENTER},
            {"KEY_BACKSPACE", dmHID::KEY_BACKSPACE}, {"KEY_INSERT", dmHID::KEY_INSERT}, {"KEY_DEL", dmHID::KEY_DEL},
            {"KEY_DELETE", dmHID::KEY_DEL}, {"KEY_PAGEUP", dmHID::KEY_PAGEUP}, {"KEY_PAGEDOWN", dmHID::KEY_PAGEDOWN},
            {"KEY_HOME", dmHID::KEY_HOME}, {"KEY_END", dmHID::KEY_END}, {"KEY_LSHIFT", dmHID::KEY_LSHIFT},
            {"KEY_RSHIFT", dmHID::KEY_RSHIFT}, {"KEY_LCTRL", dmHID::KEY_LCTRL}, {"KEY_RCTRL", dmHID::KEY_RCTRL},
            {"KEY_LALT", dmHID::KEY_LALT}, {"KEY_RALT", dmHID::KEY_RALT},
            {"KEY_F1", dmHID::KEY_F1}, {"KEY_F2", dmHID::KEY_F2}, {"KEY_F3", dmHID::KEY_F3},
            {"KEY_F4", dmHID::KEY_F4}, {"KEY_F5", dmHID::KEY_F5}, {"KEY_F6", dmHID::KEY_F6},
            {"KEY_F7", dmHID::KEY_F7}, {"KEY_F8", dmHID::KEY_F8}, {"KEY_F9", dmHID::KEY_F9},
            {"KEY_F10", dmHID::KEY_F10}, {"KEY_F11", dmHID::KEY_F11}, {"KEY_F12", dmHID::KEY_F12},
            // Punctuation and symbol keys -- every remaining named key in dmHID's Key enum,
            // so any key_trigger a game.input_binding can express is reachable by name.
            {"KEY_EXCLAIM", dmHID::KEY_EXCLAIM}, {"KEY_QUOTEDBL", dmHID::KEY_QUOTEDBL},
            {"KEY_HASH", dmHID::KEY_HASH}, {"KEY_DOLLAR", dmHID::KEY_DOLLAR},
            {"KEY_AMPERSAND", dmHID::KEY_AMPERSAND}, {"KEY_QUOTE", dmHID::KEY_QUOTE},
            {"KEY_LPAREN", dmHID::KEY_LPAREN}, {"KEY_RPAREN", dmHID::KEY_RPAREN},
            {"KEY_ASTERISK", dmHID::KEY_ASTERISK}, {"KEY_PLUS", dmHID::KEY_PLUS},
            {"KEY_COMMA", dmHID::KEY_COMMA}, {"KEY_MINUS", dmHID::KEY_MINUS},
            {"KEY_PERIOD", dmHID::KEY_PERIOD}, {"KEY_SLASH", dmHID::KEY_SLASH},
            {"KEY_COLON", dmHID::KEY_COLON}, {"KEY_SEMICOLON", dmHID::KEY_SEMICOLON},
            {"KEY_LESS", dmHID::KEY_LESS}, {"KEY_EQUALS", dmHID::KEY_EQUALS},
            {"KEY_GREATER", dmHID::KEY_GREATER}, {"KEY_QUESTION", dmHID::KEY_QUESTION},
            {"KEY_AT", dmHID::KEY_AT}, {"KEY_LBRACKET", dmHID::KEY_LBRACKET},
            {"KEY_BACKSLASH", dmHID::KEY_BACKSLASH}, {"KEY_RBRACKET", dmHID::KEY_RBRACKET},
            {"KEY_CARET", dmHID::KEY_CARET}, {"KEY_UNDERSCORE", dmHID::KEY_UNDERSCORE},
            {"KEY_BACKQUOTE", dmHID::KEY_BACKQUOTE}, {"KEY_LBRACE", dmHID::KEY_LBRACE},
            {"KEY_PIPE", dmHID::KEY_PIPE}, {"KEY_RBRACE", dmHID::KEY_RBRACE},
            {"KEY_TILDE", dmHID::KEY_TILDE},
            // Keypad
            {"KEY_KP_0", dmHID::KEY_KP_0}, {"KEY_KP_1", dmHID::KEY_KP_1},
            {"KEY_KP_2", dmHID::KEY_KP_2}, {"KEY_KP_3", dmHID::KEY_KP_3},
            {"KEY_KP_4", dmHID::KEY_KP_4}, {"KEY_KP_5", dmHID::KEY_KP_5},
            {"KEY_KP_6", dmHID::KEY_KP_6}, {"KEY_KP_7", dmHID::KEY_KP_7},
            {"KEY_KP_8", dmHID::KEY_KP_8}, {"KEY_KP_9", dmHID::KEY_KP_9},
            {"KEY_KP_DIVIDE", dmHID::KEY_KP_DIVIDE}, {"KEY_KP_MULTIPLY", dmHID::KEY_KP_MULTIPLY},
            {"KEY_KP_SUBTRACT", dmHID::KEY_KP_SUBTRACT}, {"KEY_KP_ADD", dmHID::KEY_KP_ADD},
            {"KEY_KP_DECIMAL", dmHID::KEY_KP_DECIMAL}, {"KEY_KP_EQUAL", dmHID::KEY_KP_EQUAL},
            {"KEY_KP_ENTER", dmHID::KEY_KP_ENTER}, {"KEY_KP_NUM_LOCK", dmHID::KEY_KP_NUM_LOCK},
            // Lock/system keys
            {"KEY_CAPS_LOCK", dmHID::KEY_CAPS_LOCK}, {"KEY_SCROLL_LOCK", dmHID::KEY_SCROLL_LOCK},
            {"KEY_PAUSE", dmHID::KEY_PAUSE}, {"KEY_LSUPER", dmHID::KEY_LSUPER},
            {"KEY_RSUPER", dmHID::KEY_RSUPER}, {"KEY_MENU", dmHID::KEY_MENU},
            {"KEY_BACK", dmHID::KEY_BACK}
        };
        for (uint32_t i = 0; i < DM_ARRAY_SIZE(keys); ++i)
        {
            if (StringsEqual(name, keys[i].m_Name)) return keys[i].m_Key;
        }
        if (strlen(name) == 5 && StartsWith(name, "KEY_") && name[4] >= 'A' && name[4] <= 'Z')
            return (dmHID::Key)(dmHID::KEY_A + (name[4] - 'A'));
        if (strlen(name) == 5 && StartsWith(name, "KEY_") && name[4] >= '0' && name[4] <= '9')
            return (dmHID::Key)(dmHID::KEY_0 + (name[4] - '0'));
        return dmHID::MAX_KEY_COUNT;
    }

    static bool ParseSpecialKey(const char* keys, uint32_t index, char* name, uint32_t name_size, uint32_t* next_index)
    {
        if (index >= strlen(keys) || keys[index] != '{') return false;
        const char* start = keys + index + 1;
        const char* end = strchr(start, '}');
        if (!end) return false;
        uint32_t length = (uint32_t)(end - start);
        if (length == 0 || length >= name_size) return false;
        memcpy(name, start, length);
        name[length] = 0;
        *next_index = (uint32_t)(end - keys) + 1;
        return true;
    }

    bool ValidateSpecialKeyInput(const char* keys, const char** error, uint32_t* out_special_key_count)
    {
        if (out_special_key_count) *out_special_key_count = 0;
        if (IsEmpty(keys))
        {
            *error = "special key input is empty";
            return false;
        }
        uint32_t index = 0;
        uint32_t length = (uint32_t)strlen(keys);
        while (index < length)
        {
            if (keys[index] != '{')
            {
                ++index;
                continue;
            }
            char name[64];
            uint32_t next_index = index;
            if (!ParseSpecialKey(keys, index, name, sizeof(name), &next_index))
            {
                *error = "special keys must use balanced braces, for example {KEY_ENTER}";
                return false;
            }
            if (KeyFromName(name) == dmHID::MAX_KEY_COUNT)
            {
                *error = "unsupported special key; accepted names include KEY_A-KEY_Z, KEY_0-KEY_9, KEY_F1-KEY_F12, KEY_SPACE, KEY_ESCAPE, arrows, modifiers, navigation keys, punctuation/symbol keys (e.g. KEY_EQUALS, KEY_MINUS, KEY_COMMA), keypad keys (KEY_KP_0-KEY_KP_9 etc.), and lock/system keys";
                return false;
            }
            if (out_special_key_count) ++(*out_special_key_count);
            index = next_index;
        }
        return true;
    }

    static float ApplyEasing(float t, InputEasing easing)
    {
        t = ClampFloat(t, 0.0f, 1.0f);
        if (easing == INPUT_EASING_EASE_IN) return t * t;
        if (easing == INPUT_EASING_EASE_OUT) return 1.0f - (1.0f - t) * (1.0f - t);
        if (easing == INPUT_EASING_EASE_IN_OUT) return t < 0.5f ? 2.0f * t * t : 1.0f - 2.0f * (1.0f - t) * (1.0f - t);
        return t;
    }

    static void SamplePath(const InputEvent* event, float t, float* x, float* y)
    {
        if (event->m_PathMode == INPUT_PATH_SAMPLED)
        {
            const InputPoint* a = &event->m_Points.Begin()[event->m_Segment];
            const InputPoint* b = &event->m_Points.Begin()[event->m_Segment + 1];
            *x = a->m_X + (b->m_X - a->m_X) * t;
            *y = a->m_Y + (b->m_Y - a->m_Y) * t;
            return;
        }
        const InputPoint* p = event->m_Points.Begin();
        float u = 1.0f - t;
        if (event->m_PathMode == INPUT_PATH_QUADRATIC)
        {
            *x = u * u * p[0].m_X + 2.0f * u * t * p[1].m_X + t * t * p[2].m_X;
            *y = u * u * p[0].m_Y + 2.0f * u * t * p[1].m_Y + t * t * p[2].m_Y;
            return;
        }
        *x = u * u * u * p[0].m_X + 3.0f * u * u * t * p[1].m_X + 3.0f * u * t * t * p[2].m_X + t * t * t * p[3].m_X;
        *y = u * u * u * p[0].m_Y + 3.0f * u * u * t * p[1].m_Y + 3.0f * u * t * t * p[2].m_Y + t * t * t * p[3].m_Y;
    }

    static void AddVisualizationPoint(float x, float y, bool reset)
    {
        InputVisualization* visualization = &g_AutomationBridge.m_InputVisualization;
        // InjectPointer passes integer framebuffer coordinates to HID. Store
        // those same coordinates so fractional request values cannot make the
        // debug overlay differ from the event the game actually receives.
        x = (float)(int32_t)x;
        y = (float)(int32_t)y;
        if (reset)
        {
            memset(visualization, 0, sizeof(*visualization));
            visualization->m_Active = true;
            visualization->m_Duration = INPUT_VISUALIZATION_SECONDS;
        }
        if (!visualization->m_Active) return;
        if (visualization->m_PointCount > 0)
        {
            uint32_t last = visualization->m_PointCount - 1;
            if (fabsf(visualization->m_X[last] - x) < 0.5f && fabsf(visualization->m_Y[last] - y) < 0.5f) return;
        }
        if (visualization->m_PointCount < MAX_INPUT_PATH_POINTS)
        {
            uint32_t i = visualization->m_PointCount++;
            visualization->m_X[i] = x;
            visualization->m_Y[i] = y;
            visualization->m_Drag = visualization->m_PointCount > 1;
            // Keep the complete trail visible for its configured duration after the
            // latest injected point, including for drags longer than that duration.
            visualization->m_Age = 0.0f;
        }
    }

    static bool InjectPointer(InputEvent* event, float x, float y, bool pressed, bool released, bool cancelled)
    {
        InputDevice device = event->m_Receipt.m_Device;
        if (device == INPUT_DEVICE_MOUSE)
        {
            dmHID::HMouse mouse = dmHID::GetMouse(g_AutomationBridge.m_HidContext, 0);
            if (mouse == dmHID::INVALID_MOUSE_HANDLE) return false;
            dmHID::SetMousePosition(mouse, (int32_t)x, (int32_t)y);
            dmHID::SetMouseButton(mouse, event->m_MouseButton, !(released || cancelled));
            return true;
        }
        if (device == INPUT_DEVICE_TOUCH)
        {
            dmHID::HTouchDevice touch = dmHID::GetTouchDevice(g_AutomationBridge.m_HidContext, 0);
            if (touch == dmHID::INVALID_TOUCH_DEVICE_HANDLE) return false;
            dmHID::Phase phase = pressed ? dmHID::PHASE_BEGAN : dmHID::PHASE_MOVED;
            if (released) phase = dmHID::PHASE_ENDED;
            if (cancelled) phase = dmHID::PHASE_CANCELLED;
            return dmHID::SetSyntheticTouch(g_AutomationBridge.m_HidContext, (int32_t)x, (int32_t)y, event->m_Receipt.m_PointerId, phase);
        }
        return false;
    }

    static void StartReceipt(InputReceipt* receipt)
    {
        receipt->m_State = INPUT_STATE_STARTED;
        receipt->m_StartFrame = g_AutomationBridge.m_Frame;
        receipt->m_StartTime = dmTime::GetMonotonicTime();
    }

    static void FinishReceipt(InputReceipt* receipt, InputState state, const char* reason)
    {
        receipt->m_State = state;
        receipt->m_ReleaseFrame = g_AutomationBridge.m_Frame;
        receipt->m_ReleaseTime = dmTime::GetMonotonicTime();
        ReceiptSetReason(receipt, reason);
    }

    static bool InjectPointerOrFail(InputEvent* event, float x, float y, bool pressed, bool released, bool cancelled)
    {
        if (InjectPointer(event, x, y, pressed, released, cancelled)) return true;
        FinishReceipt(&event->m_Receipt, INPUT_STATE_FAILED, "input_device_unavailable");
        return false;
    }

    // Re-assert an event's chord modifiers for this update. The engine's HID update
    // re-polls the OS keyboard right before extension updates each frame, wiping injected
    // key state -- so held modifiers must be asserted every update, and simply stopping
    // is already a clean release (one update after the primary's release, giving games
    // that track a modifier's own key_trigger the ordering a human chord produces).
    static bool AssertModifiers(InputEvent* event)
    {
        dmHID::HKeyboard keyboard = dmHID::GetKeyboard(g_AutomationBridge.m_HidContext, 0);
        if (keyboard == dmHID::INVALID_KEYBOARD_HANDLE)
        {
            return false;
        }
        for (uint8_t i = 0; i < event->m_ModifierCount; ++i)
        {
            dmHID::SetKey(keyboard, event->m_Modifiers[i], true);
        }
        return true;
    }

    static bool UpdateMouseEvent(float dt, InputEvent* event)
    {
        InputPoint* first = &event->m_Points.Begin()[0];
        if (event->m_CancelRequested && !event->m_Pressed)
        {
            FinishReceipt(&event->m_Receipt, INPUT_STATE_CANCELLED, event->m_Receipt.m_Reason);
            return true;
        }
        if (event->m_ModifierCount > 0)
        {
            if (!AssertModifiers(event))
            {
                FinishReceipt(&event->m_Receipt, INPUT_STATE_FAILED, "keyboard_unavailable");
                return true;
            }
            if (!event->m_Pressed && !event->m_ModifierLeadDone)
            {
                // Modifiers lead the pointer by one update: Defold actions carry no
                // modifier flags, so games track the modifier's own key_trigger
                // pressed/released into a flag -- its press must reach on_input before
                // the pointer action does, exactly as a human chord arrives.
                event->m_ModifierLeadDone = true;
                if (event->m_Receipt.m_State == INPUT_STATE_ACCEPTED) StartReceipt(&event->m_Receipt);
                return false;
            }
        }
        if (!event->m_Pressed)
        {
            if (!InjectPointerOrFail(event, first->m_X, first->m_Y, true, false, false))
            {
                return true;
            }
            event->m_Pressed = true;
            event->m_Phase = event->m_HoldBefore > 0.0f ? 1 : (event->m_Points.Size() > 1 ? 2 : (event->m_PointerOpen ? 4 : 3));
            event->m_Elapsed = 0.0f;
            StartReceipt(&event->m_Receipt);
            if (event->m_Visualize) AddVisualizationPoint(first->m_X, first->m_Y, true);
            return false;
        }

        InputPoint* last = &event->m_Points.Begin()[event->m_Points.Size() - 1];
        if (event->m_CancelRequested)
        {
            if (event->m_ReleaseOnCancel && !InjectPointerOrFail(event, last->m_X, last->m_Y, false, false, true)) return true;
            FinishReceipt(&event->m_Receipt, INPUT_STATE_CANCELLED, event->m_Receipt.m_Reason);
            return true;
        }

        if (event->m_Phase == 1)
        {
            event->m_Elapsed += dt;
            if (event->m_Elapsed < event->m_HoldBefore)
            {
                if (!InjectPointerOrFail(event, first->m_X, first->m_Y, false, false, false)) return true;
                return false;
            }
            event->m_Elapsed = 0.0f;
            event->m_Phase = event->m_Points.Size() > 1 ? 2 : (event->m_PointerOpen ? 4 : 3);
        }

        if (event->m_Phase == 2)
        {
            uint32_t duration_index = event->m_PathMode == INPUT_PATH_SAMPLED ? event->m_Segment + 1 : event->m_Points.Size() - 1;
            InputPoint* duration_point = &event->m_Points.Begin()[duration_index];
            float duration = MaxFloat(0.0f, duration_point->m_Duration);
            event->m_Elapsed += dt;
            float t = duration <= 0.0f ? 1.0f : event->m_Elapsed / duration;
            bool segment_done = t >= 1.0f;
            t = ApplyEasing(t, duration_point->m_Easing);
            float x = 0.0f;
            float y = 0.0f;
            SamplePath(event, t, &x, &y);
            if (!InjectPointerOrFail(event, x, y, false, false, false)) return true;
            if (event->m_Visualize) AddVisualizationPoint(x, y, false);
            if (!segment_done) return false;
            event->m_Elapsed = 0.0f;
            if (event->m_PathMode == INPUT_PATH_SAMPLED && event->m_Segment + 2 < event->m_Points.Size())
            {
                ++event->m_Segment;
                return false;
            }
            event->m_Phase = event->m_PointerOpen ? 4 : 3;
        }

        // Native HID polling replaces injected position and button state each frame.
        // Reassert the held pointer even while no path segment is advancing.
        if (event->m_Phase == 4 && !event->m_ReleaseRequested)
        {
            if (!InjectPointerOrFail(event, last->m_X, last->m_Y, false, false, false)) return true;
            return false;
        }
        if (event->m_Phase == 3 && event->m_HoldAfter > 0.0f)
        {
            event->m_Elapsed += dt;
            if (event->m_Elapsed < event->m_HoldAfter)
            {
                if (!InjectPointerOrFail(event, last->m_X, last->m_Y, false, false, false)) return true;
                return false;
            }
        }
        if (event->m_PointerOpen && !event->m_ReleaseRequested) return false;

        if (!InjectPointerOrFail(event, last->m_X, last->m_Y, false, true, false)) return true;
        FinishReceipt(&event->m_Receipt, INPUT_STATE_RELEASED, 0);
        return true;
    }

    static bool UpdateKeyEvent(float dt, InputEvent* event)
    {
        dmHID::HKeyboard keyboard = dmHID::GetKeyboard(g_AutomationBridge.m_HidContext, 0);
        if (keyboard == dmHID::INVALID_KEYBOARD_HANDLE)
        {
            FinishReceipt(&event->m_Receipt, INPUT_STATE_FAILED, "keyboard_unavailable");
            return true;
        }
        if (event->m_CancelRequested)
        {
            if (event->m_ActiveKey != dmHID::MAX_KEY_COUNT && event->m_ReleaseOnCancel)
                dmHID::SetKey(keyboard, event->m_ActiveKey, false);
            FinishReceipt(&event->m_Receipt, INPUT_STATE_CANCELLED, event->m_Receipt.m_Reason);
            return true;
        }
        if (event->m_Receipt.m_State == INPUT_STATE_ACCEPTED) StartReceipt(&event->m_Receipt);
        if (event->m_ModifierCount > 0)
        {
            AssertModifiers(event); // keyboard handle already validated above
            if (!event->m_ModifierLeadDone)
            {
                // Modifiers lead the first tap by one update -- see UpdateMouseEvent's
                // matching comment for why the ordering matters.
                event->m_ModifierLeadDone = true;
                return false;
            }
        }
        if (event->m_ActiveKey != dmHID::MAX_KEY_COUNT)
        {
            // Hold phase: the engine's own HID update re-polls the OS keyboard right before
            // extension updates each frame, wiping injected key state -- so a held key must
            // be re-asserted every update until its hold duration elapses. A side effect of
            // that same wipe is that a hold can never leave a key stuck down: simply not
            // re-asserting is already a release.
            event->m_Elapsed += dt;
            if (event->m_Elapsed < event->m_HoldAfter)
            {
                dmHID::SetKey(keyboard, event->m_ActiveKey, true);
                return false;
            }
            event->m_Elapsed = 0.0f;
            dmHID::SetKey(keyboard, event->m_ActiveKey, false);
            event->m_ActiveKey = dmHID::MAX_KEY_COUNT;
            if (event->m_KeyIndex >= strlen(event->m_Keys))
            {
                FinishReceipt(&event->m_Receipt, INPUT_STATE_RELEASED, 0);
                return true;
            }
            return false;
        }
        if (event->m_KeyIndex >= strlen(event->m_Keys))
        {
            FinishReceipt(&event->m_Receipt, INPUT_STATE_RELEASED, 0);
            return true;
        }
        char key_name[64];
        uint32_t next_index = event->m_KeyIndex;
        if (event->m_ParseSpecialKeys && ParseSpecialKey(event->m_Keys, event->m_KeyIndex, key_name, sizeof(key_name), &next_index))
        {
            dmHID::Key key = KeyFromName(key_name);
            event->m_KeyIndex = next_index;
            if (key != dmHID::MAX_KEY_COUNT)
            {
                dmHID::SetKey(keyboard, key, true);
                event->m_ActiveKey = key;
            }
            return false;
        }
        const char* cursor = event->m_Keys + event->m_KeyIndex;
        uint32_t codepoint = dmUtf8::NextChar(&cursor);
        event->m_KeyIndex = (uint32_t)(cursor - event->m_Keys);
        if (codepoint != 0) dmHID::AddKeyboardChar(g_AutomationBridge.m_HidContext, (int)codepoint);
        if (event->m_KeyIndex >= strlen(event->m_Keys))
        {
            FinishReceipt(&event->m_Receipt, INPUT_STATE_RELEASED, 0);
            return true;
        }
        return false;
    }

    bool CancelInput(uint64_t input_id, bool release, const char* reason)
    {
        InputEvent* event = FindQueuedEvent(input_id, 0);
        if (!event) return false;
        event->m_CancelRequested = true;
        event->m_ReleaseOnCancel = release;
        ReceiptSetReason(&event->m_Receipt, reason);
        return true;
    }

    uint32_t FlushInput(const char* client_id, const char* session_id, bool release, const char* reason)
    {
        uint32_t count = 0;
        for (uint32_t i = 0; i < g_AutomationBridge.m_InputEvents.Size(); ++i)
        {
            InputEvent* event = &g_AutomationBridge.m_InputEvents.Begin()[i];
            if (client_id && (!StringsEqual(event->m_Receipt.m_ClientId, client_id) ||
                              !StringsEqual(event->m_Receipt.m_SessionId, session_id)))
                continue;
            event->m_CancelRequested = true;
            event->m_ReleaseOnCancel = release;
            ReceiptSetReason(&event->m_Receipt, reason);
            ++count;
        }
        return count;
    }

    void MaintainInput()
    {
        if (!g_AutomationBridge.m_HidContext) return;
        uint64_t now = dmTime::GetMonotonicTime();
        if (!IsEmpty(g_AutomationBridge.m_ControllerClientId) && EffectiveControllerLeaseDeadline() <= now)
        {
            FlushInput(g_AutomationBridge.m_ControllerClientId, g_AutomationBridge.m_ControllerSessionId, true, "controller_lease_expired");
            ReleaseInputController(g_AutomationBridge.m_ControllerClientId, g_AutomationBridge.m_ControllerSessionId);
        }
        for (uint32_t i = 0; i < g_AutomationBridge.m_InputEvents.Size();)
        {
            InputEvent* event = &g_AutomationBridge.m_InputEvents[i];
            if (event->m_PointerOpen && event->m_LeaseDeadline <= now)
            {
                event->m_CancelRequested = true;
                event->m_ReleaseOnCancel = true;
                ReceiptSetReason(&event->m_Receipt, "pointer_lease_expired");
            }
            // Service iterations can run without simulation or rendering. Only
            // release cancelled input here; new input starts after HID sampling.
            if (event->m_CancelRequested &&
                (event->m_Type == INPUT_EVENT_KEYS ? UpdateKeyEvent(0.0f, event) : UpdateMouseEvent(0.0f, event)))
                RemoveFinishedEvent(i);
            else ++i;
        }
    }

    void UpdateInput(float dt)
    {
        if (!g_AutomationBridge.m_HidContext) return;
        uint64_t now = dmTime::GetMonotonicTime();
        if (g_AutomationBridge.m_InputEvents.Size() > 0 &&
            g_AutomationBridge.m_InputEvents.Begin()[0].m_Receipt.m_State == INPUT_STATE_ACCEPTED)
        {
            RefreshKeyCompletionDeadline(&g_AutomationBridge.m_InputEvents.Begin()[0], now);
        }
        MaintainInput();
        // A service-tick cancellation may have staged a release before HID polling.
        // Dispatch it before starting another queued touch with the same pointer ID.
        if (dmHID::HasSyntheticTouch(g_AutomationBridge.m_HidContext)) return;
        if (g_AutomationBridge.m_InputEvents.Size() == 0) return;

        InputEvent* event = &g_AutomationBridge.m_InputEvents.Begin()[0];
        if (event->m_ReleaseModifiers) { RemoveFinishedEvent(0); return; }
        if (!event->m_TargetValidated && (!IsEmpty(event->m_Target) || !IsEmpty(event->m_EndTarget)))
        {
            UpdateSnapshot();
            g_AutomationBridge.m_SnapshotFrame = g_AutomationBridge.m_Frame;
            const char* ids[] = {event->m_Target, event->m_EndTarget};
            for (uint32_t i = 0; i < 2; ++i)
            {
                if (IsEmpty(ids[i])) continue;
                float x, y;
                const char* error = 0;
                if (!GetNodeCenter(ids[i], &x, &y, &error))
                {
                    FinishReceipt(&event->m_Receipt, INPUT_STATE_FAILED, "stale_element");
                    RemoveFinishedEvent(0);
                    return;
                }
                uint32_t point = i == 0 ? 0 : event->m_Points.Size() - 1;
                event->m_Points[point].m_X = x;
                event->m_Points[point].m_Y = y;
            }
            event->m_TargetValidated = true;
        }
        if (event->m_PointerOpen && event->m_LeaseDeadline <= now)
        {
            event->m_CancelRequested = true;
            event->m_ReleaseOnCancel = true;
            ReceiptSetReason(&event->m_Receipt, "pointer_lease_expired");
        }
        uint32_t previous_key_index = event->m_KeyIndex;
        dmHID::Key previous_active_key = event->m_ActiveKey;
        bool done = event->m_Type == INPUT_EVENT_KEYS ? UpdateKeyEvent(dt, event) : UpdateMouseEvent(dt, event);
        if (!done && event->m_Type == INPUT_EVENT_KEYS &&
            (event->m_KeyIndex != previous_key_index || event->m_ActiveKey != previous_active_key))
        {
            RefreshKeyCompletionDeadline(event, dmTime::GetMonotonicTime());
        }
        if (done)
        {
            // Keep the chord through the primary release dispatch. Explicitly release
            // it on the following sampling iteration, including on the null HID backend.
            if (event->m_ModifierCount && event->m_Receipt.m_StartFrame && !event->m_CancelRequested)
                event->m_ReleaseModifiers = true;
            else RemoveFinishedEvent(0);
        }
    }
}
