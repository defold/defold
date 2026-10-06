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

#pragma once

// Keep native graphics headers out of common code: X11's Font typedef conflicts with text layout.
#include <dmsdk/dlib/array.h>
#include <dmsdk/dlib/configfile_gen.hpp>
#include <dmsdk/dlib/dstrings.h>
#include <dmsdk/dlib/log.h>
#include <dmsdk/dlib/mutex.h>
#include <dmsdk/dlib/time.h>
#include <dmsdk/dlib/webserver.h>
#include <dmsdk/engine/extension.h>
#include <dmsdk/extension/extension.hpp>
#include <dmsdk/graphics/graphics.h>
#include <dmsdk/lua/lauxlib.h>
#include <dmsdk/render/render.h>
#include <dmsdk/script.h>

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

namespace dmAutomation
{
    static const char* API_PREFIX = "/automation-bridge/v3";
    static const char* API_VERSION = "3";
    static const uint32_t MAX_INPUT_EVENTS = 64;
    static const uint32_t MAX_INPUT_HISTORY = 256;
    static const uint32_t MAX_INPUT_PATH_POINTS = 128;
    static const uint32_t MAX_INPUT_MODIFIERS = 4;
    static const float MAX_INPUT_DURATION = 60.0f;
    static const uint32_t MAX_KEY_INPUT_BYTES = 4096;
    static const uint32_t MAX_APPLICATION_JSON_BYTES = 32768;
    static const uint32_t MAX_APPLICATION_NAME_BYTES = 128;
    static const uint32_t MAX_JSON_REQUEST_BYTES = 64 * 1024;
    static const uint32_t MAX_JSON_NESTING = 16;
    static const uint32_t MAX_METAL_CAPTURE_FRAMES = 10000;

    enum MetalCaptureState
    {
        METAL_CAPTURE_IDLE,
        METAL_CAPTURE_PENDING,
        METAL_CAPTURE_CAPTURING,
        METAL_CAPTURE_COMPLETE,
        METAL_CAPTURE_CANCELED,
        METAL_CAPTURE_FAILED
    };

    struct StringBuffer
    {
        char*    m_Data;
        uint32_t m_Size;
        uint32_t m_Capacity;
        bool     m_Failed;
    };

    struct QueryParam
    {
        char* m_Key;
        char* m_Value;
    };

    struct Property
    {
        char*   m_Name;
        char*   m_Json;
        char*   m_StringValue;
        float   m_Vector[4];
        uint8_t m_VectorCount;
        bool    m_HasVector;
        bool    m_BoolValue;
        bool    m_HasBool;
    };

    struct Bounds
    {
        bool  m_Valid;
        bool  m_Ambiguous;
        float m_X;
        float m_Y;
        float m_W;
        float m_H;
        float m_CX;
        float m_CY;
        float m_NX;
        float m_NY;
    };

    struct Node
    {
        char*                m_Id;
        char*                m_InstanceId;
        char*                m_LogicalId;
        char*                m_Name;
        char*                m_Type;
        char*                m_Kind;
        char*                m_Path;
        char*                m_Parent;
        char*                m_Text;
        char*                m_Url;
        char*                m_Resource;
        char*                m_AutomationId;
        char*                m_LocalizationKey;
        char*                m_Role;
        bool                 m_SnapshotScoped;
        bool                 m_Visible;
        bool                 m_Enabled;
        Bounds               m_Bounds;
        dmArray<Property>       m_Properties;
        dmArray<uint32_t>       m_Children;
        uint64_t              m_CreatedSceneSequence;
        uint32_t              m_InstanceGeneration;
        bool                  m_HasInstanceIdentity;

        bool  m_HasPosition;
        bool  m_HasSize;
        float m_Position[3];
        float m_Size[3];
        float m_AnchorX;
        float m_AnchorY;
    };

    struct Snapshot
    {
        dmArray<Node>      m_Nodes;
        int32_t          m_Root;
        uint32_t         m_WindowWidth;
        uint32_t         m_WindowHeight;
        uint32_t         m_BackbufferWidth;
        uint32_t         m_BackbufferHeight;
        uint32_t         m_DisplayWidth;
        uint32_t         m_DisplayHeight;
        float            m_DisplayScale;
        int32_t          m_ViewportX;
        int32_t          m_ViewportY;
        uint32_t         m_ViewportWidth;
        uint32_t         m_ViewportHeight;
        uint64_t         m_Sequence;
        uint64_t         m_Frame;
    };

    enum ScreenshotState
    {
        SCREENSHOT_NONE,
        SCREENSHOT_PENDING,
        SCREENSHOT_COMPLETE,
        SCREENSHOT_FAILED
    };

