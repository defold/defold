#include <launcher.h>
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#ifndef _WIN32
#include <unistd.h>
#include <sys/wait.h>
#endif

#include <dlib/dlib.h>
#include <dlib/path.h>
#include <dlib/sys.h>
#include <dlib/dstrings.h>
#include <dlib/log.h>
#include <dlib/configfile.h>
#include <dlib/template.h>

#ifdef _WIN32
#include <dlib/safe_windows.h>
#endif

// bootstrap.resourcespath must default to resourcespath of the installation
#define RESOURCES_PATH_KEY ("bootstrap.resourcespath")
#define LAUNCHER_PATH_KEY ("bootstrap.launcherpath")
#define MAX_ARGS_SIZE (10 * DMPATH_MAX_PATH)

struct ReplaceContext
{
    dmConfigFile::HConfig m_Config;
    // Will be set to either bootstrap.resourcespath (if set) or the default installation resources path
    const char* m_ResourcesPath;
    const char* m_LauncherPath;
};

static const char* ReplaceCallback(void* user_data, const char* key)
{
    ReplaceContext* context = (ReplaceContext*)user_data;

    if (dmStrCaseCmp(key, RESOURCES_PATH_KEY) == 0)
    {
        return context->m_ResourcesPath;
    } else if (dmStrCaseCmp(key, LAUNCHER_PATH_KEY) == 0)
    {
        return context->m_LauncherPath;
    }

    return dmConfigFile::GetString(context->m_Config, key, 0x0);
}

static bool ConfigGetString(ReplaceContext* context, const char* key, char* buf, uint32_t buf_len)
{
    const char* value = dmConfigFile::GetString(context->m_Config, key, 0x0);
    if (value != 0x0)
    {
        dmTemplate::Result result = dmTemplate::RESULT_OK;
        char last_buf[MAX_ARGS_SIZE];
        *last_buf = '\0';
        dmStrlCpy(buf, value, buf_len);
        int tries_left = 5;
        while (result == dmTemplate::RESULT_OK && dmStrCaseCmp(buf, last_buf) != 0 && tries_left > 0)
        {
            dmStrlCpy(last_buf, buf, buf_len);
            result = dmTemplate::Format(context, buf, buf_len, last_buf, ReplaceCallback);
            --tries_left;
        }
        switch (result)
        {
        case dmTemplate::RESULT_OK:
            return true;
        case dmTemplate::RESULT_MISSING_REPLACEMENT:
            dmLogFatal("One of the replacements in %s could not be resolved: %s", key, buf);
            break;
        case dmTemplate::RESULT_BUFFER_TOO_SMALL:
            dmLogFatal("The buffer is too small to account for the replacements in %s.", key);
            break;
        case dmTemplate::RESULT_SYNTAX_ERROR:
            dmLogFatal("The value at %s has syntax errors: %s", key, buf);
            break;
        }
    }
    dmStrlCpy(buf, "", buf_len);
    return false;
}

