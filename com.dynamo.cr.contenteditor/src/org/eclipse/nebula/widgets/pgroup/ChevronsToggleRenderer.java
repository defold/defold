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
 * A dual chevron toggle strategy that shows two chevrons facing upwards when
 * expanded (thus clueing the user to click the chevrons to collapse) and two
 * chevrons facing downwards when collapsed. When hovered the chevrons are
 * encircled by a rounded rectangle.
 * 
 * @author chris
 */
public class ChevronsToggleRenderer extends AbstractRenderer
{

    /**
     * 
     */
    public ChevronsToggleRenderer()
    {
        super();
        setSize(new Point(17, 16));
    }

    public void paint(GC gc, Object value)
    {
        Transform transform = new Transform(gc.getDevice());
        transform.translate(getBounds().x, getBounds().y);
        gc.setTransform(transform);

        Color back = gc.getBackground();
        Color fore = gc.getForeground();

        if (isHover())
        {
            Color old = gc.getForeground();
            gc.setForeground(gc.getDevice().getSystemColor(SWT.COLOR_WIDGET_NORMAL_SHADOW));
            gc.drawRoundRectangle(0, 0, 16, 15, 5, 5);
            gc.setForeground(old);
        }

        if (isExpanded())
        {
            gc.drawPolygon(new int[] {5, 6, 8, 3, 11, 6, 10, 6, 8, 4, 6, 6 });

            gc.drawPolygon(new int[] {5, 10, 8, 7, 11, 10, 10, 10, 8, 8, 6, 10 });
        }
        else
        {
            gc.drawPolygon(new int[] {5, 4, 8, 7, 11, 4, 10, 4, 8, 6, 6, 4 });

            gc.drawPolygon(new int[] {5, 8, 8, 11, 11, 8, 10, 8, 8, 10, 6, 8 });
        }

        if (isFocus())
        {
            gc.setBackground(back);
            gc.setForeground(fore);
            gc.drawFocus(2, 2, 13, 12);
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