    struct ScreenshotCapture
    {
        uint64_t        m_Id;
        ScreenshotState m_State;
        uint32_t        m_AfterFrames;
        uint64_t        m_Frame;
        uint64_t        m_SceneSequence;
        uint32_t        m_Width;
        uint32_t        m_Height;
        char            m_Path[1024];
        char            m_Sha256[65];
        char            m_Failure[128];
    };

    enum InputEventType
    {
        INPUT_EVENT_MOUSE,
        INPUT_EVENT_KEYS,
        INPUT_EVENT_POINTER
    };

    enum InputDevice
    {
        INPUT_DEVICE_AUTO,
        INPUT_DEVICE_MOUSE,
        INPUT_DEVICE_TOUCH
    };

    enum InputState
    {
        INPUT_STATE_ACCEPTED,
        INPUT_STATE_STARTED,
        INPUT_STATE_RELEASED,
        INPUT_STATE_CANCELLED,
        INPUT_STATE_FAILED
    };

    enum InputEasing
    {
        INPUT_EASING_LINEAR,
        INPUT_EASING_EASE_IN,
        INPUT_EASING_EASE_OUT,
        INPUT_EASING_EASE_IN_OUT
    };

    enum InputPathMode
    {
        INPUT_PATH_SAMPLED,
        INPUT_PATH_QUADRATIC,
        INPUT_PATH_CUBIC
    };

    struct InputPoint
    {
        float       m_X;
        float       m_Y;
        float       m_Duration;
        InputEasing m_Easing;
    };

    struct InputReceipt
    {
        uint64_t    m_Id;
        char*       m_Kind;
        InputState  m_State;
        uint64_t    m_AcceptedFrame;
        uint64_t    m_StartFrame;
        uint64_t    m_ReleaseFrame;
        uint64_t    m_DeliveredFrame;
        uint64_t    m_AcceptedTime;
        uint64_t    m_StartTime;
        uint64_t    m_ReleaseTime;
        float       m_RequestedDuration;
        char*       m_Reason;
        char*       m_ClientId;
        char*       m_SessionId;
        char*       m_RequestId;
        uint64_t    m_SceneSequence;
        InputDevice m_Device;
        uint32_t    m_PointerId;
        uint8_t     m_ModifierCount;
    };

    struct InputEvent
    {
        char* m_Target;
        char* m_EndTarget;
        bool m_TargetValidated;
        bool m_ReleaseModifiers;
        InputReceipt      m_Receipt;
        InputEventType     m_Type;
        dmArray<InputPoint>   m_Points;
        InputPathMode       m_PathMode;
        uint32_t            m_Segment;
        uint8_t             m_Phase;
        float              m_Elapsed;
        float              m_HoldBefore;
        float              m_HoldAfter;
        bool               m_Pressed;
        bool               m_ReleaseRequested;
        bool               m_CancelRequested;
        bool               m_ReleaseOnCancel;
        bool               m_PointerOpen;
        uint64_t           m_LeaseDeadline;
        uint64_t           m_CompletionDeadline;
        dmHID::MouseButton m_MouseButton;
        bool               m_Visualize;

        char*      m_Keys;
        uint32_t   m_KeyIndex;
        dmHID::Key m_ActiveKey;
        bool       m_ParseSpecialKeys;

        // Chord modifiers held for the whole event (see UpdateMouseEvent/UpdateKeyEvent):
        // re-asserted every update while the event plays, leading the primary action by
        // one update and trailing its release by one (the engine's per-frame HID re-poll
        // releases them automatically once assertion stops).
        dmHID::Key m_Modifiers[MAX_INPUT_MODIFIERS];
        uint8_t    m_ModifierCount;
        bool       m_ModifierLeadDone;
    };

    struct InputVisualization
    {
        bool  m_Active;
        bool  m_Drag;
        float m_X[MAX_INPUT_PATH_POINTS];
        float m_Y[MAX_INPUT_PATH_POINTS];
        uint32_t m_PointCount;
        float m_Age;
        float m_Duration;
        uint64_t m_LastRenderTime;
    };

    struct BridgeEvent
    {
        uint64_t m_Sequence;
        uint64_t m_Frame;
        uint64_t m_SceneSequence;
        uint64_t m_NativeTimestampUs;
        uint64_t m_RecordingTimestampUs;
        bool     m_HasRecordingTimestamp;
        char*    m_Type;
        char*    m_Name;
        char*    m_DataJson;
    };

    struct PublishedState
    {
        char*    m_Name;
        char*    m_ValueJson;
        uint64_t m_Revision;
        uint64_t m_Frame;
        uint64_t m_NativeTimestampUs;
    };

