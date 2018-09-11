#include <jni.h>
#include <stdlib.h>
#include <unistd.h>
#include <dlib/time.h>
#include <dlib/array.h>
#include <dlib/log.h>
#include <dlib/dstrings.h>
#include <dlib/json.h>
#include <script/script.h>
#include <extension/extension.h>
#include <android_native_app_glue.h>
#include "push_utils.h"

#define LIB_NAME "push"

extern struct android_app* g_AndroidApp;

struct Push;

#define CMD_REGISTRATION_RESULT  (0)
#define CMD_PUSH_MESSAGE_RESULT  (1)
#define CMD_LOCAL_MESSAGE_RESULT (2)

struct Command
{
    Command()
    {
        memset(this, 0, sizeof(*this));
    }
    uint32_t m_Command;
    int32_t  m_ResponseCode;
    void*    m_Data1;
    void*    m_Data2;
    bool     m_WasActivated;
};

static JNIEnv* Attach()
{
    JNIEnv* env;
    g_AndroidApp->activity->vm->AttachCurrentThread(&env, NULL);
    return env;
}

static void Detach()
{
    g_AndroidApp->activity->vm->DetachCurrentThread();
}

struct ScheduledNotification
{
    int32_t id;
    uint64_t timestamp; // in microseconds
    char* title;
    char* message;
    char* payload;
    int priority;
};

struct PushListener
{
    PushListener()
    {
        m_L = 0;
        m_Callback = LUA_NOREF;
        m_Self = LUA_NOREF;
    }
    lua_State* m_L;
    int        m_Callback;
    int        m_Self;
};

struct Push
{
    Push()
    {
        memset(this, 0, sizeof(*this));
        m_Callback = LUA_NOREF;
        m_Self = LUA_NOREF;
        m_Listener.m_Callback = LUA_NOREF;
        m_Listener.m_Self = LUA_NOREF;
        m_ScheduledNotifications.SetCapacity(8);
    }
    int                  m_Callback;
    int                  m_Self;
    lua_State*           m_L;
    PushListener         m_Listener;

    jobject              m_Push;
    jobject              m_PushJNI;
    jmethodID            m_Start;
    jmethodID            m_Stop;
    jmethodID            m_Register;
    jmethodID            m_Schedule;
    jmethodID            m_Cancel;
    int                  m_Pipefd[2];

    dmArray<ScheduledNotification> m_ScheduledNotifications;
};

Push g_Push;

static void VerifyCallback(lua_State* L)
{
    if (g_Push.m_Callback != LUA_NOREF) {
        dmLogError("Unexpected callback set");
        dmScript::Unref(L, LUA_REGISTRYINDEX, g_Push.m_Callback);
        dmScript::Unref(L, LUA_REGISTRYINDEX, g_Push.m_Self);
        g_Push.m_Callback = LUA_NOREF;
        g_Push.m_Self = LUA_NOREF;
        g_Push.m_L = 0;
    }
}

static int Push_Register(lua_State* L)
{
    int top = lua_gettop(L);
    VerifyCallback(L);

    // NOTE: We ignore argument one. Only for iOS
    luaL_checktype(L, 2, LUA_TFUNCTION);
    lua_pushvalue(L, 2);
    g_Push.m_Callback = dmScript::Ref(L, LUA_REGISTRYINDEX);
    dmScript::GetInstance(L);
    g_Push.m_Self = dmScript::Ref(L, LUA_REGISTRYINDEX);
    g_Push.m_L = dmScript::GetMainThread(L);

    JNIEnv* env = Attach();
    env->CallVoidMethod(g_Push.m_Push, g_Push.m_Register, g_AndroidApp->activity->clazz);
    Detach();

    assert(top == lua_gettop(L));
    return 0;
}

