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

#ifdef _WIN32
#include <dlib/safe_windows.h>
#endif

static void MakePath(dmConfigFile::HConfig config, char* resources_path, const char* key, const char* default_value, char* buf, int buf_len)
{
    const char* value = dmConfigFile::GetString(config, key, default_value);
    if (value[0] == '/' || strchr(value, ':')) {
        dmStrlCpy(buf, "", buf_len);
    } else {
        dmStrlCpy(buf, resources_path, buf_len);
        dmStrlCat(buf, "/", buf_len);
    }
    dmStrlCat(buf, value, buf_len);


    for (size_t i = 0; i < strlen(buf); i++) {
        if (buf[i] == '\\') buf[i] = '/';
    }
}

int Launch(int argc, char **argv) {
    char resources_path[DMPATH_MAX_PATH];
    char config_path[DMPATH_MAX_PATH];
    char java_path[DMPATH_MAX_PATH];
    char jar_path[DMPATH_MAX_PATH];
    char packages_arg[DMPATH_MAX_PATH];

    dmSys::Result r = dmSys::GetResourcesPath(argc, (char**) argv, resources_path, sizeof(resources_path));
    if (r != dmSys::RESULT_OK) {
        dmLogFatal("Failed to located resources path (%d)", r);
        return 5;
    }

    dmStrlCpy(packages_arg, "-Ddefold.resourcespath=", sizeof(packages_arg));
    dmStrlCat(packages_arg, resources_path, sizeof(packages_arg));

    dmStrlCpy(config_path, resources_path, sizeof(config_path));
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

    const char* main = dmConfigFile::GetString(config, "launcher.main", "Main");

    MakePath(config, resources_path, "launcher.java", "jre/bin/java", java_path, sizeof(java_path));
    MakePath(config, resources_path, "launcher.jar", "app.jar", jar_path, sizeof(jar_path));

    int max_args = 128;
    const char ** args = (const char**) new char*[max_args];
    int i = 0;
    args[i++] = java_path;
    args[i++] = "-cp";
    args[i++] = jar_path;
    args[i++] = packages_arg;
#ifdef __MACH__
    char icon_arg[DMPATH_MAX_PATH];
    dmStrlCpy(icon_arg, "-Xdock:icon=", sizeof(icon_arg));
    dmStrlCat(icon_arg, resources_path, sizeof(icon_arg));
    dmStrlCat(icon_arg, "/logo.icns", sizeof(icon_arg));
    args[i++] = icon_arg;
#endif

    const char* tmp = dmConfigFile::GetString(config, "launcher.vmargs", 0);
    char *vm_args = 0;
    if (tmp) {
    	vm_args = strdup(tmp);

        char* s, *last;
        s = dmStrTok(vm_args, ",", &last);
        while (s) {
            args[i++] = s;
        	s = dmStrTok(0, ",", &last);
        }
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
    buffer[0] = 0;

    for (int j = 0; j < i - 1; j++) {
		// We must quote on windows...
        if (j == 0) {
            dmStrlCat(buffer, "\"", buffer_size);
        }
        dmStrlCat(buffer, args[j], buffer_size);
        if (j == 0) {
            dmStrlCat(buffer, "\"", buffer_size);
        }
        if (j != i - 2) {
            dmStrlCat(buffer, " ", buffer_size);
        }
    }

	dmLogDebug("%s", buffer);

    memset(&si, 0, sizeof(si));
    memset(&pi, 0, sizeof(pi));

    si.cb = sizeof(STARTUPINFO);
    si.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
    si.hStdError = GetStdHandle(STD_ERROR_HANDLE);
    si.dwFlags |= STARTF_USESTDHANDLES;

    BOOL ret = CreateProcess(0, buffer, 0, 0, TRUE, 0, 0, 0, &si, &pi);
    if (!ret) {
        char* msg;
        DWORD err = GetLastError();
        FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ARGUMENT_ARRAY, 0, err, LANG_NEUTRAL, (LPSTR) &msg, 0, 0);
        LocalFree((HLOCAL) msg);
        dmLogFatal("Failed to launch application: %s (%d)", msg, err);
        exit(5);
    }

    WaitForSingleObject( pi.hProcess, INFINITE);

    DWORD exit_code = 0;
    GetExitCodeProcess(pi.hProcess, &exit_code);

    CloseHandle( pi.hProcess  );
    CloseHandle( pi.hThread  );

    delete[] buffer;
    free(vm_args);
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

    free(vm_args);
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
	int ret = Launch(argc, argv);
	while (ret == 17) {
		ret = Launch(argc, argv);

	}
    return ret;
}
