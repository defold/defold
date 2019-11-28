#import <UIKit/UIKit.h>

@interface AppDelegateProxy: NSObject

// due to bootstrap issues, we need to invoke the delegates via this class method for now
+ (BOOL) application:(UIApplication *)application willFinishLaunchingWithOptions:(NSDictionary *)launchOptions;

- (BOOL)application:(UIApplication *)application willFinishLaunchingWithOptions:(NSDictionary *)launchOptions;
- (BOOL)application:(UIApplication *)application didFinishLaunchingWithOptions:(NSDictionary *)launchOptions;

@end