static int Push_SetListener(lua_State* L)
{
    Push* push = &g_Push;
    luaL_checktype(L, 1, LUA_TFUNCTION);
    lua_pushvalue(L, 1);
    int cb = dmScript::Ref(L, LUA_REGISTRYINDEX);

    if (push->m_Listener.m_Callback != LUA_NOREF) {
        dmScript::Unref(push->m_Listener.m_L, LUA_REGISTRYINDEX, push->m_Listener.m_Callback);
        dmScript::Unref(push->m_Listener.m_L, LUA_REGISTRYINDEX, push->m_Listener.m_Self);
    }

    push->m_Listener.m_L = dmScript::GetMainThread(L);
    push->m_Listener.m_Callback = cb;

    dmScript::GetInstance(L);
    push->m_Listener.m_Self = dmScript::Ref(L, LUA_REGISTRYINDEX);

    return 0;
}

static int Push_Schedule(lua_State* L)
{
    int top = lua_gettop(L);

    int seconds = luaL_checkinteger(L, 1);
    if (seconds < 0)
    {
        lua_pushnil(L);
        lua_pushstring(L, "invalid seconds argument");
        return 2;
    }

    const char* title = luaL_checkstring(L, 2);
    const char* message = luaL_checkstring(L, 3);

    // param: payload
    const char* payload = 0;
    if (top > 3) {
        payload = luaL_checkstring(L, 4);
    }

    // param: notification_settings
    int priority = 2;
    // char* icon = 0;
    // char* sound = 0;
    if (top > 4) {
        luaL_checktype(L, 5, LUA_TTABLE);

        // priority
        lua_pushstring(L, "priority");
        lua_gettable(L, 5);
        if (lua_isnumber(L, -1)) {
            priority = lua_tointeger(L, -1);

            if (priority < -2) {
                priority = -2;
            } else if (priority > 2) {
                priority = 2;
            }
        }
        lua_pop(L, 1);

        /*

        // icon
        There is now way of automatically bundle files inside the .app folder (i.e. skipping
        archiving them inside the .darc), but to have custom notification sounds they need to
        be accessable from the .app folder.

        lua_pushstring(L, "icon");
        lua_gettable(L, 5);
        if (lua_isstring(L, -1)) {
            icon = lua_tostring(L, -1);
        }
        lua_pop(L, 1);

        // sound
        lua_pushstring(L, "sound");
        lua_gettable(L, 5);
        if (lua_isstring(L, -1)) {
            notification.soundName = [NSString stringWithUTF8String:lua_tostring(L, -1)];
        }
        lua_pop(L, 1);
        */
    }

    uint64_t t = dmTime::GetTime();

    ScheduledNotification sn;

    // Use the current time to create a unique identifier. It should be unique between sessions
    sn.id = (int32_t) dmHashBufferNoReverse32(&t, (uint32_t)sizeof(t));
    if (sn.id < 0) {
        sn.id = -sn.id; // JNI doesn't support unsigned int
    }

    sn.timestamp = t + ((uint64_t)seconds) * 1000000L; // in microseconds

    sn.title     = strdup(title);
    sn.message   = strdup(message);
    sn.payload   = strdup(payload);
    sn.priority  = priority;
    if (g_Push.m_ScheduledNotifications.Remaining() == 0) {
        g_Push.m_ScheduledNotifications.SetCapacity(g_Push.m_ScheduledNotifications.Capacity()*2);
    }
    g_Push.m_ScheduledNotifications.Push( sn );

    JNIEnv* env = Attach();
    jstring jtitle   = env->NewStringUTF(sn.title);
    jstring jmessage = env->NewStringUTF(sn.message);
    jstring jpayload = env->NewStringUTF(sn.payload);
    env->CallVoidMethod(g_Push.m_Push, g_Push.m_Schedule, g_AndroidApp->activity->clazz, sn.id, sn.timestamp / 1000, jtitle, jmessage, jpayload, sn.priority);
    env->DeleteLocalRef(jpayload);
    env->DeleteLocalRef(jmessage);
    env->DeleteLocalRef(jtitle);
    Detach();

    assert(top == lua_gettop(L));

    lua_pushnumber(L, sn.id);
    return 1;

}

