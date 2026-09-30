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

#include "BaseView.h"
#import "TextUtil.h"
#import "AppDelegate.h"

#import <Foundation/Foundation.h>
#import <CoreMotion/CoreMotion.h>
#import <UIKit/UIKit.h>

#include "internal.h"


static int                  g_AccelerometerEnabled = 0;
static double               g_AccelerometerFrequency = 1.0 / 60.0;
static CMMotionManager*     g_MotionManager = nil;

// AppDelegate.m
extern UIWindow*            g_ApplicationWindow;
extern AppDelegate*         g_ApplicationDelegate;

static BaseView*            g_BaseView = 0;

@implementation BaseView

NSString *const FAKE_STRING = @"Abcd";

+ (Class)layerClass
{
    [self doesNotRecognizeSelector:_cmd];
    return nil;
}

- (id) init {
    self = [super init];
    if (self != nil) {
        [self setSwapInterval: 1];
        markedText = [[NSMutableString alloc] initWithCapacity:128];
        // This hack needed to make sure backspace long press works fine.
        fakeText = FAKE_STRING;
    }

    dmNativeInput.MouseEmulationTouch = 0;
    return self;
}

- (id)initWithFrame:(CGRect)frame
{
    self.multipleTouchEnabled = YES;
    self.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
    g_BaseView = self;
    if ((self = [super initWithFrame:frame]))
    {
        [self setupView];
    }

    markedText = [[NSMutableString alloc] initWithCapacity:128];
    fakeText = FAKE_STRING;

    dmNativeInput.MouseEmulationTouch = 0;
    for (int i = 0; i < NATIVE_MAX_TOUCH; ++i)
    {
        dmNativeInput.Touch[i].Id = i;
        dmNativeInput.Touch[i].Reference = 0x0;
        dmNativeInput.Touch[i].Phase = NATIVE_PHASE_IDLE;
    }

    return self;
}

- (void)setupView
{
}

- (void)teardownView
{
}

//========================================================================
// UITextInput protocol methods
//
// These are used for;
//   * Keyboard input (insertText:)
//   * Keeping track of "marked text" (unfinished keyboard input)
//========================================================================

- (UITextRange *)selectedTextRange {
    // We always return the "caret" position as the end of the marked text
    return [IndexedRange rangeWithNSRange:NSMakeRange(markedText.length, 0)];
}

- (UITextRange *)markedTextRange {
    return [IndexedRange rangeWithNSRange:NSMakeRange(0, markedText.length)];
}

- (void)unmarkText {
    if ([markedText length] > 0) {
        [self insertText: markedText];

        // Clear marked text
        [inputDelegate textWillChange: self];
        [markedText setString: @""];
        [inputDelegate textDidChange: self];
        dmNativeSetMarkedText("");
    }
}

- (NSString *)textInRange:(UITextRange *)range
{
    IndexedRange* _range = (IndexedRange*)range;
    NSString* sub_string = [markedText length] < _range.range.length ? [fakeText substringWithRange:_range.range] : [markedText substringWithRange:_range.range];
    return sub_string;
}

- (void)replaceRange:(UITextRange *)range
            withText:(NSString *)text
{
    IndexedRange* _range = (IndexedRange*)range;
    [markedText replaceCharactersInRange:_range.range withString:text];
}

- (UITextRange *)textRangeFromPosition:(UITextPosition *)fromPosition
                            toPosition:(UITextPosition *)toPosition
{
    IndexedPosition *from = (IndexedPosition *)fromPosition;
    IndexedPosition *to = (IndexedPosition *)toPosition;
    NSRange range = NSMakeRange(MIN(from.index, to.index), ABS(to.index - from.index));
    return [IndexedRange rangeWithNSRange:range];
}

- (UITextPosition *)positionFromPosition:(UITextPosition *)position
                                  offset:(NSInteger)offset
{
    IndexedPosition *pos = (IndexedPosition *)position;
    NSInteger end = pos.index + offset;
    NSString* string = markedText.length != 0 ? markedText : fakeText;
    if (end > [string length] || end < 0)
        return nil;
    return [IndexedPosition positionWithIndex:end];
}

- (NSInteger)offsetFromPosition:(UITextPosition *)fromPosition
                     toPosition:(UITextPosition *)toPosition
{
    IndexedPosition *f = (IndexedPosition *)fromPosition;
    IndexedPosition *t = (IndexedPosition *)toPosition;
    return (t.index - f.index);
}

- (id< UITextInputDelegate >) inputDelegate {
    return inputDelegate;
}

