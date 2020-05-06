// Some good notes on the app flow
//    https://developer.nintendo.com/html/online-docs/nx-en/g1kr9vj6-en/Packages/SDK/NintendoSDK/Documents/Package/contents/Pages/Page_110932904.html
//    https://developer.nintendo.com/html/online-docs/nx-en/g1kr9vj6-en/Packages/SDK/NintendoSDK/Documents/Package/contents/Pages/Page_105383443.html
// Exiting the program:
//    https://developer.nintendo.com/html/online-docs/nx-en/g1kr9vj6-en/Packages/SDK/NintendoSDK/Documents/Package/contents/Pages/Page_162179114.html

#include <stdio.h>

#include <nn/fs.h>
#include <nn/fs/fs_Debug.h>
#include <nn/os.h>
#include <nn/oe.h>

extern "C" int main(int argc, char** argv);

extern "C" void nnMain()
{
	// https://developer.nintendo.com/html/online-docs/nx-en/g1kr9vj6-en/Packages/SDK/NintendoSDK/Documents/Package/contents/external.html?file=../../Api/HtmlNX/index.html
    size_t mount_rom_cache_size = 0;
    nn::fs::QueryMountRomCacheSize(&mount_rom_cache_size);
    void* mount_rom_cache_buffer = malloc(mount_rom_cache_size);

    nn::fs::MountRom("data", mount_rom_cache_buffer, mount_rom_cache_size);
    nn::fs::MountSaveDataForDebug("save"); // using this instead of relying on the user account selection
    nn::fs::MountCacheStorage("cache");
    nn::fs::MountHost("host", ".");

    nn::oe::Initialize();
    nn::oe::SetOperationModeChangedNotificationEnabled(true); // Detect docking/undocking

    int r = main(nn::os::GetHostArgc(), nn::os::GetHostArgv());
    (void)r;

    nn::fs::Unmount("host");
    nn::fs::Unmount("cache");
    nn::fs::Unmount("save");
    nn::fs::Unmount("data");
    free(mount_rom_cache_buffer);
}
