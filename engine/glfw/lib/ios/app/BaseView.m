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

#include "BaseView.h"
#import "TextUtil.h"
#import "AppDelegate.h"

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

#include "internal.h"


static int                  g_AccelerometerEnabled = 0;
static double               g_AccelerometerFrequency = 1.0 / 60.0;

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

    _glfwInput.MouseEmulationTouch = 0;
    return self;
}

- (id)initWithFrame:(CGRect)frame
{
    self.multipleTouchEnabled = YES;
    self.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
    g_BaseView = self;
    if ((self = [super initWithFrame:frame]))
    {
        displayLink = [[UIScreen mainScreen] displayLinkWithTarget:self selector:@selector(newFrame)];
        [displayLink addToRunLoop:[NSRunLoop currentRunLoop] forMode:NSDefaultRunLoopMode];
        displayLink.frameInterval = 1;

        [self setupView];
    }

    markedText = [[NSMutableString alloc] initWithCapacity:128];
    fakeText = FAKE_STRING;

    _glfwInput.MouseEmulationTouch = 0;
    for (int i = 0; i < GLFW_MAX_TOUCH; ++i)
    {
        _glfwInput.Touch[i].Id = i;
        _glfwInput.Touch[i].Reference = 0x0;
        _glfwInput.Touch[i].Phase = GLFW_PHASE_IDLE;
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
        _glfwSetMarkedText("");
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

- (UITextWritingDirection) baseWritingDirectionForPosition: (UITextPosition *)position inDirection: (UITextStorageDirection)direction {
    return UITextWritingDirectionRightToLeft;
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

- (void) setBaseWritingDirection: (UITextWritingDirection)writingDirection forRange:(UITextRange *)range { }

- (void)swapBuffers
{
}

- (void)newFrame
{
    if (_glfwWin.iconified)
        return;

    countDown--;

    [g_ApplicationDelegate appUpdate]; // will eventually call glfwSwapBuffers -> swapBuffers
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

- (GLFWTouch*) touchById: (UITouch*) ref
{
    int32_t i;

    GLFWTouch* freeTouch = 0x0;
    for (i=0;i!=GLFW_MAX_TOUCH;i++)
    {
        _glfwInput.Touch[i].Id = i;
        if (_glfwInput.Touch[i].Reference == ref) {
            return &_glfwInput.Touch[i];
        }

        // Save touch entry for later if we need to "alloc" one in case we don't find the current reference.
        if (freeTouch == 0x0 && _glfwInput.Touch[i].Reference == 0x0) {
            freeTouch = &_glfwInput.Touch[i];
        }
    }

    if (freeTouch != 0x0) {
        freeTouch->Reference = ref;
    }

    return freeTouch;
}

- (void) updateGlfwMousePos: (int32_t) x y: (int32_t) y
{
    _glfwInput.MousePosX = x;
    _glfwInput.MousePosY = y;
}

- (void) touchStart: (GLFWTouch*) glfwt withTouch: (UITouch*) t
{
    // When a new touch starts, and there was no previous one, this will be our mouse emulation touch.
    if (_glfwInput.MouseEmulationTouch == 0x0) {
        _glfwInput.MouseEmulationTouch = glfwt;
    }

    CGPoint touchLocation = [t locationInView:self];
    //CGPoint prevTouchLocation = [t previousLocationInView:self];
    CGFloat scaleFactor = self.contentScaleFactor;

    int x = touchLocation.x * scaleFactor;
    int y = touchLocation.y * scaleFactor;

    glfwt->Phase = GLFW_PHASE_BEGAN;
    glfwt->X = x;
    glfwt->Y = y;
    glfwt->DX = 0;
    glfwt->DY = 0;
}

- (void) touchUpdate: (GLFWTouch*) glfwt withTouch: (UITouch*) t
{
    CGPoint touchLocation = [t locationInView:self];
    CGPoint prevTouchLocation = [t previousLocationInView:self];
    CGFloat scaleFactor = self.contentScaleFactor;

    int x = touchLocation.x * scaleFactor;
    int y = touchLocation.y * scaleFactor;
    int px = prevTouchLocation.x * scaleFactor;
    int py = prevTouchLocation.y * scaleFactor;

    int prevPhase = glfwt->Phase;
    int newPhase = t.phase;

    // If previous phase was TAPPED, we need to return early since we currently cannot buffer actions/phases.
    if (prevPhase == GLFW_PHASE_TAPPED) {
        return;
    }

    // If this touch is currently used for mouse emulation, and it ended, unset the mouse emulation pointer.
    if (newPhase == GLFW_PHASE_ENDED && _glfwInput.MouseEmulationTouch == glfwt) {
        _glfwInput.MouseEmulationTouch = 0x0;
    }

    // This is an invalid touch order, we need to recieve a began or moved
    // phase before moving pushing any more move inputs.
    if (prevPhase == GLFW_PHASE_ENDED && newPhase == GLFW_PHASE_MOVED) {
        return;
    }

    glfwt->TapCount = t.tapCount;
    glfwt->X = x;
    glfwt->Y = y;
    glfwt->DX = x - px;
    glfwt->DY = y - py;

    // If we recieved both a began and moved for the same touch during one frame/update,
    // just update the coordinates but leave the phase as began.
    if (prevPhase == GLFW_PHASE_BEGAN && newPhase == GLFW_PHASE_MOVED) {
        return;

    // If a touch both began and ended during one frame/update, set the phase as
    // tapped and we will send the released event during next update (see input.c).
    } else if (prevPhase == GLFW_PHASE_BEGAN && newPhase == GLFW_PHASE_ENDED) {
        glfwt->Phase = GLFW_PHASE_TAPPED;
        return;
    }

    glfwt->Phase = t.phase;

}

- (void) fillTouchStart: (UIEvent*) event
{
    NSSet *touches = [event allTouches];

    for (UITouch *t in touches)
    {
        if (GLFW_PHASE_BEGAN == t.phase) {
            GLFWTouch* glfwt = [self touchById: t];
            if (glfwt == 0x0) {
                // Could not find corresponding GLFWTouch.
                // Possibly due to too many touches at once,
                // we only support GLFW_MAX_TOUCH.
                continue;
            }

            // We can't start/begin a new touch if it already has an ongoing phase (ie not idle).
            if (glfwt->Phase != GLFW_PHASE_IDLE) {
                continue;
            }

            [self touchStart: glfwt withTouch: t];

            if (glfwt == _glfwInput.MouseEmulationTouch) {
                [self updateGlfwMousePos: glfwt->X y: glfwt->Y];
                _glfwInputMouseClick( GLFW_MOUSE_BUTTON_LEFT, GLFW_PRESS );
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
            GLFWTouch* glfwt = [self touchById: t];
            if (glfwt == 0x0) {
                // Could not find corresponding GLFWTouch.
                // Possibly due to too many touches at once,
                // we only support GLFW_MAX_TOUCH.
                continue;
            }

            // We can only update previous touches that has been initialized (began, moved etc).
            if (glfwt->Phase == GLFW_PHASE_IDLE) {
                glfwt->Reference = 0x0;
                continue;
            }

            [self touchUpdate: glfwt withTouch: t];

            if (glfwt == _glfwInput.MouseEmulationTouch || !_glfwInput.MouseEmulationTouch) {
                [self updateGlfwMousePos: glfwt->X y: glfwt->Y];
                if ((phase == GLFW_PHASE_ENDED || phase == GLFW_PHASE_CANCELLED)) {
                    _glfwInputMouseClick( GLFW_MOUSE_BUTTON_LEFT, GLFW_RELEASE );
                } else {
                    if (_glfwWin.mousePosCallback) {
                        _glfwWin.mousePosCallback(glfwt->X, glfwt->Y);
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
        _glfwShowKeyboard(0, 0, 0);
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
    _glfwSetMarkedText((char*)[markedText UTF8String]);
}

- (void)insertText:(NSString *)theText
{
    int length = [theText length];

    if (length == 1 && [theText characterAtIndex: 0] == 10) {
        _glfwInputKey( GLFW_KEY_ENTER, GLFW_PRESS );
        self.textkeyActive = TEXT_KEY_COOLDOWN;
        return;
    }

    for(int i = 0;  i < length;  i++) {
        // Trick to "fool" glfw. Otherwise repeated characters will be filtered due to repeat
        _glfwInputChar( [theText characterAtIndex:i], GLFW_RELEASE );
        _glfwInputChar( [theText characterAtIndex:i], GLFW_PRESS );
    }
}

- (void)deleteBackward
{
    if (markedText.length > 0)
    {
        [markedText setString:@""];
        _glfwSetMarkedText("");
    } else {
        _glfwInputKey( GLFW_KEY_BACKSPACE, GLFW_RELEASE );
        _glfwInputKey( GLFW_KEY_BACKSPACE, GLFW_PRESS );
        self.textkeyActive = TEXT_KEY_COOLDOWN;
    }
}

- (void)clearMarkedText
{
    [inputDelegate textWillChange: self];
    [markedText setString: @""];
    [inputDelegate textDidChange: self];
    _glfwSetMarkedText("");
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

- (void)dealloc
{
    if (displayLink != 0)
    {
        [displayLink release];
    }

    [self teardownView];

    [super dealloc];
}

@end


//========================================================================
// Reset keyboard input state (clears marked text)
//========================================================================

void _glfwResetKeyboard( void )
{
    BaseView* view = (BaseView*) _glfwWin.view;
    [view clearMarkedText];
}

//========================================================================
// Get physical accelerometer
//========================================================================

int _glfwPlatformGetAcceleration(float* x, float* y, float* z)
{
    if (g_AccelerometerEnabled) {
        *x = _glfwInput.AccX;
        *y = _glfwInput.AccY;
        *z = _glfwInput.AccZ;
    }
    return g_AccelerometerEnabled;
}

GLFWAPI void glfwAccelerometerEnable()
{
    [[UIAccelerometer sharedAccelerometer] setUpdateInterval:g_AccelerometerFrequency];
    [[UIAccelerometer sharedAccelerometer] setDelegate:_glfwWin.viewController];
    g_AccelerometerEnabled = 1;
}

//========================================================================
// Keyboard
//========================================================================

void _glfwShowKeyboard( int show, int type, int auto_close )
{
    BaseView* view = (BaseView*) _glfwWin.view;
    view.secureTextEntry = NO;
    switch (type) {
        case GLFW_KEYBOARD_DEFAULT:
            view.keyboardType = UIKeyboardTypeDefault;
            break;
        case GLFW_KEYBOARD_NUMBER_PAD:
            view.keyboardType = UIKeyboardTypeNumberPad;
            break;
        case GLFW_KEYBOARD_EMAIL:
            view.keyboardType = UIKeyboardTypeEmailAddress;
            break;
        case GLFW_KEYBOARD_PASSWORD:
            view.secureTextEntry = YES;
            view.keyboardType = UIKeyboardTypeDefault;
            break;
        default:
            view.keyboardType = UIKeyboardTypeDefault;
    }
    view.autoCloseKeyboard = auto_close;
    if (show) {
        view.keyboardActive = YES;
        [_glfwWin.view becomeFirstResponder];
    } else {
        view.keyboardActive = NO;
        [_glfwWin.view resignFirstResponder];
    }
    // check if there are any active special keys and immediately release
    // them when the keyboard is manipulated
    if (view.textkeyActive > 0) {
        _glfwInputKey( GLFW_KEY_BACKSPACE, GLFW_RELEASE );
        _glfwInputKey( GLFW_KEY_ENTER, GLFW_RELEASE );
        view.textkeyActive = 0;
    }
}


//========================================================================
// Poll for new window and input events
//========================================================================

void _glfwPlatformPollEvents( void )
{
    BaseView* view = (BaseView*) _glfwWin.view;
    if (view.keyboardActive > 0) {
        view.textkeyActive--;
        if (view.textkeyActive == 0) {
            _glfwInputKey( GLFW_KEY_BACKSPACE, GLFW_RELEASE );
            _glfwInputKey( GLFW_KEY_ENTER, GLFW_RELEASE );
        }
    }
}

int _glfwPlatformGetWindowRefreshRate( void )
{
    BaseView* view = (BaseView*) _glfwWin.view;
    CADisplayLink* displayLink = view->displayLink;

    @try { // displayLink.preferredFramesPerSecond only supported on iOS 10.0 and higher, default to 0 for older versions.
        return displayLink.preferredFramesPerSecond;
    } @catch (NSException* exception) {
        return 0;
    }
}

