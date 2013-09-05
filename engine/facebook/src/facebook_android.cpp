#include <jni.h>

#include <assert.h>

#include <extension/extension.h>
#include <dlib/dstrings.h>
#include <dlib/log.h>
#include <script/script.h>

#include <pthread.h>
#include <unistd.h>

#include <android_native_app_glue.h>

#define LIB_NAME "facebook"

extern struct android_app* g_AndroidApp;

#define CMD_LOGIN 1
#define CMD_REQUEST_READ 2
#define CMD_REQUEST_PUBLISH 3

// Must match iOS for now
enum State
{
    STATE_CREATED = 0,
    STATE_CREATED_TOKEN_LOADED = 1,
    STATE_CREATED_OPENING = 2,
    STATE_OPEN = 1 | (1 << 9),
    STATE_OPEN_TOKEN_EXTENDED = 2 | (1 << 9),
    STATE_CLOSED_LOGIN_FAILED = 1 | (1 << 8),
    STATE_CLOSED = 2 | (1 << 8)
};

// Must match iOS for now
enum Audience
{
    AUDIENCE_NONE = 0,
    AUDIENCE_ONLYME = 10,
    AUDIENCE_FRIENDS = 20,
    AUDIENCE_EVERYONE = 30
};

struct CBData
{
    lua_State* m_L;
    int m_State;
    const char* m_Error;
};

struct Facebook
{
    Facebook()
    {
        memset(this, 0, sizeof(*this));
        m_Callback = LUA_NOREF;
        m_Self = LUA_NOREF;
    }

    jobject m_FB;
    jmethodID m_Login;
    jmethodID m_Logout;
    jmethodID m_IterateMe;
    jmethodID m_IteratePermissions;
    jmethodID m_GetAccessToken;
    jmethodID m_RequestReadPermissions;
    jmethodID m_RequestPublishPermissions;
    int m_Callback;
    int m_Self;
    int m_Pipefd[2];
    CBData m_CBData;
};

Facebook g_Facebook;

static void PushError(lua_State*L, const char* error)
{
    // Could be extended with error codes etc
    if (error != NULL) {
        lua_newtable(L);
        lua_pushstring(L, "error");
        lua_pushstring(L, error);
        lua_rawset(L, -3);
    } else {
        lua_pushnil(L);
    }
}

static void RunStateCallback()
{
    if (g_Facebook.m_Callback != LUA_NOREF) {
        lua_State* L = g_Facebook.m_CBData.m_L;

        int state = g_Facebook.m_CBData.m_State;
        const char* error = g_Facebook.m_CBData.m_Error;

        int top = lua_gettop(L);

        int callback = g_Facebook.m_Callback;
        g_Facebook.m_Callback = LUA_NOREF;
        lua_rawgeti(L, LUA_REGISTRYINDEX, callback);

        // Setup self
        lua_rawgeti(L, LUA_REGISTRYINDEX, g_Facebook.m_Self);
        lua_pushvalue(L, -1);
        dmScript::SetInstance(L);

        if (!dmScript::IsInstanceValid(L))
        {
            dmLogError("Could not run facebook callback because the instance has been deleted.");
            lua_pop(L, 2);
            assert(top == lua_gettop(L));
            return;
        }

        lua_pushnumber(L, (lua_Number) state);
        PushError(L, error);

        int ret = lua_pcall(L, 3, LUA_MULTRET, 0);
        if (ret != 0) {
            dmLogError("Error running facebook callback: %s", lua_tostring(L,-1));
            lua_pop(L, 1);
        }
        assert(top == lua_gettop(L));
        luaL_unref(L, LUA_REGISTRYINDEX, callback);
    } else {
        dmLogError("No callback set");
    }
}

