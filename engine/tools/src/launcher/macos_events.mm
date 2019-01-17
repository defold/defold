#include "macos_events.h" // interface
#include <stdlib.h> // malloc and free
#include <string.h> // strlen and memcpy
#import <Cocoa/Cocoa.h> // NS stuff

char*** files;

@interface AppDelegate : NSObject <NSApplicationDelegate>
@end

@implementation AppDelegate

- (void)application:(NSApplication *)sender openFiles:(NSArray<NSString *> *)filenames {
  if(filenames == nil || [filenames count] == 0) {
    return;
  }
  // We expect to open only one file so 100 should be enough. Change if needed.
  char* tmpArray[100];
  // Use this to trick the compiler. If it detects usage of a stack allocated
  // array in a block it will complain. But in this case it is safe because we
  // are not using the block anywhere else.
  char** array_cheat = tmpArray;
  NSUInteger i = 0;
  NSUInteger* i_cheat = &i;
  [filenames enumerateObjectsUsingBlock:^(NSString* str, NSUInteger idx, BOOL *stop) {
      if(idx == 98) {
        *stop = TRUE;
      }
      else {
        const char *filename = [str cStringUsingEncoding:NSUTF8StringEncoding];
        size_t len = strlen(filename);
        array_cheat[idx] = (char*)malloc(len + 1);
        memcpy(array_cheat[idx], filename, len + 1);
      }
      (*i_cheat)++;
    }];
  tmpArray[i] = NULL;
  size_t list_size = (i + 1) * sizeof(char*);
  *files = (char**)malloc(list_size);
  memcpy(*files, tmpArray, list_size);
}

- (void)applicationDidFinishLaunching:(NSNotification *)aNotification {
  // Tell application to stop and post an empty event to make it actually happen.
  [[NSApplication sharedApplication] stop:nil];
  NSAutoreleasePool* pool = [[NSAutoreleasePool alloc] init];
  NSEvent* event = [NSEvent otherEventWithType:NSEventTypeApplicationDefined
                                      location:NSMakePoint(0, 0)
                                 modifierFlags:0
                                     timestamp:0
                                  windowNumber:0
                                       context:nil
                                       subtype:0
                                         data1:0
                                         data2:0];
  [NSApp postEvent:event atStart:YES];
  [pool drain];
}

- (void)dealloc {
  [super dealloc];
}

@end

void ReceiveFileOpenEvent(char*** fileList) {
  files = fileList;
  *files = NULL;
  [NSApplication sharedApplication];
  AppDelegate* delegate = [[AppDelegate alloc] init];

  [NSApp setDelegate:delegate];
  [NSApp run];

  [delegate dealloc];
}

void FreeFileList(char** fileList) {
  if(!fileList) {
    return;
  }
  char** p = fileList;
  while(*p) {
    free(*p);
    p++;
  }
  free(fileList);
}