static void RemoveNotification(int id)
{
    for (unsigned int i = 0; i < g_Push.m_ScheduledNotifications.Size(); ++i)
    {
        ScheduledNotification sn = g_Push.m_ScheduledNotifications[i];

        if (sn.id == id)
        {
            if (sn.title) {
                free(sn.title);
            }

            if (sn.message) {
                free(sn.message);
            }

            if (sn.payload) {
                free(sn.payload);
            }

            g_Push.m_ScheduledNotifications.EraseSwap(i);
            break;
        }
    }
}

static int Push_Cancel(lua_State* L)
{
    int cancel_id = luaL_checkinteger(L, 1);

    for (unsigned int i = 0; i < g_Push.m_ScheduledNotifications.Size(); ++i)
    {
        ScheduledNotification sn = g_Push.m_ScheduledNotifications[i];

        if (sn.id == cancel_id)
        {
            JNIEnv* env = Attach();
            jstring jtitle   = env->NewStringUTF(sn.title);
            jstring jmessage = env->NewStringUTF(sn.message);
            jstring jpayload = env->NewStringUTF(sn.payload);
            env->CallVoidMethod(g_Push.m_Push, g_Push.m_Cancel, g_AndroidApp->activity->clazz, sn.id, jtitle, jmessage, jpayload, sn.priority);
            env->DeleteLocalRef(jpayload);
            env->DeleteLocalRef(jmessage);
            env->DeleteLocalRef(jtitle);
            Detach();

            RemoveNotification(cancel_id);
            break;
        }
    }

    return 0;
}

static void NotificationToLua(lua_State* L, ScheduledNotification notification)
{
    lua_createtable(L, 0, 5);

    lua_pushstring(L, "seconds");
    lua_pushnumber(L, (notification.timestamp - dmTime::GetTime()) / 1000000.0);
    lua_settable(L, -3);

    lua_pushstring(L, "title");
    lua_pushstring(L, notification.title);
    lua_settable(L, -3);

    lua_pushstring(L, "message");
    lua_pushstring(L, notification.message);
    lua_settable(L, -3);

    lua_pushstring(L, "payload");
    lua_pushstring(L, notification.payload);
    lua_settable(L, -3);

    lua_pushstring(L, "priority");
    lua_pushnumber(L, notification.priority);
    lua_settable(L, -3);

}

static int Push_GetScheduled(lua_State* L)
{
    int get_id = luaL_checkinteger(L, 1);
    uint64_t cur_time = dmTime::GetTime();

    for (unsigned int i = 0; i < g_Push.m_ScheduledNotifications.Size(); ++i)
    {
        ScheduledNotification sn = g_Push.m_ScheduledNotifications[i];

        // filter out and remove notifications that have elapsed
        if (sn.timestamp <= cur_time)
        {
            RemoveNotification(sn.id);
            i--;
            continue;
        }

        if (sn.id == get_id)
        {
            NotificationToLua(L, sn);
            return 1;
        }
    }
    return 0;
}

static int Push_GetAllScheduled(lua_State* L)
{
    uint64_t cur_time = dmTime::GetTime();

    lua_createtable(L, 0, 0);
    for (unsigned int i = 0; i < g_Push.m_ScheduledNotifications.Size(); ++i)
    {
        ScheduledNotification sn = g_Push.m_ScheduledNotifications[i];

        // filter out and remove notifications that have elapsed
        if (sn.timestamp <= cur_time)
        {
            RemoveNotification(sn.id);
            i--;
            continue;
        }

        lua_pushnumber(L, sn.id);
        NotificationToLua(L, sn);
        lua_settable(L, -3);
    }

    return 1;
}

static const luaL_reg Push_methods[] =
{
    {"register", Push_Register},
    {"set_listener", Push_SetListener},

    {"schedule", Push_Schedule},
    {"cancel", Push_Cancel},
    {"get_scheduled", Push_GetScheduled},
    {"get_all_scheduled", Push_GetAllScheduled},

    {0, 0}
};


