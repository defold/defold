// Some good notes on the app flow
//    https://developer.nintendo.com/html/online-docs/nx-en/g1kr9vj6-en/Packages/SDK/NintendoSDK/Documents/Package/contents/Pages/Page_110932904.html
//    https://developer.nintendo.com/html/online-docs/nx-en/g1kr9vj6-en/Packages/SDK/NintendoSDK/Documents/Package/contents/Pages/Page_105383443.html
// Exiting the program:
//    https://developer.nintendo.com/html/online-docs/nx-en/g1kr9vj6-en/Packages/SDK/NintendoSDK/Documents/Package/contents/Pages/Page_162179114.html

#include <app/app.h>

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
