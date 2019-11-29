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
