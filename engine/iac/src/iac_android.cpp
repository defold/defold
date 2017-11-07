#include <jni.h>
#include <stdlib.h>
#include <unistd.h>
#include <dlib/log.h>
#include <dlib/mutex.h>
#include <dlib/dstrings.h>
#include <script/script.h>
#include <extension/extension.h>
#include <android_native_app_glue.h>
#include "iac.h"

extern struct android_app* g_AndroidApp;

#define CMD_INVOKE 1

struct Command
{
    Command()
    {
        memset(this, 0, sizeof(Command));
    }
    uint8_t m_Type;
    const char* m_Payload;
    const char* m_Origin;
};

static JNIEnv* Attach()
{
    JNIEnv* env = 0;
    g_AndroidApp->activity->vm->AttachCurrentThread(&env, NULL);
    return env;
}

static void Detach()
{
    g_AndroidApp->activity->vm->DetachCurrentThread();
}

struct IACInvocation
{
    IACInvocation()
    {
        memset(this, 0x0, sizeof(IACInvocation));
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

    void Store(const char* payload, const char* origin)
    {
        Release();
        if(payload)
        {
            m_Payload = strdup(payload);
            m_Pending = true;
        }
        if(origin)
        {
            m_Origin = strdup(origin);
            m_Pending = true;
        }
    }

    void Release()
    {
        if(m_Payload)
            free((void*)m_Payload);
        if(m_Origin)
            free((void*)m_Origin);
        memset(this, 0x0, sizeof(IACInvocation));
    }

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

    IACInvocation        m_StoredInvocation;

    dmMutex::Mutex       m_Mutex;
    dmArray<Command>     m_CmdQueue;
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

    int ret = dmScript::PCall(L, 3, 0);
    if (ret != 0) {
        dmLogError("Error running iac callback");
    }
    assert(top == lua_gettop(L));
}


int IAC_PlatformSetListener(lua_State* L)
{
    IAC* iac = &g_IAC;
    luaL_checktype(L, 1, LUA_TFUNCTION);
    lua_pushvalue(L, 1);
    int cb = dmScript::Ref(L, LUA_REGISTRYINDEX);

    if (iac->m_Listener.m_Callback != LUA_NOREF) {
        dmScript::Unref(iac->m_Listener.m_L, LUA_REGISTRYINDEX, iac->m_Listener.m_Callback);
        dmScript::Unref(iac->m_Listener.m_L, LUA_REGISTRYINDEX, iac->m_Listener.m_Self);
    }

    iac->m_Listener.m_L = dmScript::GetMainThread(L);
    iac->m_Listener.m_Callback = cb;
    dmScript::GetInstance(L);
    iac->m_Listener.m_Self = dmScript::Ref(L, LUA_REGISTRYINDEX);

    // handle stored invocation
    const char* payload, *origin;
    if(iac->m_StoredInvocation.Get(&payload, &origin))
        OnInvocation(payload, origin);

    return 0;
}


static void QueueCommand(Command* cmd)
{
    dmMutex::ScopedLock lk(g_IAC.m_Mutex);
    if (g_IAC.m_CmdQueue.Full())
    {
        g_IAC.m_CmdQueue.OffsetCapacity(8);
    }
    g_IAC.m_CmdQueue.Push(*cmd);
}

static const char* StrDup(JNIEnv* env, jstring s)
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

JNIEXPORT void JNICALL Java_com_defold_iac_IACJNI_onInvocation(JNIEnv* env, jobject, jstring jpayload, jstring jorigin)
{
    Command cmd;
    cmd.m_Type = CMD_INVOKE;
    cmd.m_Payload = StrDup(env, jpayload);
    cmd.m_Origin = StrDup(env, jorigin);
    QueueCommand(&cmd);
}

#ifdef __cplusplus
}
#endif


dmExtension::Result AppInitializeIAC(dmExtension::AppParams* params)
{
    g_IAC.m_Mutex = dmMutex::New();
    g_IAC.m_CmdQueue.SetCapacity(8);

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
    g_IAC.m_StoredInvocation.Release();
    env->CallVoidMethod(g_IAC.m_IAC, g_IAC.m_Stop);
    env->DeleteGlobalRef(g_IAC.m_IAC);
    env->DeleteGlobalRef(g_IAC.m_IACJNI);
    Detach();
    g_IAC.m_IAC = NULL;
    g_IAC.m_IACJNI = NULL;
    g_IAC.m_L = 0;
    g_IAC.m_Callback = LUA_NOREF;
    g_IAC.m_Self = LUA_NOREF;
    dmMutex::Delete(g_IAC.m_Mutex);
    return dmExtension::RESULT_OK;
}


dmExtension::Result InitializeIAC(dmExtension::Params* params)
{
    return dmIAC::Initialize(params);
}


dmExtension::Result FinalizeIAC(dmExtension::Params* params)
{
    if (params->m_L == g_IAC.m_Listener.m_L && g_IAC.m_Listener.m_Callback != LUA_NOREF) {
        dmScript::Unref(g_IAC.m_Listener.m_L, LUA_REGISTRYINDEX, g_IAC.m_Listener.m_Callback);
        dmScript::Unref(g_IAC.m_Listener.m_L, LUA_REGISTRYINDEX, g_IAC.m_Listener.m_Self);
        g_IAC.m_Listener.m_L = 0;
        g_IAC.m_Listener.m_Callback = LUA_NOREF;
        g_IAC.m_Listener.m_Self = LUA_NOREF;
    }
    return dmIAC::Finalize(params);
}


dmExtension::Result UpdateIAC(dmExtension::Params* params)
{
    dmMutex::ScopedLock lk(g_IAC.m_Mutex);
    for (uint32_t i=0;i!=g_IAC.m_CmdQueue.Size();i++)
    {
        Command& cmd = g_IAC.m_CmdQueue[i];

        switch (cmd.m_Type)
        {
            case CMD_INVOKE:
                {
                    if (g_IAC.m_Listener.m_Callback == LUA_NOREF)
                    {
                        dmLogError("No iac listener set. Invocation discarded.");
                        g_IAC.m_StoredInvocation.Store(cmd.m_Payload, cmd.m_Origin);
                    }
                    else
                    {
                        OnInvocation(cmd.m_Payload, cmd.m_Origin);
                    }
                }
                break;
        }
        if (cmd.m_Payload != 0x0)
        {
            free((void*)cmd.m_Payload);
            cmd.m_Payload = 0x0;
        }
        if (cmd.m_Origin != 0x0)
        {
            free((void*)cmd.m_Origin);
            cmd.m_Origin = 0x0;
        }
    }
    g_IAC.m_CmdQueue.SetSize(0);
    return dmExtension::RESULT_OK;
}


DM_DECLARE_EXTENSION(IACExt, "IAC", AppInitializeIAC, AppFinalizeIAC, InitializeIAC, UpdateIAC, 0, FinalizeIAC)