#ifdef __cplusplus
extern "C" {
#endif

static void PushError(lua_State*L, const char* error)
{
    // Could be extended with error codes etc
    if (error != 0) {
        lua_newtable(L);
        lua_pushstring(L, "error");
        lua_pushstring(L, error);
        lua_rawset(L, -3);
    } else {
        lua_pushnil(L);
    }
}

JNIEXPORT void JNICALL Java_com_defold_push_PushJNI_addPendingNotifications(JNIEnv* env, jobject, jint uid, jstring title, jstring message, jstring payload, jlong timestampMillis, jint priority)
{
    uint64_t cur_time = dmTime::GetTime();
    uint64_t timestamp = 1000 * (uint64_t)timestampMillis;
    if (timestamp <= cur_time) {
        return;
    }

    const char* c_title = "";
    const char* c_message = "";
    const char* c_payload = "";
    if (title) {
        c_title = env->GetStringUTFChars(title, 0);
    }
    if (message) {
        c_message = env->GetStringUTFChars(message, 0);
    }
    if (payload) {
        c_payload = env->GetStringUTFChars(payload, 0);
    }

    ScheduledNotification sn;
    sn.id        = (uint64_t)uid;
    sn.timestamp = timestamp;
    sn.title     = strdup(c_title);
    sn.message   = strdup(c_message);
    sn.payload   = strdup(c_payload);
    sn.priority  = (int)priority;

    if (g_Push.m_ScheduledNotifications.Remaining() == 0) {
        g_Push.m_ScheduledNotifications.SetCapacity(g_Push.m_ScheduledNotifications.Capacity()*2);
    }
    g_Push.m_ScheduledNotifications.Push( sn );


    if (c_title) {
        env->ReleaseStringUTFChars(title, c_title);
    }
    if (c_message) {
        env->ReleaseStringUTFChars(message, c_message);
    }
    if (c_payload) {
        env->ReleaseStringUTFChars(payload, c_payload);
    }
}

JNIEXPORT void JNICALL Java_com_defold_push_PushJNI_onRegistration(JNIEnv* env, jobject, jstring regId, jstring errorMessage)
{
    const char* ri = 0;
    const char* em = 0;

    if (regId)
    {
        ri = env->GetStringUTFChars(regId, 0);
    }
    if (errorMessage)
    {
        em = env->GetStringUTFChars(errorMessage, 0);
    }

    Command cmd;
    cmd.m_Command = CMD_REGISTRATION_RESULT;
    if (ri) {
        cmd.m_Data1 = strdup(ri);
        env->ReleaseStringUTFChars(regId, ri);
    }
    if (em) {
        cmd.m_Data2 = strdup(em);
        env->ReleaseStringUTFChars(errorMessage, em);
    }
    if (write(g_Push.m_Pipefd[1], &cmd, sizeof(cmd)) != sizeof(cmd)) {
        dmLogFatal("Failed to write command");
    }
}


JNIEXPORT void JNICALL Java_com_defold_push_PushJNI_onMessage(JNIEnv* env, jobject, jstring json, bool wasActivated)
{
    const char* j = 0;

    if (json)
    {
        j = env->GetStringUTFChars(json, 0);
    }

    Command cmd;
    cmd.m_Command = CMD_PUSH_MESSAGE_RESULT;
    cmd.m_Data1 = strdup(j);
    cmd.m_WasActivated = wasActivated;
    if (write(g_Push.m_Pipefd[1], &cmd, sizeof(cmd)) != sizeof(cmd)) {
        dmLogFatal("Failed to write command");
    }
    if (j)
    {
        env->ReleaseStringUTFChars(json, j);
    }
}

JNIEXPORT void JNICALL Java_com_defold_push_PushJNI_onLocalMessage(JNIEnv* env, jobject, jstring json, int id, bool wasActivated)
{
    const char* j = 0;

    if (json)
    {
        j = env->GetStringUTFChars(json, 0);
    }

    // keeping track of local notifications, need to remove from internal list
    RemoveNotification(id);

    Command cmd;
    cmd.m_Command = CMD_LOCAL_MESSAGE_RESULT;
    cmd.m_Data1 = strdup(j);
    cmd.m_WasActivated = wasActivated;
    if (write(g_Push.m_Pipefd[1], &cmd, sizeof(cmd)) != sizeof(cmd)) {
        dmLogFatal("Failed to write command");
    }
    if (j)
    {
        env->ReleaseStringUTFChars(json, j);
    }
}

#ifdef __cplusplus
}
#endif

