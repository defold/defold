#include <stdio.h>
#include <stdint.h>

//#pragma pack(1)

// enum Phase //: uint8_t
// {
//     PHASE_BEGAN = 0,
//     PHASE_MOVED = 1,
//     PHASE_STATIONARY = 2,
//     PHASE_ENDED = 3,
//     PHASE_CANCELLED = 4,
// };

// struct Touch
// {
//     /// Single-click, double, etc
//     uint16_t m_TapCount;
//     /// Current x
//     int16_t m_X;
//     /// Current y
//     int16_t m_Y;
//     /// Current x, in screen space
//     int16_t m_ScreenX;
//     /// Current y, in screen space
//     int16_t m_ScreenY;
//     /// Current dx
//     int16_t m_DX;
//     /// Current dy
//     int16_t m_DY;
//     /// Current dx, in screen space
//     int16_t m_ScreenDX;
//     /// Current dy, in screen space
//     int16_t m_ScreenDY;
//     /// Touch id
//     int16_t m_Id;
//     /// Begin, end, etc
//     Phase   m_Phase;
// };

// const static uint32_t MAX_GAMEPAD_AXIS_COUNT = 32;
// const static uint32_t MAX_GAMEPAD_BUTTON_COUNT = 32;
// const static uint32_t MAX_GAMEPAD_HAT_COUNT = 4;
// const static uint32_t MAX_TOUCH_COUNT = 11;
// const static uint32_t MAX_CHAR_COUNT = 256;

// struct GamepadPacket
// {
//     float    m_Axis[MAX_GAMEPAD_AXIS_COUNT];
//     uint32_t m_Buttons[MAX_GAMEPAD_BUTTON_COUNT / 32 + 1];
//     uint8_t  m_Hat[MAX_GAMEPAD_HAT_COUNT];
//     uint8_t  m_GamepadDisconnected:1;
//     uint8_t  m_GamepadConnected:1;
//     uint8_t  :6;
// };

// struct Action
// {
//     union {
//         Touch           m_Touch[MAX_TOUCH_COUNT];
//         char            m_Text[MAX_CHAR_COUNT];
//         GamepadPacket   m_GamepadPacket;
//     };
//     union {
//         uintptr_t m_Gamepad;
//     };

//     float m_Value;
//     float m_PrevValue;
//     float m_RepeatTimer;
//     float m_AccX;
//     float m_AccY;
//     float m_AccZ;
//     int16_t m_X;
//     int16_t m_Y;
//     int16_t m_DX;
//     int16_t m_DY;

//     /// Text or touch count
//     int16_t      m_Count;
//     uint16_t     m_GamepadIndex;
//     uint16_t     m_UserID;

//     uint16_t m_IsGamepad : 1;
//     uint16_t m_GamepadUnknown : 1;
//     uint16_t m_GamepadDisconnected : 1;
//     uint16_t m_GamepadConnected : 1;
//     uint16_t m_HasGamepadPacket : 1;
//     uint16_t m_Pressed : 1;
//     uint16_t m_Released : 1;
//     uint16_t m_Repeated : 1;
//     uint16_t m_PositionSet : 1;
//     uint16_t m_AccelerationSet : 1;
//     uint16_t m_HasText : 1;
//     uint16_t m_Dirty : 1; // it's dirty and should report its value
//     uint16_t : 4;
// };

// struct ActionOld
// {
//     float m_Value;
//     float m_PrevValue;
//     float m_RepeatTimer;
//     int16_t m_X;
//     int16_t m_Y;
//     int16_t m_DX;
//     int16_t m_DY;
//     float m_AccX;
//     float m_AccY;
//     float m_AccZ;
//     union {
//         Touch m_Touch[MAX_TOUCH_COUNT];
//         char  m_Text[MAX_CHAR_COUNT];
//     };
//     /// Text or touch count
//     int16_t      m_Count;
//     uint16_t     m_GamepadIndex;
//     uint16_t     m_UserID;
//     GamepadPacket m_GamepadPacket;

//     uint16_t m_IsGamepad : 1;
//     uint16_t m_GamepadUnknown : 1;
//     uint16_t m_GamepadDisconnected : 1;
//     uint16_t m_GamepadConnected : 1;
//     uint16_t m_HasGamepadPacket : 1;
//     uint16_t m_Pressed : 1;
//     uint16_t m_Released : 1;
//     uint16_t m_Repeated : 1;
//     uint16_t m_PositionSet : 1;
//     uint16_t m_AccelerationSet : 1;
//     uint16_t m_HasText : 1;
//     uint16_t m_Dirty : 1; // it's dirty and should report its value
//     uint16_t : 4;
// };

// struct GamepadGuid
// {
//     uint16_t m_Bus;
//     uint16_t m_CRC16;
//     uint16_t m_Vendor;
//     uint16_t m_Reserved0;
//     uint16_t m_Product;
//     uint16_t m_Reserved1;
//     uint16_t m_Version;
//     uint8_t  m_DriverSignature;
//     uint8_t  m_DriverData;
// };


