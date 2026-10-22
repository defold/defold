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

#include "native/native.h"

#include "native/native_handles.h"
#import "native/ios/app/ViewController.h"

#include <assert.h>
#include <math.h>
#include <string.h>
#include <dlib/array.h>
#include <dlib/log.h>
#include <UIKit/UIKit.h>

#include "platform_window_ios.h"

namespace dmPlatform
{
    id GetiOSUIWindow()
    {
        return dmNativeGetiOSUIWindow();
    }

    id GetiOSUIView()
    {
        return dmNativeGetiOSUIView();
    }

    id GetiOSEAGLContext()
    {
        return dmNativeGetiOSEAGLContext();
    }

    void SetiOSHomeIndicatorMode(const char* mode)
    {
        assert([NSThread isMainThread]);

        HomeIndicatorMode indicator_mode = HOME_INDICATOR_MODE_DEFERRED;
        if (strcmp(mode, "system") == 0)
            indicator_mode = HOME_INDICATOR_MODE_SYSTEM;
        else if (strcmp(mode, "auto_hide") == 0)
            indicator_mode = HOME_INDICATOR_MODE_AUTO_HIDE;
        else if (strcmp(mode, "deferred") != 0)
            dmLogWarning("Unknown ios.home_indicator_mode '%s', defaulting to deferred", mode);

        UIWindow* window = (UIWindow*)dmNativeGetiOSUIWindow();
        ViewController* controller = (ViewController*)window.rootViewController;
        controller.homeIndicatorMode = indicator_mode;
    }

    bool GetSafeAreaiOS(HWindow window, WindowSafeArea* out)
    {
        UIView* view = (UIView*)dmNativeGetiOSUIView();
        if (!view || ![view respondsToSelector:@selector(safeAreaInsets)])
        {
            return false;
        }

        UIEdgeInsets insets = view.safeAreaInsets;
        const uint32_t window_width = GetWindowWidth(window);
        const uint32_t window_height = GetWindowHeight(window);

        // Convert UIKit points to pixel units to match window.get_size().
        CGFloat scale = view.contentScaleFactor;
        if (view.bounds.size.width > 0.0f)
        {
            scale = (CGFloat)window_width / view.bounds.size.width;
        }

        const int32_t inset_left = (int32_t)lrint(insets.left * scale);
        const int32_t inset_right = (int32_t)lrint(insets.right * scale);
        const int32_t inset_top = (int32_t)lrint(insets.top * scale);
        const int32_t inset_bottom = (int32_t)lrint(insets.bottom * scale);

        out->m_X = inset_left;
        out->m_Y = inset_bottom;
        out->m_Width = window_width - inset_left - inset_right;
        out->m_Height = window_height - inset_top - inset_bottom;
        out->m_InsetLeft = inset_left;
        out->m_InsetTop = inset_top;
        out->m_InsetRight = inset_right;
        out->m_InsetBottom = inset_bottom;

        return true;
    }

    void GetReservedAreas(HWindow window, dmArray<WindowReservedArea>& areas)
    {
        areas.SetSize(0);
        if (@available(iOS 27.1, *))
        {
            UIView* view = (UIView*)dmNativeGetiOSUIView();
            if (!view || !view.window)
                return;

            NSArray<UIViewReservedRegion*>* regions = [view reservedRegionsOfKind:[UIViewReservedRegionKind occlusionRegionKind]
                                                                         options:UIViewReservedRegionQueryOptionsNone];
            if (areas.Capacity() < regions.count)
                areas.SetCapacity((uint32_t)regions.count);

            const CGRect bounds = view.bounds;
            for (UIViewReservedRegion* region in regions)
            {
                if (!region.isActive)
                    continue;

                // UIKit's frame already includes the interaction margins.
                const CGRect frame = region.frame;
                WindowReservedArea area;
                if (ConvertReservedArea(CGRectGetMinX(frame) - bounds.origin.x, CGRectGetMinY(frame) - bounds.origin.y,
                                        CGRectGetMaxX(frame) - bounds.origin.x, CGRectGetMaxY(frame) - bounds.origin.y,
                                        bounds.size.width, bounds.size.height, GetWindowWidth(window), GetWindowHeight(window), &area))
                    areas.Push(area);
            }
        }
    }

}
