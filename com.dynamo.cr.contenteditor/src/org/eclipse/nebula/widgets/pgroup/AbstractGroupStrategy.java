/*******************************************************************************
 * Copyright (c) 2006 Chris Gross. All rights reserved. This program and the
 * accompanying materials are made available under the terms of the Eclipse
 * Public License v1.0 which accompanies this distribution, and is available at
 * http://www.eclipse.org/legal/epl-v10.html 
 * 
 * Contributors: schtoo@schtoo.com(Chris Gross) - initial API and implementation
 ******************************************************************************/

package org.eclipse.nebula.widgets.pgroup;

import org.eclipse.swt.graphics.GC;
import org.eclipse.swt.graphics.Rectangle;

/**
 * AbstractGroupStrategy is a convenient starting point for all
 * IGroupStrategy's.
 * <P>
 * The AbstractGroupStrategy handles most behavior for you. All that is required
 * of extending classes, is to implement painting and sizing.
 * 
 * @author chris
 */
public abstract class AbstractGroupStrategy
{

    private PGroup group;

    /*
     * (non-Javadoc)
     * 
     * @see com.swtplus.widgets.IGroupStrategy#initialize(com.swtplus.widgets.PGroup)
     */
    public void initialize(PGroup g)
    {
        group = g;

        update();
    }

    public boolean isToggleLocation(int x, int y)
    {
        if (getGroup().getToggleRenderer() != null)
        {
            Rectangle r = new Rectangle(getGroup().getToggleRenderer().getBounds().x, getGroup()
                .getToggleRenderer().getBounds().y,
                                        getGroup().getToggleRenderer().getBounds().width,
                                        getGroup().getToggleRenderer().getBounds().height);
            if (r.contains(x, y))
            {
                return true;
            }
        }
        return false;
    }

    /**
     * Paints the actual group widget. This method is to be implemented by
     * extending classes.
     * 
     * @param gc
     */
    public abstract void paint(GC gc);

    public abstract void dispose();

    /**
     * @return Returns the PGroup.
     */
    public PGroup getGroup()
    {
        return group;
    }

    public abstract Rectangle computeTrim(int x, int y, int width, int height);

    public abstract Rectangle getClientArea();

    public abstract void update();
}