int Launch(int argc, char **argv) {
    char default_resources_path[DMPATH_MAX_PATH];
    char config_path[DMPATH_MAX_PATH];
    char java_path[DMPATH_MAX_PATH];
    char jar_path[DMPATH_MAX_PATH];
    char os_args[MAX_ARGS_SIZE];
    char vm_args[MAX_ARGS_SIZE];

    #if defined(__MACH__)
    // Make the dock happy. The application is probably identified from its executable. Without CFProcessPath
    // set autoupdate resulted in two dock icons, launching from command-line in two dock icons, etc
    setenv("CFProcessPath", argv[0], 1);
    #endif

    dmSys::Result r = dmSys::GetResourcesPath(argc, (char**) argv, default_resources_path, sizeof(default_resources_path));
    if (r != dmSys::RESULT_OK) {
        dmLogFatal("Failed to locate resources path (%d)", r);
        return 5;
    }

    dmStrlCpy(config_path, default_resources_path, sizeof(config_path));
    dmStrlCat(config_path, "/config", sizeof(config_path));

    dmConfigFile::HConfig config;
    dmConfigFile::Result cr = dmConfigFile::Load(config_path, argc, (const char**) argv, &config);

    if (cr != dmConfigFile::RESULT_OK) {
        dmLogFatal("Failed to load config file '%s' (%d)", config_path, cr);
        return 5;
    }

    if (dmConfigFile::GetInt(config, "launcher.debug", 0)) {
        dmLogSetlevel(DM_LOG_SEVERITY_DEBUG);
    }

    ReplaceContext context;
    context.m_Config = config;
    context.m_ResourcesPath = dmConfigFile::GetString(config, RESOURCES_PATH_KEY, default_resources_path);
#if defined(_WIN32)
    char argv_0[DMPATH_MAX_PATH];
    GetModuleFileName(NULL, argv_0, DMPATH_MAX_PATH);
    context.m_LauncherPath = argv_0;
#else
    context.m_LauncherPath = argv[0];
#endif
    if (*context.m_ResourcesPath == '\0')
    {
        context.m_ResourcesPath = default_resources_path;
    }

    const char* main = dmConfigFile::GetString(config, "launcher.main", "Main");

    ConfigGetString(&context, "launcher.java", java_path, sizeof(java_path));
    ConfigGetString(&context, "launcher.jar", jar_path, sizeof(jar_path));

    int max_args = 128;
    const char ** args = (const char**) new char*[max_args];
    int i = 0;
    args[i++] = java_path;
    args[i++] = "-cp";
    args[i++] = jar_path;

    const char* os_key = "";
#if defined(__MACH__)
    os_key = "platform.osx";
#elif defined(_WIN32)
    os_key = "platform.windows";
#elif defined(__linux__)
    os_key = "platform.linux";
#endif
    ConfigGetString(&context, os_key, os_args, sizeof(os_args));
    char* s, *last;
    s = dmStrTok(os_args, ",", &last);
    while (s) {
        args[i++] = s;
        s = dmStrTok(0, ",", &last);
    }

    ConfigGetString(&context, "launcher.vmargs", vm_args, sizeof(vm_args));
    s = dmStrTok(vm_args, ",", &last);
    while (s) {
        args[i++] = s;
        s = dmStrTok(0, ",", &last);
    }

    args[i++] = (char*) main;
    args[i++] = 0;

    for (int j = 0; j < i; j++) {
        dmLogDebug("arg %d: %s", j, args[j]);
    }

#ifdef _WIN32
    STARTUPINFO si;
    PROCESS_INFORMATION pi;
    int buffer_size = 32000;
    char* buffer = new char[buffer_size];

    QuoteArgv(args, buffer);

    dmLogDebug("%s", buffer);

    memset(&si, 0, sizeof(si));
    memset(&pi, 0, sizeof(pi));

    si.cb = sizeof(STARTUPINFO);
    si.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
    si.hStdError = GetStdHandle(STD_ERROR_HANDLE);
    si.dwFlags |= STARTF_USESTDHANDLES;

    BOOL ret = CreateProcess(0, buffer, 0, 0, TRUE, CREATE_NO_WINDOW, 0, 0, &si, &pi);
    if (!ret) {
        char* msg;
        DWORD err = GetLastError();
        FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ARGUMENT_ARRAY, 0, err, LANG_NEUTRAL, (LPSTR) &msg, 0, 0);
        dmLogFatal("Failed to launch application: %s (%d)", msg, err);
        LocalFree((HLOCAL) msg);
        exit(5);
    }

    WaitForSingleObject( pi.hProcess, INFINITE);

    DWORD exit_code = 0;
    GetExitCodeProcess(pi.hProcess, &exit_code);

    CloseHandle( pi.hProcess  );
    CloseHandle( pi.hThread  );

    delete[] buffer;
    delete[] args;
    dmConfigFile::Delete(config);

    return exit_code;
#else

    pid_t pid = fork();
    if (pid == 0) {
        int er = execv(args[0], (char *const *) args);
        if (er < 0) {
            char buf[2048];
            strerror_r(errno, buf, sizeof(buf));
            dmLogFatal("Failed to launch application: %s", buf);
            exit(127);
        }
    }
    int stat;
    wait(&stat);

    delete[] args;
    dmConfigFile::Delete(config);

    if (WIFEXITED(stat)) {
        return WEXITSTATUS(stat);
    } else {
        return 127;
    }
#endif
}

int main(int argc, char **argv) {
    dmLogInfo("Launcher version %s", DEFOLD_SHA1);
    int ret = Launch(argc, argv);
    while (ret == 17) {
        ret = Launch(argc, argv);
    }
    return ret;
}

#undef RESOURCES_PATH_KEY
#undef MAX_ARGS_SIZE