void HandleRegistrationResult(const Command* cmd)
{
    if (g_Push.m_Callback == LUA_NOREF) {
        dmLogError("No callback set");
        return;
    }

    lua_State* L = g_Push.m_L;
    int top = lua_gettop(L);

    lua_rawgeti(L, LUA_REGISTRYINDEX, g_Push.m_Callback);

    // Setup self
    lua_rawgeti(L, LUA_REGISTRYINDEX, g_Push.m_Self);
    lua_pushvalue(L, -1);
    dmScript::SetInstance(L);

    if (!dmScript::IsInstanceValid(L))
    {
        dmLogError("Could not run push callback because the instance has been deleted.");
        lua_pop(L, 2);
        assert(top == lua_gettop(L));
        return;
    }

    if (cmd->m_Data1) {
        lua_pushstring(L, (const char*) cmd->m_Data1);
        lua_pushnil(L);
    } else {
        lua_pushnil(L);
        PushError(L, (const char*) cmd->m_Data2);
        dmLogError("GCM error %s", (const char*) cmd->m_Data2);
    }

    dmScript::PCall(L, 3, 0);

    dmScript::Unref(L, LUA_REGISTRYINDEX, g_Push.m_Callback);
    dmScript::Unref(L, LUA_REGISTRYINDEX, g_Push.m_Self);
    g_Push.m_Callback = LUA_NOREF;
    g_Push.m_Self = LUA_NOREF;

    assert(top == lua_gettop(L));
}

void HandlePushMessageResult(const Command* cmd, bool local)
{
    if (g_Push.m_Listener.m_Callback == LUA_NOREF) {
        dmLogError("No callback set");
        return;
    }

    lua_State* L = g_Push.m_Listener.m_L;
    int top = lua_gettop(L);

    lua_rawgeti(L, LUA_REGISTRYINDEX, g_Push.m_Listener.m_Callback);

    // Setup self
    lua_rawgeti(L, LUA_REGISTRYINDEX, g_Push.m_Listener.m_Self);
    lua_pushvalue(L, -1);
    dmScript::SetInstance(L);

    if (!dmScript::IsInstanceValid(L))
    {
        dmLogError("Could not run push callback because the instance has been deleted.");
        lua_pop(L, 2);
        assert(top == lua_gettop(L));
        return;
    }



    dmJson::Document doc;
    dmJson::Result r = dmJson::Parse((const char*) cmd->m_Data1, &doc);
    if (r == dmJson::RESULT_OK && doc.m_NodeCount > 0) {
        dmScript::JsonToLua(L, &doc, 0);

        if (local) {
            lua_pushnumber(L, DM_PUSH_EXTENSION_ORIGIN_LOCAL);
        } else {
            lua_pushnumber(L, DM_PUSH_EXTENSION_ORIGIN_REMOTE);
        }

        lua_pushboolean(L, cmd->m_WasActivated);

        dmScript::PCall(L, 4, 0);
    } else {
        dmLogError("Failed to parse push response (%d)", r);
    }
    dmJson::Free(&doc);

    assert(top == lua_gettop(L));
}

