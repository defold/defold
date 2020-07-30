#include <dmsdk/sdk.h>
#include "tapticengine.h"
#include "UIKit/UIKit.h"

@interface TapticEnginePlugin : NSObject{
}
+ (TapticEnginePlugin*) shared;
- (void) notification:(UINotificationFeedbackType) type;
- (void) selection;
- (void) impact:(UIImpactFeedbackStyle) style;
+ (BOOL) isSupport;
@end

@interface TapticEnginePlugin ()
@property (nonatomic, strong) UINotificationFeedbackGenerator* notificationGenerator;
@property (nonatomic, strong) UISelectionFeedbackGenerator* selectionGenerator;
@property (nonatomic, strong) NSArray<UIImpactFeedbackGenerator*>* impactGenerators;
@end

@implementation TapticEnginePlugin

static TapticEnginePlugin * _shared;

+ (TapticEnginePlugin*) shared {
    @synchronized(self) {
        if(_shared == nil) {
            _shared = [[self alloc] init];
        }
    }
    return _shared;
}

- (id) init {
    NSLog(@"taptic_engine -- init");
    if (self = [super init])
    {        
        self.notificationGenerator = [UINotificationFeedbackGenerator new];
        [self.notificationGenerator prepare];
        
        self.selectionGenerator = [UISelectionFeedbackGenerator new];
        [self.selectionGenerator prepare];

        self.impactGenerators = @[
             [[UIImpactFeedbackGenerator alloc] initWithStyle:UIImpactFeedbackStyleLight],
             [[UIImpactFeedbackGenerator alloc] initWithStyle:UIImpactFeedbackStyleMedium],
             [[UIImpactFeedbackGenerator alloc] initWithStyle:UIImpactFeedbackStyleHeavy],
        ];
        for(UIImpactFeedbackGenerator* impact in self.impactGenerators) {
            [impact prepare];
        }
    }
    return self;
}

- (void) dealloc {
    self.notificationGenerator = NULL;
    self.selectionGenerator = NULL;
    self.impactGenerators = NULL;
}

- (void) notification:(UINotificationFeedbackType)type {
    [self.notificationGenerator notificationOccurred:type];
}

- (void) selection {
    [self.selectionGenerator selectionChanged];
}

- (void) impact:(UIImpactFeedbackStyle)style {
    NSLog(@"taptic_engine -- impact invoked");
    [self.impactGenerators[(int) style] impactOccurred];
}

+ (BOOL) isSupport {
    if ([UINotificationFeedbackGenerator class]) {
        return YES;
    }
    return NO;
}

@end

bool TapticEngine_IsSupported() {
    return [TapticEnginePlugin isSupport];
}

void TapticEngine_Impact(ImpactStyle style) {
    if (TapticEngine_IsSupported()) {
        [[TapticEnginePlugin shared] impact:(UIImpactFeedbackStyle) style];
    }
}

void TapticEngine_Notification(NotificationType type) {
    if (TapticEngine_IsSupported()) {
        [[TapticEnginePlugin shared] notification:(UINotificationFeedbackType) type];
    }
}

void TapticEngine_Selection() {
    if (TapticEngine_IsSupported()) {
        [[TapticEnginePlugin shared] selection];
    }
}
