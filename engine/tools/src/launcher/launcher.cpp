#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <dlib/dlib.h>
#include <dlib/path.h>
#include <dlib/sys.h>
#include <dlib/dstrings.h>
#include <dlib/log.h>
#include <dlib/configfile.h>

static void MakePath(dmConfigFile::HConfig config, char* resources_path, const char* key, const char* default_value, char* buf, int buf_len)
{
	const char* value = dmConfigFile::GetString(config, key, default_value);
	if (value[0] == '/') {
		dmStrlCpy(buf, "", buf_len);
	} else {
		dmStrlCpy(buf, resources_path, buf_len);
		dmStrlCat(buf, "/", buf_len);
	}
	dmStrlCat(buf, value, buf_len);
}

void Launch(int argc, char **argv) {
	char resources_path[DMPATH_MAX_PATH];
	char config_path[DMPATH_MAX_PATH];
	char java_path[DMPATH_MAX_PATH];
	char jar_path[DMPATH_MAX_PATH];
	char icon_arg[DMPATH_MAX_PATH];

	dmSys::Result r = dmSys::GetResourcesPath(argc, (char**) argv, resources_path, sizeof(resources_path));
	if (r != dmSys::RESULT_OK) {
		dmLogFatal("Failed to located resources path (%d)", r);
		return;
	}

	dmStrlCpy(config_path, resources_path, sizeof(config_path));
	dmStrlCat(config_path, "/config", sizeof(config_path));

	dmConfigFile::HConfig config;
	dmConfigFile::Result cr = dmConfigFile::Load(config_path, argc, (const char**) argv, &config);

	if (cr != dmConfigFile::RESULT_OK) {
		dmLogFatal("Failed to load config file '%s' (%d)", config_path, r);
		return;
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
#ifdef __MACH__
 	dmStrlCpy(icon_arg, "-Xdock:icon=", sizeof(icon_arg));
 	dmStrlCat(icon_arg, resources_path, sizeof(icon_arg));
 	dmStrlCat(icon_arg, "/logo.icns", sizeof(icon_arg));
 	args[i++] = icon_arg;
#endif

	args[i++] = (char*) main;
	args[i++] = 0;

	for (int j = 0; j < i; j++) {
		dmLogDebug("arg %d: %s", j, args[j]);
	}

	//if (fork() == 0) {
		int er = execv(args[0], (char *const *) args);
		if (er < 0) {
			char buf[2048];
		     strerror_r(errno, buf, sizeof(buf));
		     dmLogFatal("Failed to launch application: %s", buf);
		}
/*	} else {
		wait(0);
		exit(0);
	}*/

    delete[] args;
    dmConfigFile::Delete(config);
}

int main(int argc, char **argv) {
	Launch(argc, argv);
	return 0;
}
