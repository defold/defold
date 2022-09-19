// Copyright 2020-2022 The Defold Foundation
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

#import <UIKit/UIKit.h>
#import <QuartzCore/QuartzCore.h>

@interface BaseView : UIView<UIKeyInput, UITextInput> {
@public
    CADisplayLink* displayLink;
@private
    int backingWidth;
    int backingHeight;
    int countDown;
    int swapInterval;
    UIKeyboardType keyboardType;
    BOOL secureTextEntry;
    id <UITextInputDelegate> inputDelegate;
    NSMutableString *markedText;
    NSString *fakeText;
}

- (void)swapBuffers;
- (void)setSwapInterval: (int) interval;
- (void)setCurrentContext;
- (void)setupView;
- (void)teardownView;

@end

@interface BaseView ()

@property (nonatomic) BOOL keyboardActive;
// TODO: Cooldown "timer" *hack* for backspace and enter release
#define TEXT_KEY_COOLDOWN (10)
@property (nonatomic) int textkeyActive;
@property (nonatomic) int autoCloseKeyboard;

- (void) clearMarkedText;

- (int) getWindowWidth;
- (int) getWindowHeight;
- (void) setWindowWidth:(int)width;
- (void) setWindowHeight:(int)height;

@end
