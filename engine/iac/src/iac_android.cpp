#include <jni.h>
#include <stdlib.h>
#include <unistd.h>
#include <dlib/log.h>
#include <dlib/dstrings.h>
#include <script/script.h>
#include <extension/extension.h>
#include <android_native_app_glue.h>
#include "iac.h"

extern struct android_app* g_AndroidApp;

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

struct IACInvocaction
{
    IACInvocaction()
    {
        memset(this, 0x0, sizeof(IACInvocaction));
    }

    bool Get(const char** payload, const char** origin)
    {
        if(!m_Pending)
            return false;
        m_Pending = false;
        *payload = m_Payload;
        *origin = m_Origin;
        return true;
    }

    void Store(JNIEnv* env, jstring payload, jstring origin)
    {
        Release(env);
        if(payload)
        {
            m_JPayload = payload;
            m_Payload = env->GetStringUTFChars(payload, 0);
            m_Pending = true;
        }
        if(origin)
        {
            m_JOrigin = origin;
            m_Origin = env->GetStringUTFChars(origin, 0);
            m_Pending = true;
        }
    }

    void Release(JNIEnv* env)
    {
        if(m_JOrigin)
            env->ReleaseStringUTFChars(m_JOrigin, m_Origin);
        if(m_JPayload)
            env->ReleaseStringUTFChars(m_JPayload, m_Payload);
        memset(this, 0x0, sizeof(IACInvocaction));
    }

    jstring     m_JPayload;
    jstring     m_JOrigin;
    const char* m_Payload;
    const char* m_Origin;
    bool        m_Pending;
};

struct IACListener
{
    IACListener()
    {
        m_L = 0;
        m_Callback = LUA_NOREF;
        m_Self = LUA_NOREF;
    }
    lua_State* m_L;
    int        m_Callback;
    int        m_Self;
};

struct IAC
{
    IAC()
    {
        memset(this, 0, sizeof(*this));
        m_Callback = LUA_NOREF;
        m_Self = LUA_NOREF;
        m_Listener.m_Callback = LUA_NOREF;
        m_Listener.m_Self = LUA_NOREF;
    }
    int                  m_Callback;
    int                  m_Self;
    lua_State*           m_L;
    IACListener          m_Listener;

    jobject              m_IAC;
    jobject              m_IACJNI;
    jmethodID            m_Start;
    jmethodID            m_Stop;

    IACInvocaction       m_StoredInvocation;
} g_IAC;


static void OnInvocation(const char* payload, const char *origin)
{
    lua_State* L = g_IAC.m_Listener.m_L;
    int top = lua_gettop(L);
    lua_rawgeti(L, LUA_REGISTRYINDEX, g_IAC.m_Listener.m_Callback);

    // Setup self
    lua_rawgeti(L, LUA_REGISTRYINDEX, g_IAC.m_Listener.m_Self);
    lua_pushvalue(L, -1);
    dmScript::SetInstance(L);

    if (!dmScript::IsInstanceValid(L))
    {
        dmLogError("Could not run iac callback because the instance has been deleted.");
        lua_pop(L, 2);
        assert(top == lua_gettop(L));
        return;
    }

    lua_createtable(L, 0, 2);
    lua_pushstring(L, payload);
    lua_setfield(L, -2, "url");
    lua_pushstring(L, origin);
    lua_setfield(L, -2, "origin");
    lua_pushnumber(L, DM_IAC_EXTENSION_TYPE_INVOCATION);

    int ret = lua_pcall(L, 3, LUA_MULTRET, 0);
    if (ret != 0) {
        dmLogError("Error running iac callback: %s", lua_tostring(L,-1));
        lua_pop(L, 1);
    }
    assert(top == lua_gettop(L));
}


int IAC_PlatformSetListener(lua_State* L)
{
    IAC* iac = &g_IAC;
    luaL_checktype(L, 1, LUA_TFUNCTION);
    lua_pushvalue(L, 1);
    int cb = luaL_ref(L, LUA_REGISTRYINDEX);

    if (iac->m_Listener.m_Callback != LUA_NOREF) {
        luaL_unref(iac->m_Listener.m_L, LUA_REGISTRYINDEX, iac->m_Listener.m_Callback);
        luaL_unref(iac->m_Listener.m_L, LUA_REGISTRYINDEX, iac->m_Listener.m_Self);
    }

    iac->m_Listener.m_L = dmScript::GetMainThread(L);
    iac->m_Listener.m_Callback = cb;
    dmScript::GetInstance(L);
    iac->m_Listener.m_Self = luaL_ref(L, LUA_REGISTRYINDEX);

    // handle stored invocation
    const char* payload, *origin;
    if(iac->m_StoredInvocation.Get(&payload, &origin))
        OnInvocation(payload, origin);

    return 0;
}