    struct CommandHandler
    {
        uintptr_t m_Owner;
        char*                      m_Name;
        dmScript::LuaCallbackInfo* m_Callback;
    };

    struct ApplicationContract
    {
        char* m_Kind;
        char* m_Name;
        char* m_MetadataJson;
    };

    enum CommandState
    {
        COMMAND_PENDING,
        COMMAND_RUNNING,
        COMMAND_COMPLETED,
        COMMAND_FAILED,
        COMMAND_CANCELLED,
        COMMAND_TIMED_OUT
    };

    struct CommandInvocation
    {
        uint64_t     m_Id;
        CommandState m_State;
        char*        m_Name;
        char*        m_ArgumentsJson;
        char*        m_ResultJson;
        char*        m_Error;
        uint64_t     m_AcceptedTimestampUs;
        uint64_t     m_CompletedTimestampUs;
        uint64_t     m_DeadlineTimestampUs;
    };

    struct NodeAnnotation
    {
        uintptr_t m_Owner;
        char* m_NodeUrl;
        char* m_AutomationId;
        char* m_LocalizationKey;
        char* m_Role;
    };

    struct AutomationBridgeContext
    {
        bool                    m_Initialized;
        dmGameObject::HRegister m_Register;
        dmHID::HContext         m_HidContext;
        dmGraphics::HContext    m_GraphicsContext;
        dmRender::HRenderContext m_RenderContext;
        uint64_t                m_LastTime;
        uint64_t                m_Frame;
        uint64_t                m_DispatchedFrame;
        uint64_t                m_SnapshotFrame;
        Snapshot                m_Snapshot;
        dmArray<InputEvent>      m_InputEvents;
        dmArray<InputReceipt>    m_InputHistory;
        InputVisualization      m_InputVisualization;
        uint64_t                m_NextInputId;
        char*                   m_ControllerClientId;
        char*                   m_ControllerSessionId;
        uint64_t                m_ControllerLeaseDeadline;
        InputDevice             m_DefaultInputDevice;
        bool                    m_DefaultInputVisualize;
        char                    m_EngineInstanceId[64];
        ScreenshotCapture       m_Screenshot;
        dmArray<ScreenshotCapture> m_ScreenshotHistory;
        uint64_t                m_ScreenshotCounter;
        MetalCaptureState       m_MetalCaptureState;
        uint32_t                m_MetalCaptureFrames;
        uint32_t                m_MetalCaptureFramesCaptured;
        bool                    m_MetalCaptureStopRequested;
        char                    m_MetalCapturePath[1024];
        char                    m_MetalCaptureError[512];
        uint32_t                m_DisplayWidth;
        uint32_t                m_DisplayHeight;
        bool                    m_ApplicationApiEnabled;
        bool                    m_CommandExecuting;
        bool                    m_DebuggerPaused;
        dmWebServer::HServer     m_WebServer;
        dmMutex::HMutex         m_ApplicationMutex;
        dmArray<BridgeEvent>      m_Events;
        dmArray<PublishedState>   m_PublishedStates;
        dmArray<CommandHandler>   m_CommandHandlers;
        dmArray<ApplicationContract> m_ApplicationContracts;
        uint64_t                m_CatalogRevision;
        dmArray<CommandInvocation> m_CommandInvocations;
        dmArray<NodeAnnotation>   m_NodeAnnotations;
        uint32_t                m_EventCapacity;
        uint64_t                m_NextEventSequence;
        uint64_t                m_NextStateRevision;
        uint64_t                m_NextCommandId;
        char                    m_ProjectIdentity[32];
        char                    m_BuildIdentity[32];
        uint64_t                m_StartWallTime;
        uint64_t                m_StartMonotonicTime;
        uint64_t                m_RegisteredMonotonicTime;
        uint64_t                m_FirstHealthMonotonicTime;
        uint64_t                m_SceneReadyMonotonicTime;
        int64_t                 m_ProcessId;
    };

    struct IncludeOptions
    {
        bool m_Bounds;
        bool m_Properties;
        bool m_Children;
    };

    typedef dmArray<uint8_t> ByteArray;

    extern AutomationBridgeContext g_AutomationBridge;
    void PrepareObservations(bool rendered);
    void CompleteObservations();
    void FinalizeObservations();

    template <typename T>
    static void ArrayInit(dmArray<T>* array) { array->SetSize(0); }

    template <typename T>
    static void ArrayFree(dmArray<T>* array) { array->SetCapacity(0); }

