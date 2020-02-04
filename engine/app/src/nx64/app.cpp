// Some good notes on the app flow
//    https://developer.nintendo.com/html/online-docs/nx-en/g1kr9vj6-en/Packages/SDK/NintendoSDK/Documents/Package/contents/Pages/Page_110932904.html
//    https://developer.nintendo.com/html/online-docs/nx-en/g1kr9vj6-en/Packages/SDK/NintendoSDK/Documents/Package/contents/Pages/Page_105383443.html
// Exiting the program:
//    https://developer.nintendo.com/html/online-docs/nx-en/g1kr9vj6-en/Packages/SDK/NintendoSDK/Documents/Package/contents/Pages/Page_162179114.html

#include <app/app.h>

#include <stdio.h>
#include <nn/os/os_Argument.h>

extern "C" int main(int argc, char** argv);

/* if we wish to override the function from lib_init.a
// https://developer.nintendo.com/html/online-docs/nx-en/g1kr9vj6-en/Packages/SDK/NintendoSDK/Documents/Package/contents/external.html?file=../../Api/HtmlNX/index.html
extern "C" void nninitStartup()
{
 NN_LOG("An nninitStartup() function is invoked.\n\n");

 // Set the total size of the memory heap
 const size_t MemoryHeapSize = 16 * 1024 * 1024;
 NN_ABORT_UNLESS_RESULT_SUCCESS( nn::os::SetMemoryHeapSize( MemoryHeapSize ) );

 // Allocated the memory space used by malloc from the memory heap
 uintptr_t address;
 NN_ABORT_UNLESS_RESULT_SUCCESS( nn::os::AllocateMemoryBlock( &address, MemoryHeapSize ) );

 // Set a memory space for malloc
 nn::init::InitializeAllocator( reinterpret_cast<void*>(address), MemoryHeapSize );
}
*/

extern "C" void nnMain()
{
	// https://developer.nintendo.com/html/online-docs/nx-en/g1kr9vj6-en/Packages/SDK/NintendoSDK/Documents/Package/contents/external.html?file=../../Api/HtmlNX/index.html
	// InitializeAllocator();
    printf("HELLO WORLD!\n");
    int r = main(nn::os::GetHostArgc(), nn::os::GetHostArgv());
    (void)r;
}