#ifdef __cplusplus
extern "C" {
#endif

JNIEXPORT void JNICALL Java_com_defold_iac_IACJNI_onInvocation(JNIEnv* env, jobject, jstring jpayload, jstring jorigin)
{
    if (g_IAC.m_Listener.m_Callback == LUA_NOREF) {
        dmLogError("No callback set");
        g_IAC.m_StoredInvocation.Store(env, jpayload, jorigin);
        return;
    }
    const char* payload = jpayload == 0 ? 0 : env->GetStringUTFChars(jpayload, 0);
    const char* origin = jorigin == 0 ? 0 : env->GetStringUTFChars(jorigin, 0);
    OnInvocation(payload, origin);
    if (jpayload)
        env->ReleaseStringUTFChars(jpayload, payload);
    if (jorigin)
        env->ReleaseStringUTFChars(jorigin, origin);
}

#ifdef __cplusplus
}
#endif


dmExtension::Result AppInitializeIAC(dmExtension::AppParams* params)
{
    JNIEnv* env = Attach();

    jclass activity_class = env->FindClass("android/app/NativeActivity");
    jmethodID get_class_loader = env->GetMethodID(activity_class,"getClassLoader", "()Ljava/lang/ClassLoader;");
    jobject cls = env->CallObjectMethod(g_AndroidApp->activity->clazz, get_class_loader);
    jclass class_loader = env->FindClass("java/lang/ClassLoader");
    jmethodID find_class = env->GetMethodID(class_loader, "loadClass", "(Ljava/lang/String;)Ljava/lang/Class;");

    jstring str_class_name = env->NewStringUTF("com.defold.iac.IAC");
    jclass iac_class = (jclass)env->CallObjectMethod(cls, find_class, str_class_name);
    env->DeleteLocalRef(str_class_name);

    str_class_name = env->NewStringUTF("com.defold.iac.IACJNI");
    jclass iac_jni_class = (jclass)env->CallObjectMethod(cls, find_class, str_class_name);
    env->DeleteLocalRef(str_class_name);

    g_IAC.m_Start = env->GetMethodID(iac_class, "start", "(Landroid/app/Activity;Lcom/defold/iac/IIACListener;)V");
    g_IAC.m_Stop = env->GetMethodID(iac_class, "stop", "()V");
    jmethodID get_instance_method = env->GetStaticMethodID(iac_class, "getInstance", "()Lcom/defold/iac/IAC;");
    g_IAC.m_IAC = env->NewGlobalRef(env->CallStaticObjectMethod(iac_class, get_instance_method));

    jmethodID jni_constructor = env->GetMethodID(iac_jni_class, "<init>", "()V");
    g_IAC.m_IACJNI = env->NewGlobalRef(env->NewObject(iac_jni_class, jni_constructor));

    env->CallVoidMethod(g_IAC.m_IAC, g_IAC.m_Start, g_AndroidApp->activity->clazz, g_IAC.m_IACJNI);

    Detach();
    return dmExtension::RESULT_OK;
}


dmExtension::Result AppFinalizeIAC(dmExtension::AppParams* params)
{
    JNIEnv* env = Attach();
    g_IAC.m_StoredInvocation.Release(env);
    env->CallVoidMethod(g_IAC.m_IAC, g_IAC.m_Stop);
    env->DeleteGlobalRef(g_IAC.m_IAC);
    env->DeleteGlobalRef(g_IAC.m_IACJNI);
    Detach();
    g_IAC.m_IAC = NULL;
    g_IAC.m_IACJNI = NULL;
    g_IAC.m_L = 0;
    g_IAC.m_Callback = LUA_NOREF;
    g_IAC.m_Self = LUA_NOREF;
    return dmExtension::RESULT_OK;
}


dmExtension::Result InitializeIAC(dmExtension::Params* params)
{
    return dmIAC::Initialize(params);
}


dmExtension::Result FinalizeIAC(dmExtension::Params* params)
{
    if (params->m_L == g_IAC.m_Listener.m_L && g_IAC.m_Listener.m_Callback != LUA_NOREF) {
        luaL_unref(g_IAC.m_Listener.m_L, LUA_REGISTRYINDEX, g_IAC.m_Listener.m_Callback);
        luaL_unref(g_IAC.m_Listener.m_L, LUA_REGISTRYINDEX, g_IAC.m_Listener.m_Self);
        g_IAC.m_Listener.m_L = 0;
        g_IAC.m_Listener.m_Callback = LUA_NOREF;
        g_IAC.m_Listener.m_Self = LUA_NOREF;
    }
    return dmIAC::Finalize(params);
}

DM_DECLARE_EXTENSION(IACExt, "IAC", AppInitializeIAC, AppFinalizeIAC, InitializeIAC, 0, 0, FinalizeIAC)

