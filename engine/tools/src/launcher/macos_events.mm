// Copyright 2020-2026 The Defold Foundation
// Copyright 2014-2020 King
// Copyright 2009-2014 Ragnar Svensson, Christian Murray
// Licensed under the Defold License version 1.0 (the "License"); you may not use
// this file except in compliance with the License.
//
// You may obtain a copy of the License, together with FAQs at
// https://www.defold.com/license
//
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

#include "macos_events.h" // interface
#include <stdlib.h> // malloc and free
#include <string.h> // strlen and memcpy
#import <Cocoa/Cocoa.h> // NS stuff

char** files;

@interface AppDelegate : NSObject <NSApplicationDelegate>
@end

@implementation AppDelegate

- (void)application:(NSApplication *)sender openFiles:(NSArray<NSString *> *)filenames {
  size_t names_count = [filenames count];
  files = (char**)malloc(names_count + 1);
  [filenames enumerateObjectsUsingBlock:^(NSString* str, NSUInteger idx, BOOL *stop) {
      const char *filename = [str cStringUsingEncoding:NSUTF8StringEncoding];
      size_t len = strlen(filename) + 1;
      files[idx] = (char*)malloc(len);
      memcpy(files[idx], filename, len);
      files[idx + 1] = NULL;
    }];
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

char** ReceiveFileOpenEvent() {
  files = NULL;
  [NSApplication sharedApplication];
  AppDelegate* delegate = [[AppDelegate alloc] init];

  [NSApp setDelegate:delegate];
  [NSApp setActivationPolicy: NSApplicationActivationPolicyProhibited];
  [NSApp run];

  [delegate dealloc];
  return files;
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

