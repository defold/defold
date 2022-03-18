//
// Copyright 2022 Defold Foundation (www.defold.com)
// License: MIT
//
// Build with: cl.exe vswhere2.cpp
//
// Run with:
//
// vswhere2.exe [flag]
// where "flag" is either
// ""               -- no flag outputs all properties as "key: value"
// "--sdk_root"     -- outputs only the sdk root
// "--sdk_version"  -- outputs only the sdk version number
// "--includes"     -- outputs only the include paths
// "--lib_paths"    -- outputs only the library paths
// "--bin_paths"    -- outputs only the executable paths
//
// Version log:
//  2022-02-01: Initial version
//

#include "microsoft_craziness.h"
#include <stdio.h>

#pragma comment(lib, "advapi32")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "OleAut32.lib")

enum Feature
{
    SDK_ROOT            = 1 << 0,
    SDK_VERSION         = 1 << 1,
    LIBRARY_PATHS       = 1 << 2,
    INCLUDE_PATHS       = 1 << 3,
    BIN_PATHS           = 1 << 4,
    VS_ROOT             = 1 << 5,
    VS_VERSION          = 1 << 6,
    MAX                 = 1 << 7,
    ALL                 = 0xFF
};

static Feature get_feature(const char* feature)
{
    if (strcmp(feature, "--sdk_root") == 0)
        return SDK_ROOT;
    if (strcmp(feature, "--sdk_version") == 0)
        return SDK_VERSION;
    if (strcmp(feature, "--lib_paths") == 0)
        return LIBRARY_PATHS;
    if (strcmp(feature, "--includes") == 0)
        return INCLUDE_PATHS;
    if (strcmp(feature, "--bin_paths") == 0)
        return BIN_PATHS;
    if (strcmp(feature, "--vs_root") == 0)
        return VS_ROOT;
    if (strcmp(feature, "--vs_version") == 0)
        return VS_VERSION;
    return ALL;
}

static void output(const char* name, uint32_t num_values, const wchar_t** values, bool value_only)
{
    if (!value_only)
        printf("%s: ", name);

    uint32_t num_output = 0;
    for (uint32_t i = 0; i < num_values; ++i)
    {
        if (values[i])
        {
            if (num_output)
                printf(",");

            printf("%ls", values[i]);
            num_output++;
        }
    }

    printf("\n");
}

int main(int argc, char const *argv[])
{
	Find_Result result = find_visual_studio_and_windows_sdk();

    Feature feature = ALL;
    if (argc > 1)
        feature = get_feature(argv[1]);

    const wchar_t* sdk_root[]       = { result.windows_sdk_root };
    const wchar_t* sdk_version[]    = { result.windows_sdk_version_specific };
    const wchar_t* includes[]       = { result.windows_sdk_um_include_path, result.windows_sdk_ucrt_include_path, result.windows_sdk_shared_include_path, result.vs_include_path };
    const wchar_t* lib_paths[]      = { result.windows_sdk_um_library_path, result.windows_sdk_ucrt_library_path, result.vs_library_path };
    const wchar_t* bin_paths[]      = { result.vs_exe_path, result.windows_sdk_exe_path };
    const wchar_t* vs_root[]        = { result.vs_root };
    const wchar_t* vs_version[]     = { result.vs_version };

    bool value_only = feature != ALL;
    for (uint32_t i = 1; i < MAX; ++i)
    {
        if ( (i & feature) == i )
        {
            switch(i)
            {
            case SDK_ROOT:      output("sdk_root", sizeof(sdk_root)/sizeof(sdk_root[0]), sdk_root, value_only); break;
            case SDK_VERSION:   output("sdk_version", sizeof(sdk_version)/sizeof(sdk_version[0]), sdk_version, value_only); break;
            case INCLUDE_PATHS: output("includes", sizeof(includes)/sizeof(includes[0]), includes, value_only); break;
            case LIBRARY_PATHS: output("lib_paths", sizeof(lib_paths)/sizeof(lib_paths[0]), lib_paths, value_only); break;
            case BIN_PATHS:     output("bin_paths", sizeof(bin_paths)/sizeof(bin_paths[0]), bin_paths, value_only); break;
            case VS_ROOT:       output("vs_root", sizeof(vs_root)/sizeof(vs_root[0]), vs_root, value_only); break;
            case VS_VERSION:    output("vs_version", sizeof(vs_version)/sizeof(vs_version[0]), vs_version, value_only); break;
            }

            if (value_only)
                break;
        }
    }

	return 0;
}