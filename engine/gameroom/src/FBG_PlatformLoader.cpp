#ifndef OVRPL_DISABLED

#if defined(_MSC_VER)
#pragma warning(push, 0)
#endif
#include <Windows.h>
#if defined(_MSC_VER)
#pragma warning(pop)
#endif

#include <stdio.h>
#include <stdlib.h>

#include "FBG_Platform.h"
#include "FBG_PlatformVersion.h"

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable: 4996) // 'getenv': This function or variable may be unsafe.
#endif


#if defined(_DEBUG)
// OVR_INTERNAL_CODE allows internal builds to ignore signature failure
// It should never be defined in any build that actually expects to load a signed dll
#define OVR_INTERNAL_CODE sizeof(NoCompile) == BuiltForInternalOnly
#endif



//-----------------------------------------------------------------------------------
// ***** FilePathCharType, ModuleHandleType, ModuleFunctionType
//
#define FilePathCharType       wchar_t                // #define instead of typedef because debuggers (VC++, XCode) don't recognize typedef'd types as a string type.
typedef HMODULE                ModuleHandleType;
typedef FARPROC                ModuleFunctionType;

#define ModuleHandleTypeNull   ((ModuleHandleType)NULL)
#define ModuleFunctionTypeNull ((ModuleFunctionType)NULL)


//-----------------------------------------------------------------------------------
// ***** OVRPL_DECLARE_IMPORT
//
// Creates typedef and pointer declaration for a function of a given signature.
// The typedef is <FunctionName>Type, and the pointer is <FunctionName>Ptr.
//
// Example usage:
//     int MultiplyValues(float x, float y);  // Assume this function exists in an external shared library. We don't actually need to redeclare it.
//     OVRPL_DECLARE_IMPORT(int, MultiplyValues, (float x, float y)) // This creates a local typedef and pointer for it.