static void RunCallback()
{
    if (g_Facebook.m_Callback != LUA_NOREF) {
        lua_State* L = g_Facebook.m_CBData.m_L;
        const char* error = g_Facebook.m_CBData.m_Error;

        int top = lua_gettop(L);

        int callback = g_Facebook.m_Callback;
        lua_rawgeti(L, LUA_REGISTRYINDEX, callback);

        // Setup self
        lua_rawgeti(L, LUA_REGISTRYINDEX, g_Facebook.m_Self);
        lua_pushvalue(L, -1);
        dmScript::SetInstance(L);

        if (!dmScript::IsInstanceValid(L))
        {
            dmLogError("Could not run facebook callback because the instance has been deleted.");
            lua_pop(L, 2);
            assert(top == lua_gettop(L));
            return;
        }

        lua_pushnil(L);
        PushError(L, error);

        int ret = lua_pcall(L, 2, LUA_MULTRET, 0);
        if (ret != 0) {
            dmLogError("Error running facebook callback: %s", lua_tostring(L,-1));
            lua_pop(L, 1);
        }
        assert(top == lua_gettop(L));
        luaL_unref(L, LUA_REGISTRYINDEX, callback);
    } else {
        dmLogError("No callback set");
    }
}

static int LooperCallback(int fd, int events, void* data)
{
    Facebook* fb = (Facebook*)data;
    (void)fb;
    int8_t cmd;
    if (read(g_Facebook.m_Pipefd[0], &cmd, sizeof(cmd)) == sizeof(cmd))
    {
        switch (cmd)
        {
        case CMD_LOGIN:
            RunStateCallback();
            break;
        case CMD_REQUEST_READ:
        case CMD_REQUEST_PUBLISH:
            RunCallback();
            break;
        }
        if (fb->m_CBData.m_Error != 0x0)
        {
            free((void*)fb->m_CBData.m_Error);
            fb->m_CBData.m_Error = 0x0;
        }
    }
    else
    {
        dmLogFatal("read error in looper callback");
    }
    return 1;
}

void PostToCallback(int8_t cmd)
{
    if (write(g_Facebook.m_Pipefd[1], &cmd, sizeof(cmd)) != sizeof(cmd))
    {
        dmLogError("Failed to write command");
    }
}

const char* StrDup(JNIEnv* env, jstring s)
{
    if (s != NULL)
    {
        const char* str = env->GetStringUTFChars(s, 0);
        const char* dup = strdup(str);
        env->ReleaseStringUTFChars(s, str);
        return dup;
    }
    else
    {
        return 0x0;
    }
}

#ifdef __cplusplus
extern "C" {
#endif

JNIEXPORT void JNICALL Java_com_dynamo_android_facebook_FacebookJNI_onLogin
  (JNIEnv* env, jobject, jlong userData, jint state, jstring error)
{
    g_Facebook.m_CBData.m_State = (int)state;
    g_Facebook.m_CBData.m_L = (lua_State*)userData;
    g_Facebook.m_CBData.m_Error = StrDup(env, error);
    PostToCallback(CMD_LOGIN);
}

JNIEXPORT void JNICALL Java_com_dynamo_android_facebook_FacebookJNI_onRequestRead
  (JNIEnv* env, jobject, jlong userData, jstring error)
{
    g_Facebook.m_CBData.m_L = (lua_State*)userData;
    g_Facebook.m_CBData.m_Error = StrDup(env, error);
    PostToCallback(CMD_REQUEST_READ);
}

JNIEXPORT void JNICALL Java_com_dynamo_android_facebook_FacebookJNI_onRequestPublish
  (JNIEnv* env, jobject, jlong userData, jstring error)
{
    g_Facebook.m_CBData.m_L = (lua_State*)userData;
    g_Facebook.m_CBData.m_Error = StrDup(env, error);
    PostToCallback(CMD_REQUEST_PUBLISH);
}

JNIEXPORT void JNICALL Java_com_dynamo_android_facebook_FacebookJNI_onIterateMeEntry
  (JNIEnv* env, jobject, jlong user_data, jstring key, jstring value)
{
    lua_State* L = (lua_State*)user_data;

    const char* str_key = env->GetStringUTFChars(key, 0);
    lua_pushstring(L, str_key);
    env->ReleaseStringUTFChars(key, str_key);
    const char* str_value = env->GetStringUTFChars(value, 0);
    lua_pushstring(L, str_value);
    env->ReleaseStringUTFChars(value, str_value);
    lua_rawset(L, -3);
}

JNIEXPORT void JNICALL Java_com_dynamo_android_facebook_FacebookJNI_onIteratePermissionsEntry
  (JNIEnv* env, jobject, jlong user_data, jstring permission)
{
    lua_State* L = (lua_State*)user_data;
    int i = lua_objlen(L, -1);

    lua_pushnumber(L, i + 1);
    const char* str_permission = env->GetStringUTFChars(permission, 0);
    lua_pushstring(L, str_permission);
    env->ReleaseStringUTFChars(permission, str_permission);
    lua_rawset(L, -3);
}

#ifdef __cplusplus
}
#endif

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