- (void) setInputDelegate: (id <UITextInputDelegate>) delegate {
    inputDelegate = delegate;
}

- (id <UITextInputTokenizer>) tokenizer {
    return [[UITextInputStringTokenizer alloc] initWithTextInput:self];
}

- (NSWritingDirection) baseWritingDirectionForPosition: (UITextPosition *)position inDirection: (UITextStorageDirection)direction {
    return NSWritingDirectionRightToLeft;
}

- (UITextAutocorrectionType) autocorrectionType {
    return UITextAutocorrectionTypeNo;
}

- (UITextSpellCheckingType) spellCheckingType {
    return UITextSpellCheckingTypeNo;
}

- (UITextPosition *) endOfDocument {
    IndexedPosition *pos = [[IndexedPosition alloc] init];
    pos.index = [fakeText length];
    return [pos autorelease];
}


//========================================================================
// UITextInput protocol methods stubs
//
// We use only a subset of the methods in the UITextInput protocol
// to get "marked text" functionality. The methods below need to be
// implemented just to satisfy the protocol but are not used.
//========================================================================

- (void)setSelectedTextRange:(UITextRange *)range {}
- (UITextPosition *) beginningOfDocument { return nil; }

- (UITextPosition *)closestPositionToPoint:(CGPoint)point { return nil; }
- (NSArray *)selectionRectsForRange:(UITextRange *)range { return nil; }
- (UITextPosition *)closestPositionToPoint:(CGPoint)point
                               withinRange:(UITextRange *)range { return nil; }
- (UITextRange *)characterRangeAtPoint:(CGPoint)point { return nil; }

- (UITextPosition *)positionWithinRange:(UITextRange *)range
                    farthestInDirection:(UITextLayoutDirection)direction { return nil; }
- (UITextRange *)characterRangeByExtendingPosition:(UITextPosition *)position
                                       inDirection:(UITextLayoutDirection)direction { return nil; }
- (CGRect)firstRectForRange:(UITextRange *)range { return CGRectMake(0, 0, 0, 0); }
- (CGRect)caretRectForPosition:(UITextPosition *)position { return CGRectMake(0, 0, 0, 0); }

- (UITextPosition *)positionFromPosition:(UITextPosition *)position
                             inDirection:(UITextLayoutDirection)direction
                                  offset:(NSInteger)offset { return nil; }

- (NSComparisonResult)comparePosition:(UITextPosition *)position
                           toPosition:(UITextPosition *)other { return NSOrderedSame; }

- (void) setBaseWritingDirection: (NSWritingDirection)writingDirection forRange:(UITextRange *)range { }

- (void)swapBuffers
{
}

- (void)newFrame
{
    if (!dmNativeOSIsSceneActive() || dmNativeWin.iconified)
        return;

    countDown--;

    [g_ApplicationDelegate appUpdate]; // will eventually call dmNativeSwapBuffers -> swapBuffers
}

- (void) setSwapInterval: (int) interval
{
    if (interval < 1)
    {
        interval = 1;
    }
    swapInterval = interval;
    countDown = swapInterval;
}

- (void)setCurrentContext
{
}

- (NativeTouch*) touchById: (UITouch*) ref
{
    int32_t i;

    NativeTouch* freeTouch = 0x0;
    for (i=0;i!=NATIVE_MAX_TOUCH;i++)
    {
        dmNativeInput.Touch[i].Id = i;
        if (dmNativeInput.Touch[i].Reference == ref) {
            return &dmNativeInput.Touch[i];
        }

        // Save touch entry for later if we need to "alloc" one in case we don't find the current reference.
        if (freeTouch == 0x0 && dmNativeInput.Touch[i].Reference == 0x0) {
            freeTouch = &dmNativeInput.Touch[i];
        }
    }

    if (freeTouch != 0x0) {
        freeTouch->Reference = ref;
    }

    return freeTouch;
}

- (void) updateGlfwMousePos: (int32_t) x y: (int32_t) y
{
    dmNativeInput.MousePosX = x;
    dmNativeInput.MousePosY = y;
}

- (void) touchStart: (NativeTouch*) dmNativet withTouch: (UITouch*) t
{
    // When a new touch starts, and there was no previous one, this will be our mouse emulation touch.
    if (dmNativeInput.MouseEmulationTouch == 0x0) {
        dmNativeInput.MouseEmulationTouch = dmNativet;
    }

    CGPoint touchLocation = [t locationInView:self];
    //CGPoint prevTouchLocation = [t previousLocationInView:self];
    CGFloat scaleFactor = self.contentScaleFactor;

    int x = touchLocation.x * scaleFactor;
    int y = touchLocation.y * scaleFactor;

    dmNativet->Phase = NATIVE_PHASE_BEGAN;
    dmNativet->X = x;
    dmNativet->Y = y;
    dmNativet->DX = 0;
    dmNativet->DY = 0;
}