#define OVRPL_DECLARE_IMPORT(ReturnValue, FunctionName, Arguments)  \
    typedef ReturnValue (OVRP_CDECL *FunctionName##Type)Arguments; \
    static FunctionName##Type FunctionName##PLPtr = NULL;

//-----------------------------------------------------------------------------------
// ***** OVRPL_GETFUNCTION
//
// Loads <FunctionName>Ptr from <library> if not already loaded.
// Assumes a variable named <FunctionName>Ptr of type <FunctionName>Type exists which is called <FunctionName> in LibOVRPlatform.
//
// Example usage:
//     OVRPL_GETFUNCTION(module, MultiplyValues)
//     int result = MultiplyValuesPtr(3.f, 4.f);

#define OVRPL_GETFUNCTION(l, f)        \
    if(!f##PLPtr)                      \
    {                                  \
        union                          \
        {                              \
            f##Type p1;                \
            ModuleFunctionType p2;     \
        } u;                           \
        u.p2 = GetProcAddress(l, #f);  \
        f##PLPtr = u.p1;               \
    }




//-----------------------------------------------------------------------------------
// ***** OVR_MAX_PATH
//
#if !defined(OVR_MAX_PATH)
#define OVR_MAX_PATH  _MAX_PATH
#endif



static size_t OVR_strlcpy(char* dest, const char* src, size_t destsize)
{
  const char* s = src;
  size_t      n = destsize;

  if (n && --n)
  {
    do {
      if ((*dest++ = *s++) == 0)
        break;
    } while (--n);
  }

  if (!n)
  {
    if (destsize)
      *dest = 0;
    while (*s++)
    {
    }
  }

  return (size_t)((s - src) - 1);
}


static size_t OVR_strlcat(char* dest, const char* src, size_t destsize)
{
  const size_t d = destsize ? strlen(dest) : 0;
  const size_t s = strlen(src);
  const size_t t = s + d;

  if (t < destsize)
    memcpy(dest + d, src, (s + 1) * sizeof(*src));
  else
  {
    if (destsize)
    {
      memcpy(dest + d, src, ((destsize - d) - 1) * sizeof(*src));
      dest[destsize - 1] = 0;
    }
  }

  return t;
}


#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4201)
#endif

#include <Softpub.h>
#include <Wincrypt.h>

#ifdef _MSC_VER
#pragma warning(pop)
#endif

static ModuleHandleType OVR_OpenLibrary(const FilePathCharType* libraryPath, fbgPlatformInitializeResult* outResult)
{
  DWORD fullPathNameLen = 0;
  FilePathCharType fullPath[MAX_PATH] = { 0 };
  HANDLE hFilePinned = INVALID_HANDLE_VALUE;
  ModuleHandleType hModule = 0;
  fullPathNameLen = GetFullPathNameW(libraryPath, MAX_PATH, fullPath, 0);
  if (fullPathNameLen <= 0 || fullPathNameLen >= MAX_PATH)
  {
    *outResult = fbgPlatformInitialize_FileInvalid;
    return ModuleHandleTypeNull;
  }
  fullPath[MAX_PATH - 1] = 0;

  // In this case we thought we found a valid library at the location, so we'll load it, and if that
  // fails for some reason we'll mark it as an invalid file error.
  hModule = LoadLibraryW(fullPath);

  if (hModule == ModuleHandleTypeNull)
  {
    *outResult = fbgPlatformInitialize_FileInvalid;
  }
  else
  {
    *outResult = fbgPlatformInitialize_Success;
  }

  return hModule;
}


static void OVR_CloseLibrary(ModuleHandleType hLibrary)
{
  if (hLibrary)
  {
    // We may need to consider what to do in the case that the library is in an exception state.
    // In a Windows C++ DLL, all global objects (including static members of classes) will be constructed just
    // before the calling of the DllMain with DLL_PROCESS_ATTACH and they will be destroyed just after
    // the call of the DllMain with DLL_PROCESS_DETACH. We may need to intercept DLL_PROCESS_DETACH and
    // have special handling for the case that the DLL is broken.
    FreeLibrary(hLibrary);
  }
}


// Returns a valid ModuleHandleType (e.g. Windows HMODULE) or returns ModuleHandleTypeNull (e.g. NULL).
// The caller is required to eventually call OVR_CloseLibrary on a valid return handle.
//
static ModuleHandleType OVR_FindLibraryPath(int requestedProductVersion, int requestedMajorVersion,
  fbgPlatformInitializeResult *outResult,
  FilePathCharType* libraryPath, size_t libraryPathCapacity)
{
  ModuleHandleType moduleHandle;
  int printfResult;
  FilePathCharType developerDir[OVR_MAX_PATH] = { '\0' };

#if defined(_WIN64)
  const char* pBitDepth = "64";
#else
  const char* pBitDepth = "32";
#endif

  (void)requestedProductVersion;

  moduleHandle = ModuleHandleTypeNull;
  if (libraryPathCapacity)
    libraryPath[0] = '\0';

#define OVR_FILE_PATH_SEPARATOR "\\"

  {
    const char* pLibFbgDllDir = getenv("LIBFBG_DLL_DIR");

    if (pLibFbgDllDir)
    {
      char developerDir8[OVR_MAX_PATH];
      size_t length = OVR_strlcpy(developerDir8, pLibFbgDllDir, sizeof(developerDir8)); // If missing a trailing path separator then append one.

      if ((length > 0) && (length < sizeof(developerDir8)) && (developerDir8[length - 1] != OVR_FILE_PATH_SEPARATOR[0]))
      {
        length = OVR_strlcat(developerDir8, OVR_FILE_PATH_SEPARATOR, sizeof(developerDir8));

        if (length < sizeof(developerDir8))
        {
          size_t i;
          for (i = 0; i <= length; ++i) // ASCII conversion of 8 to 16 bit text.
            developerDir[i] = (FilePathCharType)(uint8_t)developerDir8[i];
        }
      }
    }
  }

  auto lastOpenResult = fbgPlatformInitialize_Uninitialized;

  {
    size_t i;

    // On Windows, only search the developer directory and the usual path
    const FilePathCharType* directoryArray[2];
    directoryArray[0] = developerDir; // Developer directory.
    directoryArray[1] = L""; // No directory, which causes Windows to use the standard search strategy to find the DLL.

    for (i = 0; i < sizeof(directoryArray) / sizeof(directoryArray[0]); ++i)
    {
      printfResult = swprintf(libraryPath, libraryPathCapacity, L"%lsLibFBGPlatform%hs.dll", directoryArray[i], pBitDepth);

      if (*directoryArray[i] == 0)
      {
        int k;
        FilePathCharType foundPath[MAX_PATH] = { 0 };
        DWORD searchResult = SearchPathW(NULL, libraryPath, NULL, MAX_PATH, foundPath, NULL);
        if (searchResult <= 0 || searchResult >= libraryPathCapacity)
        {
          continue;
        }
        foundPath[MAX_PATH - 1] = 0;
        for (k = 0; k < MAX_PATH; ++k)
        {
          libraryPath[k] = foundPath[k];
        }
      }

      if ((printfResult >= 0) && (printfResult < (int)libraryPathCapacity))
      {
        auto openResult = fbgPlatformInitialize_Uninitialized;
        moduleHandle = OVR_OpenLibrary(libraryPath, &openResult);
        if (moduleHandle != ModuleHandleTypeNull)
        {
          *outResult = openResult;
          return moduleHandle;
        }
        else
        {
          lastOpenResult = openResult;
        }
      }
    }
  }

  *outResult = lastOpenResult != fbgPlatformInitialize_Uninitialized ? lastOpenResult : fbgPlatformInitialize_UnableToVerify;
  return moduleHandle;
}


OVRPL_DECLARE_IMPORT(fbgPlatformInitializeResult, fbg_PlatformInitializeWindows, (const char *appId));
OVRPL_DECLARE_IMPORT(fbgMessage*, fbg_PopMessage, ());
OVRPL_DECLARE_IMPORT(bool, fbg_IsEntitled, ());
OVRPL_DECLARE_IMPORT(void, fbg_PlatformInitializeStandaloneAccessToken, (const char *accessToken));

static void LoadFunctions(ModuleHandleType hModule) {
  OVRPL_GETFUNCTION(hModule, fbg_PlatformInitializeWindows);
  OVRPL_GETFUNCTION(hModule, fbg_PopMessage);
  OVRPL_GETFUNCTION(hModule, fbg_IsEntitled);
  OVRPL_GETFUNCTION(hModule, fbg_PlatformInitializeStandaloneAccessToken);
}

static fbgPlatformInitializeResult InitializeResult = fbgPlatformInitialize_Uninitialized;

OVRPL_PUBLIC_FUNCTION(fbgPlatformInitializeResult) fbg_PlatformInitializeWindowsEx(const char *appId, int productVersion, int majorVersion)
{
  if (InitializeResult != fbgPlatformInitialize_Uninitialized) {
    return InitializeResult;
  }

  // Check to make sure the headers they're using (which automatically pass the version to this function) match the version
  // that this loader library was compiled under.
  if (productVersion != PLATFORM_PRODUCT_VERSION || majorVersion != PLATFORM_MAJOR_VERSION) {
    InitializeResult = fbgPlatformInitialize_VersionMismatch;
    return InitializeResult;
  }

  // See if the one we want has already been loaded by mirroring the OVR_FindLibraryPath name generation but without the path
  FilePathCharType preLoadLibName[OVR_MAX_PATH];
  FilePathCharType preLoadFilePath[OVR_MAX_PATH];

#if defined(_WIN64)
  const char* pBitDepth = "64";
#else
  const char* pBitDepth = "32";
#endif

  swprintf(preLoadLibName, sizeof(preLoadLibName) / sizeof(preLoadLibName[0]), L"LibFBGPlatform%hs.dll", pBitDepth);
  auto hLibPreLoad = GetModuleHandleW(preLoadLibName);
  if (hLibPreLoad != NULL) {
    // Someone already loaded the module. Might be fine, just copy the path out so we can check it later.
    GetModuleFileNameW(hLibPreLoad, preLoadFilePath, sizeof(preLoadFilePath) / sizeof(preLoadFilePath[0]));
  }

  FilePathCharType filePath[OVR_MAX_PATH];

  auto hLib = OVR_FindLibraryPath(PLATFORM_PRODUCT_VERSION, PLATFORM_MAJOR_VERSION, &InitializeResult, filePath, sizeof(filePath) / sizeof(filePath[0]));

  if (InitializeResult == fbgPlatformInitialize_Success) {
    if (hLibPreLoad != NULL && wcsicmp(filePath, preLoadFilePath) != 0) {
      // The pre-loaded module was on a different path than the validated library. Not a particularly likely case, but if it happens we should fail, since
      // the non-shimmed functions could call into the wrong library.
      InitializeResult = fbgPlatformInitialize_PreLoaded;
      return InitializeResult;
    }

    LoadFunctions(hLib);
    InitializeResult = fbg_PlatformInitializeWindowsPLPtr(appId);
  }

  return InitializeResult;
}


OVRPL_PUBLIC_FUNCTION(fbgMessage*) fbg_PopMessage() {
  if (InitializeResult == fbgPlatformInitialize_Success) {
    return fbg_PopMessagePLPtr();
  }
  return nullptr;
}

// Defold change, missing pop for warning push in beginning of file (line 19).
#if defined(_MSC_VER)
#pragma warning(pop)
#endif

#endif