static void VerifyCallback(lua_State* L)
{
    if (g_Facebook.m_Callback != LUA_NOREF) {
        dmLogError("Unexpected callback set");
        luaL_unref(L, LUA_REGISTRYINDEX, g_Facebook.m_Callback);
        luaL_unref(L, LUA_REGISTRYINDEX, g_Facebook.m_Self);
        g_Facebook.m_Callback = LUA_NOREF;
        g_Facebook.m_Self = LUA_NOREF;
    }
}

int Facebook_Login(lua_State* L)
{
    int top = lua_gettop(L);
    VerifyCallback(L);

    luaL_checktype(L, 1, LUA_TFUNCTION);
    lua_pushvalue(L, 1);
    g_Facebook.m_Callback = luaL_ref(L, LUA_REGISTRYINDEX);

    dmScript::GetInstance(L);
    g_Facebook.m_Self = luaL_ref(L, LUA_REGISTRYINDEX);

    JNIEnv* env = Attach();

    env->CallVoidMethod(g_Facebook.m_FB, g_Facebook.m_Login, (jlong)L);

    Detach();

    assert(top == lua_gettop(L));
    return 0;
}

int Facebook_Logout(lua_State* L)
{
    int top = lua_gettop(L);
    VerifyCallback(L);

    JNIEnv* env = Attach();

    env->CallVoidMethod(g_Facebook.m_FB, g_Facebook.m_Logout);

    Detach();

    assert(top == lua_gettop(L));
    return 0;
}

void AppendArray(lua_State* L, char* buffer, uint32_t buffer_size, int idx)
{
    lua_pushnil(L);
    *buffer = 0;
    while (lua_next(L, idx) != 0)
    {
        if (!lua_isstring(L, -1))
            luaL_error(L, "permissions can only be strings (not %s)", lua_typename(L, lua_type(L, -1)));
        if (*buffer != 0)
            dmStrlCat(buffer, ",", buffer_size);
        const char* permission = lua_tostring(L, -1);
        dmStrlCat(buffer, permission, buffer_size);
        lua_pop(L, 1);
    }
}

int Facebook_RequestReadPermissions(lua_State* L)
{
    int top = lua_gettop(L);
    VerifyCallback(L);

    luaL_checktype(L, top-1, LUA_TTABLE);
    luaL_checktype(L, top, LUA_TFUNCTION);
    lua_pushvalue(L, top);
    g_Facebook.m_Callback = luaL_ref(L, LUA_REGISTRYINDEX);

    dmScript::GetInstance(L);
    g_Facebook.m_Self = luaL_ref(L, LUA_REGISTRYINDEX);

    char permissions[512];
    AppendArray(L, permissions, 512, top-1);

    JNIEnv* env = Attach();

    jstring str_permissions = env->NewStringUTF(permissions);
    env->CallVoidMethod(g_Facebook.m_FB, g_Facebook.m_RequestReadPermissions, (jlong)L, str_permissions);
    env->DeleteLocalRef(str_permissions);

    Detach();

    assert(top == lua_gettop(L));
    return 0;
}