    template <typename T>
    static bool ArrayReserve(dmArray<T>* array, uint32_t capacity)
    {
        if (capacity > UINT32_MAX / sizeof(T)) return false;
        if (capacity > array->Capacity()) array->SetCapacity(capacity < 8 ? 8 : capacity * 2);
        return true;
    }

    template <typename T>
    static bool ArraySetCount(dmArray<T>* array, uint32_t count)
    {
        if (!ArrayReserve(array, count)) return false;
        array->SetSize(count);
        return true;
    }

    template <typename T>
    static bool ArrayPush(dmArray<T>* array, const T* value)
    {
        if (!ArrayReserve(array, array->Size() + 1)) return false;
        array->PushArray(value, 1);
        return true;
    }

    template <typename T>
    static void ArrayErase(dmArray<T>* array, uint32_t index)
    {
        if (index + 1 < array->Size())
            memmove(array->Begin() + index, array->Begin() + index + 1, (array->Size() - index - 1) * sizeof(T));
        array->Pop();
    }

    static inline float MinFloat(float a, float b)
    {
        return a < b ? a : b;
    }

    static inline float MaxFloat(float a, float b)
    {
        return a > b ? a : b;
    }

    static inline float ClampFloat(float value, float min_value, float max_value)
    {
        return MaxFloat(min_value, MinFloat(value, max_value));
    }

    static inline bool IsEmpty(const char* value)
    {
        return value == 0 || value[0] == 0;
    }

    static inline bool StringsEqual(const char* a, const char* b)
    {
        if (a == 0)
        {
            a = "";
        }
        if (b == 0)
        {
            b = "";
        }
        return strcmp(a, b) == 0;
    }

    static inline bool StartsWith(const char* value, const char* prefix)
    {
        if (!value || !prefix)
        {
            return false;
        }
        size_t prefix_len = strlen(prefix);
        return strncmp(value, prefix, prefix_len) == 0;
    }

    static inline bool IsFiniteFloat(float value)
    {
        return isfinite(value) != 0;
    }

    static inline bool IsFiniteDouble(double value)
    {
        return isfinite(value) != 0;
    }

    char* DuplicateStringN(const char* value, uint32_t length);
    char* DuplicateString(const char* value);
    bool SetString(char** target, const char* value);
    void FreeString(char** value);
    void StringBufferInit(StringBuffer* buffer);
    void StringBufferFree(StringBuffer* buffer);
    bool StringBufferReserve(StringBuffer* buffer, uint32_t size);
    void StringBufferAppendN(StringBuffer* buffer, const char* value, uint32_t length);
    void StringBufferAppend(StringBuffer* buffer, const char* value);
    void StringBufferAppendChar(StringBuffer* buffer, char value);
    char* StringBufferDetach(StringBuffer* buffer);
    void AppendNumber(StringBuffer* out, double value);
    void AppendJsonString(StringBuffer* out, const char* value);
    bool SetHashString(char** out, dmhash_t hash);
    void FreeQueryParams(dmArray<QueryParam>* query);
    void JsonSkipWhitespace(const char** cursor);
    bool JsonParseString(const char** cursor, char** value);
    bool JsonPushParam(dmArray<QueryParam>* params, const char* key, char* value);
    bool JsonValidate(const char* json, uint32_t max_bytes);
    bool JsonParseRequest(const char* body, dmArray<QueryParam>* params, dmArray<QueryParam>* fields);
    void ParseResource(const char* resource, char** path, dmArray<QueryParam>* query);
    const char* GetParam(const dmArray<QueryParam>* query, const char* key);
    bool GetFloatParam(const dmArray<QueryParam>* query, const char* key, float* value);
    bool GetBoolParam(const dmArray<QueryParam>* query, const char* key, bool* value);
    bool ContainsCaseInsensitive(const char* haystack, const char* needle);
    bool ContainsCaseSensitive(const char* haystack, const char* needle);

    void InitSnapshot(Snapshot* snapshot);
    void FreeSnapshot(Snapshot* snapshot);
    void UpdateSnapshot();
    const Node* FindNodeById(const char* id);
    void AppendNodeJson(StringBuffer* out, const Snapshot* snapshot, const Node* node, const IncludeOptions* include, bool recursive, bool visible_only);
    void RefreshScreenInfo(Snapshot* snapshot);
    void AppendScreenJson(StringBuffer* out, const Snapshot* snapshot);
    void ApplyNodeAnnotation(Node* node);

