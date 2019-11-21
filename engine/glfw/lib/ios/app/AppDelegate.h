#import <UIKit/UIKit.h>
#import "AppDelegateProxy.h"

@interface AppDelegate : NSObject <UIApplicationDelegate>
{
@private
    UIWindow* window;
}

- (void)reinit:(UIApplication*)application;
- (void)appUpdate;

@property (nonatomic, retain) IBOutlet UIWindow* window;

@end
