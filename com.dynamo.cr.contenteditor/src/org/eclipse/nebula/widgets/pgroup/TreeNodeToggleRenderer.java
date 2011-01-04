/*******************************************************************************
 * Copyright (c) 2006 Chris Gross. All rights reserved. This program and the
 * accompanying materials are made available under the terms of the Eclipse
 * Public License v1.0 which accompanies this distribution, and is available at
 * http://www.eclipse.org/legal/epl-v10.html Contributors: schtoo@schtoo.com
 * (Chris Gross) - initial API and implementation
 ******************************************************************************/

package org.eclipse.nebula.widgets.pgroup;

import org.eclipse.swt.SWT;
import org.eclipse.swt.graphics.Color;
import org.eclipse.swt.graphics.GC;
import org.eclipse.swt.graphics.Point;
import org.eclipse.swt.graphics.Transform;

/**
 * This toggle strategy mimics a Windows tree node. That is, it shows a plus
 * (surrounded by a box) when collapsed and a minus when expanded.
 * 
 * @author chris
 */
public class TreeNodeToggleRenderer extends AbstractRenderer
{

    /**
     * 
     */
    public TreeNodeToggleRenderer()
    {
        super();
        setSize(new Point(9, 9));
    }

    public void paint(GC gc, Object value)
    {
        Transform transform = new Transform(gc.getDevice());
        transform.translate(getBounds().x, getBounds().y);
        gc.setTransform(transform);

        Color back = gc.getBackground();
        Color fore = gc.getForeground();

        gc.setBackground(gc.getDevice().getSystemColor(SWT.COLOR_LIST_BACKGROUND));
        gc.fillRectangle(0, 0, 8, 8);

        gc.setForeground(gc.getDevice().getSystemColor(SWT.COLOR_WIDGET_FOREGROUND));
        gc.drawLine(2, 4, 6, 4);
        if (!isExpanded())
        {
            gc.drawLine(4, 2, 4, 6);
        }
        gc.setForeground(gc.getDevice().getSystemColor(SWT.COLOR_WIDGET_NORMAL_SHADOW));
        gc.drawRectangle(0, 0, 8, 8);

        if (isFocus())
        {
            gc.setBackground(back);
            gc.setForeground(fore);
            gc.drawFocus(-1, -1, 11, 11);
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