// struct InputAction
// {
//         union {
//             Touch           m_Touch[MAX_TOUCH_COUNT];
//             char            m_Text[MAX_CHAR_COUNT];  /// Contains text input if m_HasText, and gamepad name if m_GamepadConnected
//             GamepadPacket   m_GamepadPacket;
//         };

//         union {
//             GamepadGuid     m_GamepadGuid;
//         };

//         /// Action id, hashed action name
//         uint64_t m_ActionId;
//         /// Value of the input [0,1]
//         float m_Value;
//         /// Cursor X coordinate, in virtual screen space
//         float m_X;
//         /// Cursor Y coordinate, in virtual screen space
//         float m_Y;
//         /// Cursor dx since last frame, in virtual screen space
//         float m_DX;
//         /// Cursor dy since last frame, in virtual screen space
//         float m_DY;
//         /// Cursor X coordinate, in screen space
//         float m_ScreenX;
//         /// Cursor Y coordinate, in screen space
//         float m_ScreenY;
//         /// Cursor dx since last frame, in screen space
//         float m_ScreenDX;
//         /// Cursor dy since last frame, in screen space
//         float m_ScreenDY;
//         /// Accelerometer x value (if present)
//         float m_AccX;
//         /// Accelerometer y value (if present)
//         float m_AccY;
//         /// Accelerometer z value (if present)
//         float m_AccZ;

//         /// Text or touch count
//         int16_t m_Count;
//         uint16_t m_GamepadIndex;
//         uint16_t m_UserID;

//         uint16_t  m_IsGamepad : 1;
//         uint16_t  m_GamepadUnknown : 1;
//         uint16_t  m_GamepadDisconnected : 1;
//         uint16_t  m_GamepadConnected : 1;
//         uint16_t  m_HasGamepadPacket : 1;
//         /// If input has a text payload (can be true even if text count is 0)
//         uint16_t  m_HasText : 1;
//         /// If the input was 0 last update
//         uint16_t  m_Pressed : 1;
//         /// If the input turned from above 0 to 0 this update
//         uint16_t  m_Released : 1;
//         /// If the input was held enough for the value to be repeated this update
//         uint16_t  m_Repeated : 1;
//         /// If the position fields (m_X, m_Y, m_DX, m_DY) were set and valid to read
//         uint16_t  m_PositionSet : 1;
//         /// If the accelerometer fields (m_AccX, m_AccY, m_AccZ) were set and valid to read
//         uint16_t  m_AccelerationSet : 1;
//         /// If the input action was consumed in an event dispatch
//         uint16_t  m_Consumed : 1;
//         uint16_t  : 4;

// };
// //#pragma pack(pop)


struct Point3
{
    float x, y, z, w;
};

struct Vector3
{
    float x, y, z, w;
};

struct LightInstance // 48
{
    Point3       m_Position;
    Vector3      m_Direction;
    const void*  m_LightPrototype;
    float        m_Scale;
    uint16_t     m_LightBufferIndex;
};

struct LightInstance_v2 // 40
{
    const void*     m_LightPrototype;
    float           m_Position[3];
    float           m_Direction[3];
    float           m_Scale;
    uint16_t        m_LightBufferIndex;
};

struct LightInstance_v3 // 26 bytes
{
    float                 m_Position[3];
    float                 m_Direction[3];
    float                 m_Scale;
    uint32_t              m_LightPrototypeHandle;
    uint16_t              m_LightBufferIndex;
};

struct LightInstance_v4 // 8 bytes
{
    uint32_t              m_LightPrototypeHandle;
    uint16_t              m_LightBufferIndex;
};


struct LightInstance_v5 // 8 bytes
{
    void*                   m_LightPrototypeHandle;
    uint16_t                m_LightBufferIndex;
};


int main(int argc, char const *argv[])
{
    // printf("sizeof(Phase):          %zu\n", sizeof(Phase));
    // printf("sizeof(Touch):          %zu\n", sizeof(Touch));
    // printf("sizeof(GamepadPacket):  %zu\n", sizeof(GamepadPacket));
    // printf("sizeof(Action):         %zu\n", sizeof(Action));
    // printf("sizeof(InputAction):    %zu\n", sizeof(InputAction));

    printf("sizeof(LightInstance):       %zu\n", sizeof(LightInstance));
    printf("sizeof(LightInstance_v2):    %zu\n", sizeof(LightInstance_v2));
    printf("sizeof(LightInstance_v3):    %zu\n", sizeof(LightInstance_v3));
    printf("sizeof(LightInstance_v4):    %zu\n", sizeof(LightInstance_v4));
    printf("sizeof(LightInstance_v5):    %zu\n", sizeof(LightInstance_v5));
    return 0;
}