static int LooperCallback(int fd, int events, void* data)
{
    Push* fb = (Push*)data;
    (void)fb;
    Command cmd;
    if (read(g_Push.m_Pipefd[0], &cmd, sizeof(cmd)) == sizeof(cmd)) {
        switch (cmd.m_Command)
        {
        case CMD_REGISTRATION_RESULT:
            HandleRegistrationResult(&cmd);
            break;
        case CMD_PUSH_MESSAGE_RESULT:
            HandlePushMessageResult(&cmd, false);
            break;
        case CMD_LOCAL_MESSAGE_RESULT:
            HandlePushMessageResult(&cmd, true);
            break;

        default:
            assert(false);
        }

        if (cmd.m_Data1) {
            free(cmd.m_Data1);
        }
        if (cmd.m_Data2) {
            free(cmd.m_Data2);
        }
    }
    else {
        dmLogFatal("read error in looper callback");
    }
    return 1;
}

static dmExtension::Result AppInitializePush(dmExtension::AppParams* params)
{
    int result = pipe(g_Push.m_Pipefd);
    if (result != 0) {
        dmLogFatal("Could not open pipe for communication: %d", result);
        return dmExtension::RESULT_INIT_ERROR;
    }

    result = ALooper_addFd(g_AndroidApp->looper, g_Push.m_Pipefd[0], ALOOPER_POLL_CALLBACK, ALOOPER_EVENT_INPUT, LooperCallback, &g_Push);
    if (result != 1) {
        dmLogFatal("Could not add file descriptor to looper: %d", result);
        close(g_Push.m_Pipefd[0]);
        close(g_Push.m_Pipefd[1]);
        return dmExtension::RESULT_INIT_ERROR;
    }

    JNIEnv* env = Attach();

    jclass activity_class = env->FindClass("android/app/NativeActivity");
    jmethodID get_class_loader = env->GetMethodID(activity_class,"getClassLoader", "()Ljava/lang/ClassLoader;");
    jobject cls = env->CallObjectMethod(g_AndroidApp->activity->clazz, get_class_loader);
    jclass class_loader = env->FindClass("java/lang/ClassLoader");
    jmethodID find_class = env->GetMethodID(class_loader, "loadClass", "(Ljava/lang/String;)Ljava/lang/Class;");

    jstring str_class_name = env->NewStringUTF("com.defold.push.Push");
    jclass push_class = (jclass)env->CallObjectMethod(cls, find_class, str_class_name);
    env->DeleteLocalRef(str_class_name);

    str_class_name = env->NewStringUTF("com.defold.push.PushJNI");
    jclass push_jni_class = (jclass)env->CallObjectMethod(cls, find_class, str_class_name);
    env->DeleteLocalRef(str_class_name);

    g_Push.m_Start = env->GetMethodID(push_class, "start", "(Landroid/app/Activity;Lcom/defold/push/IPushListener;Ljava/lang/String;Ljava/lang/String;)V");
    g_Push.m_Stop = env->GetMethodID(push_class, "stop", "()V");
    g_Push.m_Register = env->GetMethodID(push_class, "register", "(Landroid/app/Activity;)V");
    g_Push.m_Schedule = env->GetMethodID(push_class, "scheduleNotification", "(Landroid/app/Activity;IJLjava/lang/String;Ljava/lang/String;Ljava/lang/String;I)V");
    g_Push.m_Cancel = env->GetMethodID(push_class, "cancelNotification", "(Landroid/app/Activity;ILjava/lang/String;Ljava/lang/String;Ljava/lang/String;I)V");

    jmethodID get_instance_method = env->GetStaticMethodID(push_class, "getInstance", "()Lcom/defold/push/Push;");
    g_Push.m_Push = env->NewGlobalRef(env->CallStaticObjectMethod(push_class, get_instance_method));

    jmethodID jni_constructor = env->GetMethodID(push_jni_class, "<init>", "()V");
    g_Push.m_PushJNI = env->NewGlobalRef(env->NewObject(push_jni_class, jni_constructor));

    const char* sender_id = dmConfigFile::GetString(params->m_ConfigFile, "android.gcm_sender_id", "");
    const char* project_title = dmConfigFile::GetString(params->m_ConfigFile, "project.title", "");
    jstring sender_id_string = env->NewStringUTF(sender_id);
    jstring project_title_string = env->NewStringUTF(project_title);
    env->CallVoidMethod(g_Push.m_Push, g_Push.m_Start, g_AndroidApp->activity->clazz, g_Push.m_PushJNI, sender_id_string, project_title_string);
    env->DeleteLocalRef(sender_id_string);
    env->DeleteLocalRef(project_title_string);

    // loop through all stored local push notifications
    jmethodID loadPendingNotifications = env->GetMethodID(push_class, "loadPendingNotifications", "(Landroid/app/Activity;)V");
    env->CallVoidMethod(g_Push.m_Push, loadPendingNotifications, g_AndroidApp->activity->clazz);

    Detach();

    return dmExtension::RESULT_OK;
}