int Facebook_RequestPublishPermissions(lua_State* L)
{
    int top = lua_gettop(L);
    VerifyCallback(L);

    luaL_checktype(L, top-2, LUA_TTABLE);
    int audience = luaL_checkinteger(L, top-1);
    luaL_checktype(L, top, LUA_TFUNCTION);
    lua_pushvalue(L, top);
    g_Facebook.m_Callback = luaL_ref(L, LUA_REGISTRYINDEX);

    dmScript::GetInstance(L);
    g_Facebook.m_Self = luaL_ref(L, LUA_REGISTRYINDEX);

    char permissions[512];
    AppendArray(L, permissions, 512, top-2);

    JNIEnv* env = Attach();

    jstring str_permissions = env->NewStringUTF(permissions);
    env->CallVoidMethod(g_Facebook.m_FB, g_Facebook.m_RequestReadPermissions, (jlong)L, (jint)audience, str_permissions);
    env->DeleteLocalRef(str_permissions);

    Detach();

    assert(top == lua_gettop(L));
    return 0;
}

int Facebook_AccessToken(lua_State* L)
{
    int top = lua_gettop(L);

    JNIEnv* env = Attach();

    jstring str_access_token = (jstring)env->CallObjectMethod(g_Facebook.m_FB, g_Facebook.m_GetAccessToken);
    const char* access_token = env->GetStringUTFChars(str_access_token, 0);
    if (access_token != 0x0)
    {
        lua_pushstring(L, access_token);
    }
    else
    {
        lua_pushnil(L);
    }
    env->ReleaseStringUTFChars(str_access_token, access_token);
    Detach();
    assert(top + 1 == lua_gettop(L));
    return 1;
}

int Facebook_Permissions(lua_State* L)
{
    int top = lua_gettop(L);

    lua_newtable(L);

    JNIEnv* env = Attach();

    env->CallVoidMethod(g_Facebook.m_FB, g_Facebook.m_IteratePermissions, (jlong)L);

    Detach();

    assert(top + 1 == lua_gettop(L));
    return 1;
}

int Facebook_Me(lua_State* L)
{
    int top = lua_gettop(L);

    lua_newtable(L);

    JNIEnv* env = Attach();

    env->CallVoidMethod(g_Facebook.m_FB, g_Facebook.m_IterateMe, (jlong)L);

    Detach();

    assert(top + 1 == lua_gettop(L));
    return 1;
}

static const luaL_reg Facebook_methods[] =
{
    {"login", Facebook_Login},
    {"logout", Facebook_Logout},
    {"access_token", Facebook_AccessToken},
    {"permissions", Facebook_Permissions},
    {"request_read_permissions", Facebook_RequestReadPermissions},
    {"request_publish_permissions", Facebook_RequestPublishPermissions},
    {"me", Facebook_Me},
    {0, 0}
};

