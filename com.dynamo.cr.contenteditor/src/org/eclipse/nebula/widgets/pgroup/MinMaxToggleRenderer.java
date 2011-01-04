/*******************************************************************************
 * Copyright (c) 2006 Chris Gross. All rights reserved. This program and the
 * accompanying materials are made available under the terms of the Eclipse
 * Public License v1.0 which accompanies this distribution, and is available at
 * http://www.eclipse.org/legal/epl-v10.html Contributors: schtoo@schtoo.com
 * (Chris Gross) - initial API and implementation
 ******************************************************************************/

package org.eclipse.nebula.widgets.pgroup;

import org.eclipse.swt.SWT;
import org.eclipse.swt.graphics.GC;
import org.eclipse.swt.graphics.Point;
import org.eclipse.swt.graphics.Transform;

/**
 * This toggle strategy mimics the buttons found in SWT's CTabFolder (i.e. the
 * same buttons found in the Eclipse's views and editors) which in turn are
 * mimicing the minimize/maximize buttons found on Shells. The expanded image is
 * a minimize image. The collapsed image is a maximize image.
 * 
 * @author chris
 */
public class MinMaxToggleRenderer extends AbstractRenderer
{

    /**
     * 
     */
    public MinMaxToggleRenderer()
    {
        super();
        setSize(new Point(18, 18));
    }

    public void paint(GC gc, Object value)
    {

        Transform transform = new Transform(gc.getDevice());
        transform.translate(getBounds().x, getBounds().y);
        gc.setTransform(transform);

        gc.setBackground(gc.getDevice().getSystemColor(SWT.COLOR_LIST_BACKGROUND));
        gc.setForeground(gc.getDevice().getSystemColor(SWT.COLOR_WIDGET_NORMAL_SHADOW));

        if (isHover())
        {
            gc.setForeground(gc.getDevice().getSystemColor(SWT.COLOR_WIDGET_NORMAL_SHADOW));
            // gc.fillRoundRectangle(0,0,16,17,5,5);
            gc.drawRoundRectangle(0, 0, 17, 17, 5, 5);
        }

        gc.setBackground(gc.getDevice().getSystemColor(SWT.COLOR_LIST_BACKGROUND));
        gc.setForeground(gc.getDevice().getSystemColor(SWT.COLOR_WIDGET_NORMAL_SHADOW));

        if (isExpanded())
        {
            gc.fillRectangle(4, 3, 9, 3);
            gc.drawRectangle(4, 3, 9, 3);
        }
        else
        {
            gc.fillRectangle(4, 3, 9, 9);
            gc.drawRectangle(4, 3, 9, 9);
            gc.drawLine(4, 5, 13, 5);
        }

        gc.setTransform(null);
        transform.dispose();

    }

    public Point computeSize(GC gc, int wHint, int hHint, Object value)
    {
        // TODO Auto-generated method stub
        return null;
    }

}
