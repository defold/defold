package com.dynamo.cr.sceneed.core.util;

import org.eclipse.swt.SWT;
import org.eclipse.swt.events.MouseEvent;

import com.dynamo.cr.sceneed.core.SceneUtil;
import com.dynamo.cr.sceneed.core.SceneUtil.MouseType;

public class CameraUtil {
    public static enum Movement {
        IDLE,
        ROTATE,
        TRACK, // panning
        DOLLY, // zoom
    }

    public static Movement getMovement(MouseEvent event) {
        MouseType mouseType = SceneUtil.getMouseType();
        switch (mouseType) {
        case ONE_BUTTON:
            if (event.button == 1) {
                if (event.stateMask == (SWT.ALT | SWT.CTRL)) {
                    return Movement.TRACK;
                } else if (event.stateMask == SWT.ALT) {
                    return Movement.ROTATE;
                } else if (event.stateMask == SWT.CTRL) {
                    return Movement.DOLLY;
                }
            }
            break;
        case THREE_BUTTON:
            if (event.stateMask == SWT.ALT) {
                if (event.button == 2) {
                    return Movement.TRACK;
                } else if (event.button == 1) {
                    return Movement.ROTATE;
                } else if (event.button == 3) {
                    return Movement.DOLLY;
                }
            }
            break;
        }
        return Movement.IDLE;
    }
}