dmExtension::Result InitializeFacebook(dmExtension::Params* params)
{
    if (g_Facebook.m_FB == NULL)
    {
        int result = pipe(g_Facebook.m_Pipefd);
        if (result != 0)
        {
            dmLogFatal("Could not open pipe for communication: %d", result);
        }

        result = ALooper_addFd(g_AndroidApp->looper, g_Facebook.m_Pipefd[0], ALOOPER_POLL_CALLBACK, ALOOPER_EVENT_INPUT, LooperCallback, &g_Facebook);
        if (result != 1)
        {
            dmLogFatal("Could not add file descriptor to looper: %d", result);
        }

        JNIEnv* env = Attach();

        jclass activity_class = env->FindClass("android/app/NativeActivity");
        jmethodID get_class_loader = env->GetMethodID(activity_class,"getClassLoader", "()Ljava/lang/ClassLoader;");
        jobject cls = env->CallObjectMethod(g_AndroidApp->activity->clazz, get_class_loader);
        jclass class_loader = env->FindClass("java/lang/ClassLoader");
        jmethodID find_class = env->GetMethodID(class_loader, "loadClass", "(Ljava/lang/String;)Ljava/lang/Class;");
        jstring str_class_name = env->NewStringUTF("com.dynamo.android.facebook.FacebookJNI");
        jclass fb_class = (jclass)env->CallObjectMethod(cls, find_class, str_class_name);
        env->DeleteLocalRef(str_class_name);

        g_Facebook.m_Login = env->GetMethodID(fb_class, "login", "(J)V");
        g_Facebook.m_Logout = env->GetMethodID(fb_class, "logout", "()V");
        g_Facebook.m_IterateMe = env->GetMethodID(fb_class, "iterateMe", "(J)V");
        g_Facebook.m_IteratePermissions = env->GetMethodID(fb_class, "iteratePermissions", "(J)V");
        g_Facebook.m_GetAccessToken = env->GetMethodID(fb_class, "getAccessToken", "()Ljava/lang/String;");
        g_Facebook.m_RequestReadPermissions = env->GetMethodID(fb_class, "requestReadPermissions", "(JLjava/lang/String;)V");
        g_Facebook.m_RequestPublishPermissions = env->GetMethodID(fb_class, "requestPublishPermissions", "(JILjava/lang/String;)V");

        // 355198514515820 is HelloFBSample. Default value in order to avoid exceptions
        // Better solution?
        const char* app_id = dmConfigFile::GetString(params->m_ConfigFile, "facebook.appid", "355198514515820");

        jmethodID jni_constructor = env->GetMethodID(fb_class, "<init>", "(Landroid/app/Activity;Ljava/lang/String;)V");
        jstring str_app_id = env->NewStringUTF(app_id);
        g_Facebook.m_FB = env->NewGlobalRef(env->NewObject(fb_class, jni_constructor, g_AndroidApp->activity->clazz, str_app_id));
        env->DeleteLocalRef(str_app_id);

        Detach();
    }

    lua_State* L = params->m_L;
    int top = lua_gettop(L);
    luaL_register(L, LIB_NAME, Facebook_methods);

#define SETCONSTANT(name) \
        lua_pushnumber(L, (lua_Number) name); \
        lua_setfield(L, -2, #name);\

    SETCONSTANT(STATE_CREATED);
    SETCONSTANT(STATE_CREATED_TOKEN_LOADED);
    SETCONSTANT(STATE_CREATED_OPENING);
    SETCONSTANT(STATE_OPEN);
    SETCONSTANT(STATE_OPEN_TOKEN_EXTENDED);
    SETCONSTANT(STATE_CLOSED_LOGIN_FAILED);
    SETCONSTANT(STATE_CLOSED);
    SETCONSTANT(AUDIENCE_NONE)
    SETCONSTANT(AUDIENCE_ONLYME)
    SETCONSTANT(AUDIENCE_FRIENDS)
    SETCONSTANT(AUDIENCE_EVERYONE)

    lua_pop(L, 1);
    assert(top == lua_gettop(L));

    return dmExtension::RESULT_OK;
}

dmExtension::Result FinalizeFacebook(dmExtension::Params* params)
{
    if (g_Facebook.m_FB != NULL)
    {
        JNIEnv* env = Attach();
        env->DeleteGlobalRef(g_Facebook.m_FB);
        Detach();
        g_Facebook.m_FB = NULL;

        int result = ALooper_removeFd(g_AndroidApp->looper, g_Facebook.m_Pipefd[0]);
        if (result != 1)
        {
            dmLogFatal("Could not remove fd from looper: %d", result);
        }

        close(g_Facebook.m_Pipefd[0]);
        close(g_Facebook.m_Pipefd[1]);

        g_Facebook = Facebook();
    }
    return dmExtension::RESULT_OK;
}

dmExtension::Desc FacebookExtDesc = {
        "Facebook",
        InitializeFacebook,
        FinalizeFacebook,
        0,
};

DM_REGISTER_EXTENSION(FacebookExt, FacebookExtDesc);
