#include <jni.h>
#include <stdlib.h>
#include <unistd.h>
#include <dlib/array.h>
#include <dlib/log.h>
#include <dlib/dstrings.h>
#include <dlib/json.h>
#include <script/script.h>
#include <extension/extension.h>
#include <android_native_app_glue.h>

#define LIB_NAME "adtruth"

extern struct android_app* g_AndroidApp;

#define CMD_LOAD (0)

struct Command
{
    Command()
    {
        memset(this, 0, sizeof(*this));
    }
    uint32_t m_Command;
    void*    m_Data1;
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

struct AdTruth
{
    AdTruth()
    {
        memset(this, 0, sizeof(*this));
        m_Callback = LUA_NOREF;
        m_Self = LUA_NOREF;
    }

    int                  m_Callback;
    int                  m_Self;
    lua_State*           m_L;

    jobject              m_AdTruthJNI;
    jmethodID            m_Load;
    jmethodID            m_GetReferrer;
    int                  m_Pipefd[2];
};

AdTruth g_AdTruth;

static void VerifyCallback(lua_State* L)
{
    if (g_AdTruth.m_Callback != LUA_NOREF) {
        dmLogError("Unexpected callback set");
        dmScript::Unref(L, LUA_REGISTRYINDEX, g_AdTruth.m_Callback);
        dmScript::Unref(L, LUA_REGISTRYINDEX, g_AdTruth.m_Self);
        g_AdTruth.m_Callback = LUA_NOREF;
        g_AdTruth.m_Self = LUA_NOREF;
        g_AdTruth.m_L = 0;
    }
}

int AdTruth_Load(lua_State* L)
{
    int top = lua_gettop(L);
    VerifyCallback(L);

    const char* url_cstr = luaL_checkstring(L, 1);
    luaL_checktype(L, 2, LUA_TFUNCTION);
    lua_pushvalue(L, 2);
    g_AdTruth.m_Callback = dmScript::Ref(L, LUA_REGISTRYINDEX);

    dmScript::GetInstance(L);
    g_AdTruth.m_Self = dmScript::Ref(L, LUA_REGISTRYINDEX);

    g_AdTruth.m_L = dmScript::GetMainThread(L);

    JNIEnv* env = Attach();
    jstring url = env->NewStringUTF(url_cstr);
    env->CallVoidMethod(g_AdTruth.m_AdTruthJNI, g_AdTruth.m_Load, url);
    env->DeleteLocalRef(url);
    Detach();

    assert(top == lua_gettop(L));
    return 0;
}

int AdTruth_GetReferrer(lua_State* L)
{
    int top = lua_gettop(L);

    const char *referrer = 0;
    JNIEnv* env = Attach();
    jstring referrer_obj = (jstring) env->CallObjectMethod(g_AdTruth.m_AdTruthJNI, g_AdTruth.m_GetReferrer);

    if (referrer_obj) {
        referrer = env->GetStringUTFChars(referrer_obj, 0);
        lua_pushstring(L, referrer);
        env->ReleaseStringUTFChars(referrer_obj, referrer);
    } else {
        lua_pushnil(L);
    }

    Detach();

    assert(top + 1 == lua_gettop(L));
    return 1;
}

static const luaL_reg AdTruth_methods[] =
{
    {"load", AdTruth_Load},
    {"get_referrer", AdTruth_GetReferrer},
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

static void RunLoadCallback(lua_State* L, int callback, int self, const char* error)
{
    if (callback != LUA_NOREF) {
        int top = lua_gettop(L);

        lua_rawgeti(L, LUA_REGISTRYINDEX, callback);
        // Setup self
        lua_rawgeti(L, LUA_REGISTRYINDEX, self);
        lua_pushvalue(L, -1);
        dmScript::SetInstance(L);

        if (!dmScript::IsInstanceValid(L))
        {
            dmLogError("Could not run adtruth callback because the instance has been deleted.");
            lua_pop(L, 2);
            assert(top == lua_gettop(L));
            return;
        }

        PushError(L, error);

        int ret = dmScript::PCall(L, 2, 0);
        assert(top == lua_gettop(L));
    } else {
        dmLogError("No callback set");
    }
}

JNIEXPORT void JNICALL Java_com_defold_adtruth_AdTruthJNI_onPageFinished(JNIEnv* env, jobject)
{
    Command cmd;
    cmd.m_Command = CMD_LOAD;
    if (write(g_AdTruth.m_Pipefd[1], &cmd, sizeof(cmd)) != sizeof(cmd)) {
        dmLogFatal("Failed to write command");
    }
}

JNIEXPORT void JNICALL Java_com_defold_adtruth_AdTruthJNI_onReceivedError__Ljava_lang_String_2(JNIEnv* env, jobject, jstring errorMessage)
{
    if (errorMessage) {
        const char* errorMsg = env->GetStringUTFChars(errorMessage, 0);

        Command cmd;
        cmd.m_Command = CMD_LOAD;
        cmd.m_Data1 = strdup(errorMsg);
        if (write(g_AdTruth.m_Pipefd[1], &cmd, sizeof(cmd)) != sizeof(cmd)) {
            dmLogFatal("Failed to write command");
        }
        env->ReleaseStringUTFChars(errorMessage, errorMsg);
    }
}

#ifdef __cplusplus
}
#endif

static int LooperCallback(int fd, int events, void* data)
{
    AdTruth* ad_truth = (AdTruth*)data;
    (void)ad_truth;
    Command cmd;
    if (read(ad_truth->m_Pipefd[0], &cmd, sizeof(cmd)) == sizeof(cmd)) {
        switch (cmd.m_Command)
        {
        case CMD_LOAD:
            RunLoadCallback(ad_truth->m_L, ad_truth->m_Callback, ad_truth->m_Self, (const char*)cmd.m_Data1);
            break;

        default:
            assert(false);
        }

        if (cmd.m_Data1) {
            free(cmd.m_Data1);
        }
    }
    else {
        dmLogFatal("read error in looper callback");
    }
    return 1;
}

dmExtension::Result AppInitializeAdTruth(dmExtension::AppParams* params)
{
    int result = pipe(g_AdTruth.m_Pipefd);
    if (result != 0) {
        dmLogFatal("Could not open pipe for communication: %d", result);
        return dmExtension::RESULT_INIT_ERROR;
    }

    result = ALooper_addFd(g_AndroidApp->looper, g_AdTruth.m_Pipefd[0], ALOOPER_POLL_CALLBACK, ALOOPER_EVENT_INPUT, LooperCallback, &g_AdTruth);
    if (result != 1) {
        dmLogFatal("Could not add file descriptor to looper: %d", result);
        return dmExtension::RESULT_INIT_ERROR;
    }

    JNIEnv* env = Attach();

    jclass activity_class = env->FindClass("android/app/NativeActivity");
    jmethodID get_class_loader = env->GetMethodID(activity_class,"getClassLoader", "()Ljava/lang/ClassLoader;");
    jobject cls = env->CallObjectMethod(g_AndroidApp->activity->clazz, get_class_loader);
    jclass class_loader = env->FindClass("java/lang/ClassLoader");
    jmethodID find_class = env->GetMethodID(class_loader, "loadClass", "(Ljava/lang/String;)Ljava/lang/Class;");
    jstring str_class_name = env->NewStringUTF("com.defold.adtruth.AdTruthJNI");
    jclass adtruth_class = (jclass)env->CallObjectMethod(cls, find_class, str_class_name);
    env->DeleteLocalRef(str_class_name);

    g_AdTruth.m_Load = env->GetMethodID(adtruth_class, "load", "(Ljava/lang/String;)V");
    g_AdTruth.m_GetReferrer = env->GetMethodID(adtruth_class, "getReferrer", "()Ljava/lang/String;");

    jmethodID jni_constructor = env->GetMethodID(adtruth_class, "<init>", "(Landroid/app/Activity;)V");
    g_AdTruth.m_AdTruthJNI = env->NewGlobalRef(env->NewObject(adtruth_class, jni_constructor, g_AndroidApp->activity->clazz));

    Detach();

    return dmExtension::RESULT_OK;
}

dmExtension::Result InitializeAdTruth(dmExtension::Params* params)
{
    lua_State*L = params->m_L;
    int top = lua_gettop(L);
    luaL_register(L, LIB_NAME, AdTruth_methods);

    lua_pop(L, 1);
    assert(top == lua_gettop(L));

    return dmExtension::RESULT_OK;
}

dmExtension::Result AppFinalizeAdTruth(dmExtension::AppParams* params)
{
    JNIEnv* env = Attach();
    env->DeleteGlobalRef(g_AdTruth.m_AdTruthJNI);
    Detach();
    g_AdTruth.m_AdTruthJNI = NULL;

    int result = ALooper_removeFd(g_AndroidApp->looper, g_AdTruth.m_Pipefd[0]);
    if (result != 1) {
        dmLogFatal("Could not remove fd from looper: %d", result);
    }

    close(g_AdTruth.m_Pipefd[0]);
    close(g_AdTruth.m_Pipefd[1]);

    return dmExtension::RESULT_OK;
}

dmExtension::Result FinalizeAdTruth(dmExtension::Params* params)
{
    return dmExtension::RESULT_OK;
}

DM_DECLARE_EXTENSION(AdTruthExt, "AdTruth", AppInitializeAdTruth, AppFinalizeAdTruth, InitializeAdTruth, 0, 0, FinalizeAdTruth)
