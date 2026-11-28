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

#include "fontviewer_macos.h"
#import <AppKit/AppKit.h>

void FontViewerMacOSSetClipboard(HWindow, const char* text, uint32_t length)
{
    NSString* string = [[NSString alloc] initWithBytes:text length:length encoding:NSUTF8StringEncoding];
    if (string)
    {
        NSPasteboard* pasteboard = [NSPasteboard generalPasteboard];
        [pasteboard clearContents];
        [pasteboard setString:string forType:NSPasteboardTypeString];
        [string release];
    }
}

void FontViewerMacOSSetLinkCursor(HWindow, bool link)
{
    [(link ? [NSCursor pointingHandCursor] : [NSCursor arrowCursor]) set];
}

void FontViewerMacOSDestroyLinkCursor(HWindow)
{
    [[NSCursor arrowCursor] set];
}