    void FreeInputReceipt(InputReceipt* receipt);
    void FreeInputEvent(InputEvent* event);
    const char* InputStateName(InputState state);
    const char* InputDeviceName(InputDevice device);
    bool ParseInputDevice(const char* value, InputDevice* device);
    bool ParseInputEasing(const char* value, InputEasing* easing);
    bool IsInputDeviceSupported(InputDevice device);
    InputDevice ResolveInputDevice(InputDevice device);
    bool AcquireInputController(const char* client_id, const char* session_id, float lease, const char** error);
    bool IsInputController(const char* client_id, const char* session_id);
    void ReleaseInputController(const char* client_id, const char* session_id);
    bool AddMouseInput(const dmArray<InputPoint>* points, InputPathMode path_mode, float hold_before, float hold_after,
                       InputDevice device, uint32_t pointer_id, bool visualize, const char* kind,
                       const dmHID::Key* modifiers, uint32_t modifier_count,
                       const char* client_id, const char* session_id, const char* request_id,
                       uint64_t scene_sequence, float lease, bool pointer_open, InputReceipt** receipt);
    dmHID::Key KeyFromName(const char* name);
    bool ValidateSpecialKeyInput(const char* keys, const char** error, uint32_t* out_special_key_count);

    bool AddKeyInput(const char* keys, bool parse_special_keys, float key_hold, float requested_duration,
                     const dmHID::Key* modifiers, uint32_t modifier_count,
                     const char* client_id, const char* session_id, const char* request_id,
                     uint64_t scene_sequence, InputReceipt** receipt);
    bool AppendPointerMove(uint64_t input_id, const InputPoint* point, float lease, const char** error);
    bool AppendPointerHold(uint64_t input_id, float duration, float lease, const char** error);
    bool ReleasePointer(uint64_t input_id, const char** error);
    const InputReceipt* FindInputReceipt(uint64_t input_id);
    void AppendInputReceiptJson(StringBuffer* out, const InputReceipt* receipt, uint32_t queue_position);
    bool CancelInput(uint64_t input_id, bool release, const char* reason);
    uint32_t FlushInput(const char* client_id, const char* session_id, bool release, const char* reason);
    bool GetNodeCenter(const char* id, float* x, float* y, const char** error);
    void MaintainInput();
    void UpdateInput(float dt);

    bool IsScreenshotSupported();
    bool BuildScreenshotPath(uint64_t capture_id, char* path, uint32_t path_size);
    bool ScheduleScreenshot(uint32_t after_frames, ScreenshotCapture* capture);
    void ProcessPendingScreenshot();
    void AppendScreenshotJson(StringBuffer* out, const ScreenshotCapture* capture);
    const ScreenshotCapture* FindScreenshotCapture(uint64_t capture_id);

    void InitApplicationBridge(uint32_t event_capacity, bool enabled);
    void FreeApplicationBridge();
    void FinalizeApplicationLua();
    void RegisterApplicationLua(lua_State* L, dmScript::HContext context);
    void UpdateApplicationBridge();
    bool EmitBridgeEvent(const char* type, const char* name, const char* data_json, uint64_t recording_timestamp_us, bool has_recording_timestamp);
    bool EmitBridgeEventWithReceipt(const char* type, const char* name, const char* data_json, uint64_t recording_timestamp_us, bool has_recording_timestamp, uint64_t* event_sequence, uint64_t* native_timestamp_us);
    uint64_t GetEventNextCursor();
    uint64_t GetEventOldestCursor();
    uint32_t AppendEventPageJson(StringBuffer* out, uint64_t cursor, uint32_t limit, bool* overflow, uint64_t* next_cursor);
    uint64_t GetStateRevision();
    uint32_t AppendPublishedStatesJson(StringBuffer* out, const char* name, uint64_t after_revision);
    void AppendApplicationCatalogJson(StringBuffer* out, const char* kind, const char* name, uint32_t offset, uint32_t limit);
    bool SubmitCommand(const char* name, const char* arguments_json, uint32_t timeout_ms, uint64_t* command_id, const char** error);
    bool AppendCommandJson(StringBuffer* out, uint64_t command_id);
    bool CancelCommand(uint64_t command_id, const char** error);
    bool AddTimelineMarker(const char* name, const char* data_json, uint64_t recording_timestamp_us, bool has_recording_timestamp, uint64_t* event_sequence, uint64_t* native_timestamp_us);

    bool IsMetalCaptureSupported();
    bool ScheduleMetalCapture(const char* path, uint32_t frames);
    bool RequestMetalCaptureStop();
    void ProcessPendingMetalCaptureStart();
    void ProcessMetalCapturePostRender();
    void StopMetalCapture();

    void RegisterWebEndpoint(dmExtension::AppParams* params);
    void AutomationBridgeHandler(void* user_data, dmWebServer::Request* request);
}
