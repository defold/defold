
#include <cstdio>
#include <frm.h>
#import <Foundation/NSBundle.h>
#import <Foundation/NSString.h>


void GetFrameWorkPath(const char* name, char* buffer, size_t buffer_size)
{
	NSString* ns_name = [NSString stringWithUTF8String: name];

	NSString* libExtension = @"dylib";
	NSString* path = [[NSBundle mainBundle] pathForResource:ns_name ofType:libExtension];
	NSLog(@"Loading dynamic library: %@", path);
	//path=[path stringByAppendingString:[NSString stringWithFormat:@"/%@", ns_name]];
	//NSLog(@" part 2: %@", path);

	snprintf(buffer, buffer_size, "%s", [path cStringUsingEncoding:NSUTF8StringEncoding]);
	printf("copied: %s\n", buffer);

	// void *revealLib = NULL;
	// revealLib = dlopen([path cStringUsingEncoding:NSUTF8StringEncoding], RTLD_NOW);

	// if (revealLib == NULL)
	// {
	// 	char *error = dlerror();
	// 	NSLog(@"dlopen error: %s", error);
	// }
}