- (void) touchUpdate: (NativeTouch*) dmNativet withTouch: (UITouch*) t
{
    CGPoint touchLocation = [t locationInView:self];
    CGPoint prevTouchLocation = [t previousLocationInView:self];
    CGFloat scaleFactor = self.contentScaleFactor;

    int x = touchLocation.x * scaleFactor;
    int y = touchLocation.y * scaleFactor;
    int px = prevTouchLocation.x * scaleFactor;
    int py = prevTouchLocation.y * scaleFactor;

    int prevPhase = dmNativet->Phase;
    int newPhase = t.phase;

    // If previous phase was TAPPED, we need to return early since we currently cannot buffer actions/phases.
    if (prevPhase == NATIVE_PHASE_TAPPED) {
        return;
    }

    // If this touch is currently used for mouse emulation, and it ended, unset the mouse emulation pointer.
    if (newPhase == NATIVE_PHASE_ENDED && dmNativeInput.MouseEmulationTouch == dmNativet) {
        dmNativeInput.MouseEmulationTouch = 0x0;
    }

    // This is an invalid touch order, we need to recieve a began or moved
    // phase before moving pushing any more move inputs.
    if (prevPhase == NATIVE_PHASE_ENDED && newPhase == NATIVE_PHASE_MOVED) {
        return;
    }

    dmNativet->TapCount = t.tapCount;
    dmNativet->X = x;
    dmNativet->Y = y;
    dmNativet->DX = x - px;
    dmNativet->DY = y - py;

    // If we recieved both a began and moved for the same touch during one frame/update,
    // just update the coordinates but leave the phase as began.
    if (prevPhase == NATIVE_PHASE_BEGAN && newPhase == NATIVE_PHASE_MOVED) {
        return;

    // If a touch both began and ended during one frame/update, set the phase as
    // tapped and we will send the released event during next update (see input.c).
    } else if (prevPhase == NATIVE_PHASE_BEGAN && newPhase == NATIVE_PHASE_ENDED) {
        dmNativet->Phase = NATIVE_PHASE_TAPPED;
        return;
    }

    dmNativet->Phase = t.phase;

}

- (void) fillTouchStart: (UIEvent*) event
{
    NSSet *touches = [event allTouches];

    for (UITouch *t in touches)
    {
        if (NATIVE_PHASE_BEGAN == t.phase) {
            NativeTouch* dmNativet = [self touchById: t];
            if (dmNativet == 0x0) {
                // Could not find corresponding NativeTouch.
                // Possibly due to too many touches at once,
                // we only support NATIVE_MAX_TOUCH.
                continue;
            }

            // We can't start/begin a new touch if it already has an ongoing phase (ie not idle).
            if (dmNativet->Phase != NATIVE_PHASE_IDLE) {
                continue;
            }

            [self touchStart: dmNativet withTouch: t];

            if (dmNativet == dmNativeInput.MouseEmulationTouch) {
                [self updateGlfwMousePos: dmNativet->X y: dmNativet->Y];
                dmNativeInputMouseClick( NATIVE_MOUSE_BUTTON_LEFT, NATIVE_PRESS );
            }
        }
    }
}

- (void) fillTouch: (UIEvent*) event forPhase:(UITouchPhase) phase
{
    NSSet *touches = [event allTouches];

    for (UITouch *t in touches)
    {
        if (phase == t.phase) {
            NativeTouch* dmNativet = [self touchById: t];
            if (dmNativet == 0x0) {
                // Could not find corresponding NativeTouch.
                // Possibly due to too many touches at once,
                // we only support NATIVE_MAX_TOUCH.
                continue;
            }

            // We can only update previous touches that has been initialized (began, moved etc).
            if (dmNativet->Phase == NATIVE_PHASE_IDLE) {
                dmNativet->Reference = 0x0;
                continue;
            }

            [self touchUpdate: dmNativet withTouch: t];

            if (dmNativet == dmNativeInput.MouseEmulationTouch || !dmNativeInput.MouseEmulationTouch) {
                [self updateGlfwMousePos: dmNativet->X y: dmNativet->Y];
                if ((phase == NATIVE_PHASE_ENDED || phase == NATIVE_PHASE_CANCELLED)) {
                    dmNativeInputMouseClick( NATIVE_MOUSE_BUTTON_LEFT, NATIVE_RELEASE );
                } else {
                    if (dmNativeWin.mousePosCallback) {
                        dmNativeWin.mousePosCallback(dmNativet->X, dmNativet->Y);
                    }
                }
            }
        }
    }
}

