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

static unsigned g_Invocations;

@interface DMTestMenuTarget : NSObject
- (IBAction)runBundledCommand:(id)sender;
@end

@implementation DMTestMenuTarget
- (IBAction)runBundledCommand:(id)sender
{
    ++g_Invocations;
}

@end

// The bundle's menu and its action target must remain usable after the last window handle is recreated.
TEST(CocoaMenuTest, BundledMenuCommand)
{
    g_Invocations = 0;
    for (unsigned i = 1; i <= 2; ++i)
    {
        @autoreleasepool
        {
            HWindow window = dmPlatform::NewWindow();
            NSMenu* commands = [NSApp.mainMenu itemWithTitle:@"Test Commands"].submenu;
            ASSERT_EQ((NSInteger)1, commands.numberOfItems);
            [commands performActionForItemAtIndex:0];
            ASSERT_EQ(i, g_Invocations);
            dmPlatform::DeleteWindow(window);
        }
    }
}

int main(int argc, char** argv)
{
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