static dmExtension::Result AppFinalizePush(dmExtension::AppParams* params)
{
    JNIEnv* env = Attach();
    env->CallVoidMethod(g_Push.m_Push, g_Push.m_Stop);
    env->DeleteGlobalRef(g_Push.m_Push);
    env->DeleteGlobalRef(g_Push.m_PushJNI);
    Detach();
    g_Push.m_Push = NULL;
    g_Push.m_PushJNI = NULL;
    g_Push.m_L = 0;
    g_Push.m_Callback = LUA_NOREF;
    g_Push.m_Self = LUA_NOREF;

    int result = ALooper_removeFd(g_AndroidApp->looper, g_Push.m_Pipefd[0]);
    if (result != 1) {
        dmLogFatal("Could not remove fd from looper: %d", result);
    }

    close(g_Push.m_Pipefd[0]);
    env = Attach();
    close(g_Push.m_Pipefd[1]);
    Detach();

    return dmExtension::RESULT_OK;
}

static dmExtension::Result InitializePush(dmExtension::Params* params)
{
    lua_State*L = params->m_L;
    int top = lua_gettop(L);
    luaL_register(L, LIB_NAME, Push_methods);

#define SETCONSTANT(name, val) \
        lua_pushnumber(L, (lua_Number) val); \
        lua_setfield(L, -2, #name);\

    // Values from http://developer.android.com/reference/android/support/v4/app/NotificationCompat.html#PRIORITY_DEFAULT
    SETCONSTANT(PRIORITY_MIN,     -2);
    SETCONSTANT(PRIORITY_LOW,     -1);
    SETCONSTANT(PRIORITY_DEFAULT,  0);
    SETCONSTANT(PRIORITY_HIGH,     1);
    SETCONSTANT(PRIORITY_MAX,      2);

    SETCONSTANT(ORIGIN_REMOTE, DM_PUSH_EXTENSION_ORIGIN_REMOTE);
    SETCONSTANT(ORIGIN_LOCAL,  DM_PUSH_EXTENSION_ORIGIN_LOCAL);

#undef SETCONSTANT

    lua_pop(L, 1);
    assert(top == lua_gettop(L));
    return dmExtension::RESULT_OK;
}

static dmExtension::Result FinalizePush(dmExtension::Params* params)
{
    if (params->m_L == g_Push.m_Listener.m_L && g_Push.m_Listener.m_Callback != LUA_NOREF) {
        dmScript::Unref(g_Push.m_Listener.m_L, LUA_REGISTRYINDEX, g_Push.m_Listener.m_Callback);
        dmScript::Unref(g_Push.m_Listener.m_L, LUA_REGISTRYINDEX, g_Push.m_Listener.m_Self);
        g_Push.m_Listener.m_L = 0;
        g_Push.m_Listener.m_Callback = LUA_NOREF;
        g_Push.m_Listener.m_Self = LUA_NOREF;
    }

    return dmExtension::RESULT_OK;
}

DM_DECLARE_EXTENSION(PushExt, "Push", AppInitializePush, AppFinalizePush, InitializePush, 0, 0, FinalizePush)
