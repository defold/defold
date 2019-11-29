#include "BaseView.h"

@interface ViewController : UIViewController<UIContentContainer, UIAccelerometerDelegate>
{
    BaseView* baseView;
    CGSize cachedViewSize;
}

- (void)createView:(BOOL)recreate;

@property (nonatomic, retain) IBOutlet BaseView* baseView;

@end