- (void)touchesMoved:(NSSet *)touches withEvent:(UIEvent *)event
{
    [self fillTouch: event forPhase: UITouchPhaseMoved];
}

- (void)touchesBegan:(NSSet *)touches withEvent:(UIEvent *)event
{
    if (self.keyboardActive && self.autoCloseKeyboard) {
        // Implicitly hide keyboard
        dmNativeShowKeyboard(0, 0, 0);
    }

    [self fillTouchStart: event];
}

- (void)touchesEnded:(NSSet *)touches withEvent:(UIEvent *)event
{
    [self fillTouch: event forPhase: UITouchPhaseEnded];
}

- (void)touchesCancelled:(NSSet *)touches withEvent:(UIEvent *)event
{
    [self fillTouch: event forPhase: UITouchPhaseCancelled];
}

- (BOOL)canBecomeFirstResponder
{
    return YES;
}

- (BOOL)hasText
{
    return YES;
}

- (void)setMarkedText:(NSString *)newMarkedText selectedRange:(NSRange)selectedRange {
    [markedText setString:newMarkedText];
    dmNativeSetMarkedText((char*)[markedText UTF8String]);
}

- (void)insertText:(NSString *)theText
{
    int length = [theText length];

    if (length == 1 && [theText characterAtIndex: 0] == 10) {
        dmNativeInputKey( NATIVE_KEY_ENTER, NATIVE_PRESS );
        self.textkeyActive = TEXT_KEY_COOLDOWN;
        return;
    }

    for(int i = 0;  i < length;  i++) {
        // Trick to "fool" dmNative. Otherwise repeated characters will be filtered due to repeat
        dmNativeInputChar( [theText characterAtIndex:i], NATIVE_RELEASE );
        dmNativeInputChar( [theText characterAtIndex:i], NATIVE_PRESS );
    }
}

- (void)deleteBackward
{
    if (markedText.length > 0)
    {
        [markedText setString:@""];
        dmNativeSetMarkedText("");
    } else {
        dmNativeInputKey( NATIVE_KEY_BACKSPACE, NATIVE_RELEASE );
        dmNativeInputKey( NATIVE_KEY_BACKSPACE, NATIVE_PRESS );
        self.textkeyActive = TEXT_KEY_COOLDOWN;
    }
}

- (void)clearMarkedText
{
    [inputDelegate textWillChange: self];
    [markedText setString: @""];
    [inputDelegate textDidChange: self];
    dmNativeSetMarkedText("");
}

- (UIKeyboardType) keyboardType
{
    return keyboardType;
}

- (void) setKeyboardType: (UIKeyboardType) type
{
    keyboardType = type;
}

- (BOOL) isSecureTextEntry
{
    return secureTextEntry;
}

- (void) setSecureTextEntry: (BOOL) secureEntry
{
    secureTextEntry = secureEntry;
}

- (UIReturnKeyType) returnKeyType
{
    return UIReturnKeyDefault;
}

- (int) getWindowWidth
{
    return backingWidth;
}

- (int) getWindowHeight
{
    return backingHeight;
}

- (void) setWindowWidth:(int)width
{
    backingWidth = width;
}

- (void) setWindowHeight:(int)height
{
    backingHeight = height;
}

- (void)startDisplayLink
{
    // Fullscreen native presentation can detach the game view while its scene
    // stays active. A replacement view still needs to drive engine callbacks.
    UIScreen* screen = self.window ? self.window.screen : g_ApplicationWindow.screen;
    if (!displayLink && screen)
    {
        displayLink = [[screen displayLinkWithTarget:self selector:@selector(newFrame)] retain];
        [displayLink addToRunLoop:[NSRunLoop mainRunLoop] forMode:NSDefaultRunLoopMode];
    }
}

- (void)invalidateDisplayLink
{
    if (displayLink)
    {
        [displayLink invalidate];
        [displayLink release];
        displayLink = nil;
    }
}

- (void)didMoveToWindow
{
    [super didMoveToWindow];
    if (self.window)
    {
        [self invalidateDisplayLink];
        self.contentScaleFactor = self.window.screen.scale;
        self.layer.contentsScale = self.contentScaleFactor;
        [self startDisplayLink];
        [self setNeedsLayout];
    }
    // Detachment can be temporary. Scene disconnection and view replacement
    // explicitly invalidate the link; a modal presentation must keep it running.
}

