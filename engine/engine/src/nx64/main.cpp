#include <nn/fs.h>
#include <nn/os.h>
#include <nn/oe.h>

extern int engine_main(int argc, char *argv[]);

extern "C" void nnMain()
{
	// https://developer.nintendo.com/html/online-docs/nx-en/g1kr9vj6-en/Packages/SDK/NintendoSDK/Documents/Package/contents/external.html?file=../../Api/HtmlNX/index.html
    size_t mount_rom_cache_size = 0;
    nn::fs::QueryMountRomCacheSize(&mount_rom_cache_size);
    void* mount_rom_cache_buffer = malloc(mount_rom_cache_size);
    nn::fs::MountRom("data", mount_rom_cache_buffer, mount_rom_cache_size);

    int r = engine_main(nn::os::GetHostArgc(), nn::os::GetHostArgv());
    (void)r;

    nn::fs::Unmount("data");
    free(mount_rom_cache_buffer);
}
