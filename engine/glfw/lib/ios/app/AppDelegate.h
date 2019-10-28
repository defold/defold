#import <UIKit/UIKit.h>

@interface AppDelegate : NSObject <UIApplicationDelegate>
{
    UIWindow *window;
}

- (void)reinit:(UIApplication *)application;
- (void)appUpdate;

@property (nonatomic, retain) IBOutlet UIWindow *window;

@end