- (void)safeAreaInsetsDidChange
{
    [super safeAreaInsetsDidChange];
    self.safeAreaChanged = YES;
}

- (void)dealloc
{
    [self invalidateDisplayLink];
    [self teardownView];
    [_markedTextStyle release];

    [super dealloc];
}

@end


//========================================================================
// Reset keyboard input state (clears marked text)
//========================================================================

void dmNativeResetKeyboard( void )
{
    BaseView* view = (BaseView*) dmNativeWin.view;
    [view clearMarkedText];
}

//========================================================================
// Get physical accelerometer
//========================================================================

int dmNativeOSGetAcceleration(float* x, float* y, float* z)
{
    if (g_AccelerometerEnabled) {
        CMAccelerometerData* data = g_MotionManager.accelerometerData;
        if (data) {
            dmNativeInput.AccX = data.acceleration.x;
            dmNativeInput.AccY = data.acceleration.y;
            dmNativeInput.AccZ = data.acceleration.z;
        }
        *x = dmNativeInput.AccX;
        *y = dmNativeInput.AccY;
        *z = dmNativeInput.AccZ;
    }
    return g_AccelerometerEnabled;
}

void dmNativeAccelerometerEnable()
{
    if (!g_MotionManager)
        g_MotionManager = [[CMMotionManager alloc] init];

    if (g_MotionManager.accelerometerAvailable) {
        g_MotionManager.accelerometerUpdateInterval = g_AccelerometerFrequency;
        [g_MotionManager startAccelerometerUpdates];
    }
    g_AccelerometerEnabled = 1;
}

//========================================================================
// Keyboard
//========================================================================

void dmNativeShowKeyboard( int show, int type, int auto_close )
{
    BaseView* view = (BaseView*) dmNativeWin.view;
    view.secureTextEntry = NO;
    switch (type) {
        case NATIVE_KEYBOARD_DEFAULT:
            view.keyboardType = UIKeyboardTypeDefault;
            break;
        case NATIVE_KEYBOARD_NUMBER_PAD:
            view.keyboardType = UIKeyboardTypeNumberPad;
            break;
        case NATIVE_KEYBOARD_EMAIL:
            view.keyboardType = UIKeyboardTypeEmailAddress;
            break;
        case NATIVE_KEYBOARD_PASSWORD:
            view.secureTextEntry = YES;
            view.keyboardType = UIKeyboardTypeDefault;
            break;
        default:
            view.keyboardType = UIKeyboardTypeDefault;
    }
    view.autoCloseKeyboard = auto_close;
    if (show) {
        view.keyboardActive = YES;
        [dmNativeWin.view becomeFirstResponder];
    } else {
        view.keyboardActive = NO;
        [dmNativeWin.view resignFirstResponder];
    }
    // check if there are any active special keys and immediately release
    // them when the keyboard is manipulated
    if (view.textkeyActive > 0) {
        dmNativeInputKey( NATIVE_KEY_BACKSPACE, NATIVE_RELEASE );
        dmNativeInputKey( NATIVE_KEY_ENTER, NATIVE_RELEASE );
        view.textkeyActive = 0;
    }
}


//========================================================================
// Poll for new window and input events
//========================================================================

void dmNativeOSPollEvents( void )
{
    BaseView* view = (BaseView*) dmNativeWin.view;
    if (view.safeAreaChanged && dmNativeWin.windowSizeCallback)
    {
        int width = [view getWindowWidth];
        int height = [view getWindowHeight];
        if (width > 0 && height > 0)
        {
            // Insets can change after layout, including rotations that preserve the window size.
            view.safeAreaChanged = NO;
            dmNativeWin.windowSizeCallback(width, height);
        }
    }
    if (view.keyboardActive > 0) {
        view.textkeyActive--;
        if (view.textkeyActive == 0) {
            dmNativeInputKey( NATIVE_KEY_BACKSPACE, NATIVE_RELEASE );
            dmNativeInputKey( NATIVE_KEY_ENTER, NATIVE_RELEASE );
        }
    }
}

int dmNativeOSGetWindowRefreshRate( void )
{
    BaseView* view = (BaseView*) dmNativeWin.view;
    CADisplayLink* displayLink = view->displayLink;

    @try { // displayLink.preferredFramesPerSecond only supported on iOS 10.0 and higher, default to 0 for older versions.
        return displayLink.preferredFramesPerSecond;
    } @catch (NSException* exception) {
        return 0;
    }
}
