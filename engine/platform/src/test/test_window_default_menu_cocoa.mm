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

#import <AppKit/AppKit.h>
#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>
#include "../window.hpp"

// The generated menu must use the game's display name even when its executable and bundle names differ.
TEST(CocoaDefaultMenuTest, BundleDisplayName)
{
    @autoreleasepool
    {
        HWindow window = dmPlatform::NewWindow();
        NSMenu* app = NSApp.mainMenu.itemArray.firstObject.submenu;
        EXPECT_TRUE([app itemWithTitle:@"Hide Defold Menu Test"].action == @selector(hide:));
        EXPECT_TRUE([app itemWithTitle:@"Quit Defold Menu Test"].action == @selector(terminate:));
        dmPlatform::DeleteWindow(window);
    }
}

// A game without a menu nib must still expose the standard About panel and AppKit's Services menu.
TEST(CocoaDefaultMenuTest, AboutAndServices)
{
    @autoreleasepool
    {
        HWindow window = dmPlatform::NewWindow();
        NSMenu* app = NSApp.mainMenu.itemArray.firstObject.submenu;
        EXPECT_TRUE([app itemWithTitle:@"About Defold Menu Test"].action == @selector(orderFrontStandardAboutPanel:));
        NSMenu* services = [app itemWithTitle:@"Services"].submenu;
        EXPECT_TRUE(services != nil);
        EXPECT_TRUE(services == NSApp.servicesMenu);
        dmPlatform::DeleteWindow(window);
    }
}

int main(int argc, char** argv)
{
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
