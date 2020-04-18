// Some good notes on the app flow
//    https://developer.nintendo.com/html/online-docs/nx-en/g1kr9vj6-en/Packages/SDK/NintendoSDK/Documents/Package/contents/Pages/Page_110932904.html
//    https://developer.nintendo.com/html/online-docs/nx-en/g1kr9vj6-en/Packages/SDK/NintendoSDK/Documents/Package/contents/Pages/Page_105383443.html
// Exiting the program:
//    https://developer.nintendo.com/html/online-docs/nx-en/g1kr9vj6-en/Packages/SDK/NintendoSDK/Documents/Package/contents/Pages/Page_162179114.html

#include <app/app.h>

#include <new>
#include <stdio.h>
#include <nn/nn_Log.h>
#include <nn/init.h>
#include <nn/mem.h>
#include <nn/fs.h>
#include <nn/os.h>
#include <nn/htc.h> // For host getenv

extern "C" int main(int argc, char** argv);

// TODO: Move the mount stuff to the actual App Create
static char* GetEnv(const char* name)
{
    nn::htc::Initialize();

    size_t readSize;
    const size_t bufferSize = 64;
    static char buffer[bufferSize];
    if( nn::htc::GetEnvironmentVariable( &readSize, buffer, bufferSize, name ).IsSuccess() )
    {
        return buffer;
    }

    nn::htc::Finalize();

    return getenv(name);
}


// TODO: Move malloc etc to a separate library, so that it can be replaced with a user library

namespace {

	// Allocates a static space as space for malloc().
	const size_t MemoryHeapSize = 1024 * 1024 * 1024;
	NN_ALIGNAS(4096) uint8_t    g_MallocBuffer[ MemoryHeapSize ];

	// Initialization is performed from nninitStartup(). Implement so that the
	// constructor does not start after nninitStartup().
	nn::util::TypedStorage<nn::mem::StandardAllocator,sizeof(nn::mem::StandardAllocator),NN_ALIGNOF(nn::mem::StandardAllocator)>    g_SampleAllocator;

}   // anonymous

 //-----------------------------------------------------------------------------
 //  The six functions malloc, free, calloc, realloc, aligned_alloc, and malloc_usable_size are defined at the same time.
 //-----------------------------------------------------------------------------

 extern "C" void* malloc(size_t size)
 {
    if (size == 0)
        size = 1;
     return Get(g_SampleAllocator).Allocate(size);
 }

 extern "C" void free(void* p)
 {
     if (p)
     {
         Get(g_SampleAllocator).Free(p);
     }
 }

 extern "C" void* calloc(size_t num, size_t size)
 {
     size_t sum = num * size;
     void*  p   = std::malloc(sum);

     if (p)
     {
         std::memset(p, 0, sum);
     }
     return p;
 }

 extern "C" void* realloc(void* p, size_t newSize)
 {
     // Change the memory block size.
     // Reallocate() and realloc() have the same specification, call it without any modifications.
     return Get(g_SampleAllocator).Reallocate(p, newSize);
 }

 extern "C" void* aligned_alloc(size_t alignment, size_t size)
 {
     return Get(g_SampleAllocator).Allocate(size, alignment);
 }

 extern "C" size_t malloc_usable_size(void* p)
 {
     if (!p)
     {
         return 0;
     }
     return Get(g_SampleAllocator).GetSizeOf(p);
 }

// https://developer.nintendo.com/html/online-docs/nx-en/g1kr9vj6-en/Packages/SDK/NintendoSDK/Documents/Package/contents/external.html?file=../../Api/HtmlNX/index.html
extern "C" void nninitStartup()
{
     // The static object constructor has not been called in nninitStartup() yet.
     // Explicitly call it using placement new.
     new( &Get(g_SampleAllocator) ) nn::mem::StandardAllocator;

     // Initialize the allocated buffer as space for malloc.
     Get(g_SampleAllocator).Initialize(g_MallocBuffer, MemoryHeapSize);

     NN_LOG("MallocBuffer is 0x%p - 0x%p (%d mb)\n\n", g_MallocBuffer, g_MallocBuffer + MemoryHeapSize, MemoryHeapSize/(1024*1024));
}

extern "C" void nnMain()
{
    bool has_host_mount = GetEnv("DM_MOUNT_HOST") != 0; // Mostly for unit tests, but also debugging
    if (has_host_mount)
        nn::fs::MountHost("host", ".");

	// https://developer.nintendo.com/html/online-docs/nx-en/g1kr9vj6-en/Packages/SDK/NintendoSDK/Documents/Package/contents/external.html?file=../../Api/HtmlNX/index.html
    size_t mount_rom_cache_size = 0;
    nn::fs::QueryMountRomCacheSize(&mount_rom_cache_size);
    void* mount_rom_cache_buffer = malloc(mount_rom_cache_size);
    nn::fs::MountRom("data", mount_rom_cache_buffer, mount_rom_cache_size);

    int r = main(nn::os::GetHostArgc(), nn::os::GetHostArgv());
    (void)r;

    nn::fs::Unmount("data");
    if (has_host_mount)
        nn::fs::Unmount("host");
    free(mount_rom_cache_buffer);
}
