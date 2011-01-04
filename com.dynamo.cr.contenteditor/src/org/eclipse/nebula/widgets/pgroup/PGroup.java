/*******************************************************************************
 * Copyright (c) 2006 Chris Gross.
 * All rights reserved. This program and the accompanying materials
 * are made available under the terms of the Eclipse Public License v1.0
 * which accompanies this distribution, and is available at
 * http://www.eclipse.org/legal/epl-v10.html
 *
 * Contributors:
 *    schtoo@schtoo.com (Chris Gross) - initial API and implementation
 *******************************************************************************/ 

package org.eclipse.nebula.widgets.pgroup;

import org.eclipse.swt.SWT;
import org.eclipse.swt.SWTException;
import org.eclipse.swt.events.DisposeEvent;
import org.eclipse.swt.events.DisposeListener;
import org.eclipse.swt.events.ExpandListener;
import org.eclipse.swt.events.PaintEvent;
import org.eclipse.swt.events.PaintListener;
import org.eclipse.swt.graphics.Color;
import org.eclipse.swt.graphics.Font;
import org.eclipse.swt.graphics.Image;
import org.eclipse.swt.graphics.Point;
import org.eclipse.swt.graphics.Rectangle;
import org.eclipse.swt.widgets.Canvas;
import org.eclipse.swt.widgets.Composite;
import org.eclipse.swt.widgets.Control;
import org.eclipse.swt.widgets.Event;
import org.eclipse.swt.widgets.Listener;
import org.eclipse.swt.widgets.TypedListener;
import org.eclipse.swt.widgets.Widget;

/**
 * Instances of this class provide a decorated border as well as an optional toggle to expand and 
 * collapse the control.
 * <p>
 * This widget is customizable through alternative <code>AbstractGroupStrategy</code>s. Each 
 * strategy determines the size and appearance of the widget.  
 * <p>
 * <dl>
 * <dt><b>Styles:</b></dt>
 * <dd>SMOOTH</dd>
 * <dt><b>Events:</b></dt>
 * <dd>Expand, Collapse</dd>
 * </dl>
 * 
 * @author cgross
 */
public class PGroup extends Canvas
{

    private AbstractGroupStrategy strategy;

    private Image image;

    private String text = "";

    private Font initialFont;
    
    private int imagePosition = SWT.LEAD;

    private int togglePosition = SWT.TRAIL;

    private int linePosition = SWT.CENTER;

    private boolean expanded = true;

    private boolean overToggle = false;
    
    private AbstractRenderer toggleRenderer;
    
    private Color backgroundColor;

    private static int checkStyle(int style)
    {
        int mask = SWT.LEFT_TO_RIGHT | SWT.RIGHT_TO_LEFT | SWT.SMOOTH;
        return (style & mask) | SWT.DOUBLE_BUFFERED;
    }
    
    /**
     * Constructs a new instance of this class given its parent
     * and a style value describing its behavior and appearance.
     * <p>
     * The style value is either one of the style constants defined in
     * class <code>SWT</code> which is applicable to instances of this
     * class, or must be built by <em>bitwise OR</em>'ing together 
     * (that is, using the <code>int</code> "|" operator) two or more
     * of those <code>SWT</code> style constants. The class description
     * lists the style constants that are applicable to the class.
     * Style bits are also inherited from superclasses.
     * </p>
     * <p>
     * To ensure that the color of corners is equal to one of the underlying control
     * invoke the parent composites {@link Composite#setBackgroundMode(int)}
     * with {@link SWT#INHERIT_DEFAULT} or {@link SWT#INHERIT_DEFAULT}
     * </p>
     * @param parent a composite control which will be the parent of the new instance (cannot be null)
     * @param style the style of control to construct
     *
     * @exception IllegalArgumentException <ul>
     *    <li>ERROR_NULL_ARGUMENT - if the parent is null</li>
     * </ul>
     * @exception SWTException <ul>
     *    <li>ERROR_THREAD_INVALID_ACCESS - if not called from the thread that created the parent</li>
     *    <li>ERROR_INVALID_SUBCLASS - if this class is not an allowed subclass</li>
     * </ul>
     *
     * @see SWT
     * @see Widget#checkSubclass
     * @see Widget#getStyle
     */
    public PGroup(Composite parent, int style)
    {
        super(parent, checkStyle(style));
        setStrategy(new RectangleGroupStrategy());
        setToggleRenderer(new ChevronsToggleRenderer());

        initialFont = new Font(getDisplay(), getFont().getFontData()[0].getName(), getFont()
            .getFontData()[0].getHeight(), SWT.BOLD);
        super.setFont(initialFont);

        strategy.initialize(this);

        initListeners();
    }

    /** 
     * {@inheritDoc}
     */
    public Color getBackground()
    {
        checkWidget();
        if (backgroundColor == null)
            return super.getBackground();
        return backgroundColor;
    }
    
    Color internalGetBackground()
    {
        return backgroundColor;
    }

	/**
 	 * Sets the receiver's background color to the color specified
  	 * by the argument, or to the default system color for the control
     * if the argument is null.
     * <p>
     * Note: This operation is a hint and may be overridden by the platform.
     * For example, on Windows the background of a Button cannot be changed.
     * </p>
     * <p>
     * To ensure that the color of corners is equal to one of the underlying control
     * invoke the parent composites {@link Composite#setBackgroundMode(int)}
     * with {@link SWT#INHERIT_DEFAULT} or {@link SWT#INHERIT_DEFAULT}
     * </p>
     * <p>
     * Note: If a new strategy is set on the receiver it may overwrite the existing
     * background color.
     * </p>
     * @param color the new color (or null)
     *
     * @exception IllegalArgumentException <ul>
     *    <li>ERROR_INVALID_ARGUMENT - if the argument has been disposed</li> 
     * </ul>
     * @exception SWTException <ul>
     *    <li>ERROR_WIDGET_DISPOSED - if the receiver has been disposed</li>
     *    <li>ERROR_THREAD_INVALID_ACCESS - if not called from the thread that created the receiver</li>
     * </ul>
     */
    public void setBackground(Color color)
    {
        checkWidget();
        backgroundColor = color;
        redraw();
    }
    
    /**
     * Returns the toggle renderer or <code>null</code>.
     * 
     * @exception SWTException <ul>
     *    <li>ERROR_WIDGET_DISPOSED - if the receiver has been disposed</li>
     *    <li>ERROR_THREAD_INVALID_ACCESS - if not called from the thread that created the receiver</li>
     * </ul>
     */
    public AbstractRenderer getToggleRenderer()
    {
        checkWidget();
        return toggleRenderer;
    }



    /**
     * Sets the toggle renderer.  If the toggle renderer is set to <code>null</code> the control 
     * will not show a toggle or allow the user to expand/collapse the group by clicking on the title.
     * 
     * @param toggleRenderer the toggleRenderer to set or <code>null</code>
     * 
     * @exception SWTException <ul>
     *    <li>ERROR_WIDGET_DISPOSED - if the receiver has been disposed</li>
     *    <li>ERROR_THREAD_INVALID_ACCESS - if not called from the thread that created the receiver</li>
     * </ul>
     */
    public void setToggleRenderer(AbstractRenderer toggleRenderer)
    {
        checkWidget();
        this.toggleRenderer = toggleRenderer;
        strategy.update();
        redraw();
    }



    private void initListeners()
    {
        this.addDisposeListener(new DisposeListener()
        {
            public void widgetDisposed(DisposeEvent e)
            {
                onDispose();
            }
        });
        
        addPaintListener(new PaintListener()
        {
            public void paintControl(PaintEvent e)
            {
                onPaint(e);
            }
        });

        addListener(SWT.Traverse, new Listener()
        {
            public void handleEvent(Event e)
            {
                switch (e.detail)
                {
                    /* Do tab group traversal */
                    case SWT.TRAVERSE_ESCAPE :
                    case SWT.TRAVERSE_RETURN :
                    case SWT.TRAVERSE_TAB_NEXT :
                    case SWT.TRAVERSE_TAB_PREVIOUS :
                    case SWT.TRAVERSE_PAGE_NEXT :
                    case SWT.TRAVERSE_PAGE_PREVIOUS :
                        e.doit = true;
                        break;
                }
            }
        });

        addListener(SWT.FocusIn, new Listener()
        {
            public void handleEvent(Event e)
            {
                redraw();
            }
        });
        addListener(SWT.FocusOut, new Listener()
        {
            public void handleEvent(Event e)
            {
                redraw();
            }
        });

        addListener(SWT.KeyDown, new Listener()
        {
            public void handleEvent(Event e)
            {
                onKeyDown(e);
            }
        });

        addListener(SWT.MouseExit, new Listener()
        {
            public void handleEvent(Event e)
            {
                onMouseExit(e);
            }
        });

        addListener(SWT.MouseMove, new Listener()
        {
            public void handleEvent(Event e)
            {
                onMouseMove(e);
            }
        });

        addListener(SWT.MouseDown, new Listener()
        {
            public void handleEvent(Event e)
            {
                onMouseDown(e);
            }
        });

    }
    
    private void onPaint(PaintEvent e)
    {
        Color back = e.gc.getBackground();
        Color fore = e.gc.getForeground();

        strategy.paint(e.gc);
        
        e.gc.setBackground(back);
        e.gc.setForeground(fore);
        
        if (toggleRenderer != null)
        {
            toggleRenderer.setExpanded(expanded);
            toggleRenderer.setFocus(isFocusControl());
            toggleRenderer.setHover(overToggle);
            toggleRenderer.paint(e.gc, this);
        }
    }

    private void onKeyDown(Event e)
    {
        if (e.character == ' ' || e.character == '\r')
        {
            setExpanded(!getExpanded());
            if (expanded)
            {
                notifyListeners(SWT.Expand, new Event());
            }
            else
            {
                notifyListeners(SWT.Collapse, new Event());
            }            
        }
    }
    
    private void onMouseMove(Event e)
    {
        boolean newOverToggle = PGroup.this.strategy.isToggleLocation(e.x, e.y);
        if (newOverToggle != overToggle)
        {
            if (newOverToggle)
            {
                setCursor(getDisplay().getSystemCursor(SWT.CURSOR_HAND));
            }
            else
            {
                setCursor(null);
            }
            overToggle = newOverToggle;
            redraw();
        }
    }
    
    private void onMouseExit(Event e)
    {
        setCursor(null);
        overToggle = false;
        redraw();
    }
    
    private void onMouseDown(Event e)
    {
        if (overToggle && e.button == 1)
        {
            setExpanded(!expanded);
            setFocus();
            if (expanded)
            {
                notifyListeners(SWT.Expand, new Event());
            }
            else
            {
                notifyListeners(SWT.Collapse, new Event());
            } 
        }
    }
    
    /**
     * Returns the strategy.
     * 
     * @exception SWTException <ul>
     *    <li>ERROR_WIDGET_DISPOSED - if the receiver has been disposed</li>
     *    <li>ERROR_THREAD_INVALID_ACCESS - if not called from the thread that created the receiver</li>
     * </ul>
     */
    public AbstractGroupStrategy getStrategy()
    {
        checkWidget();
        return strategy;
    }

    /**
     * Sets the strategy.
     * 
     * @param strategy the strategy to set
     * 
     * @exception IllegalArgumentException <ul>
     *    <li>ERROR_NULL_ARGUMENT - if the strategy is null</li>
     * </ul>
     * @exception SWTException <ul>
     *    <li>ERROR_WIDGET_DISPOSED - if the receiver has been disposed</li>
     *    <li>ERROR_THREAD_INVALID_ACCESS - if not called from the thread that created the receiver</li>
     * </ul>
     */
    public void setStrategy(AbstractGroupStrategy strategy)
    {
        checkWidget();
        if (strategy == null)
            SWT.error(SWT.ERROR_NULL_ARGUMENT);
        this.strategy = strategy;
        setForeground(null);
        strategy.initialize(this);
    }

    /**
     * Returns the image.
     * 
     * @exception SWTException <ul>
     *    <li>ERROR_WIDGET_DISPOSED - if the receiver has been disposed</li>
     *    <li>ERROR_THREAD_INVALID_ACCESS - if not called from the thread that created the receiver</li>
     * </ul>
     */
    public Image getImage()
    {
        checkWidget();
        return image;
    }

    /**
     * Sets the image.
     * 
     * @exception IllegalArgumentException <ul>
     *    <li>ERROR_NULL_ARGUMENT - if the image is disposed</li>
     * </ul>
     * @exception SWTException <ul>
     *    <li>ERROR_WIDGET_DISPOSED - if the receiver has been disposed</li>
     *    <li>ERROR_THREAD_INVALID_ACCESS - if not called from the thread that created the receiver</li>
     * </ul>
     */
    public void setImage(Image image)
    {
        checkWidget();

        if (image != null && image.isDisposed())
            SWT.error(SWT.ERROR_INVALID_ARGUMENT);
        this.image = image;
        strategy.update();
    }

    /*
     * (non-Javadoc)
     * 
     * @see org.eclipse.swt.widgets.Control#setFont(org.eclipse.swt.graphics.Font)
     */
    public void setFont(Font font)
    {
        checkWidget();
        super.setFont(font);
        strategy.update();
        redraw();
    }

    /**
     * Returns the text.
     * 
     * @exception SWTException <ul>
     *    <li>ERROR_WIDGET_DISPOSED - if the receiver has been disposed</li>
     *    <li>ERROR_THREAD_INVALID_ACCESS - if not called from the thread that created the receiver</li>
     * </ul>
     */
    public String getText()
    {
        checkWidget();
        return text;
    }

    /**
     * Sets the text.
     * 
     * @exception IllegalArgumentException <ul>
     *    <li>ERROR_NULL_ARGUMENT - if the text is null</li>
     * </ul>
     * @exception SWTException <ul>
     *    <li>ERROR_WIDGET_DISPOSED - if the receiver has been disposed</li>
     *    <li>ERROR_THREAD_INVALID_ACCESS - if not called from the thread that created the receiver</li>
     * </ul>
     */
    public void setText(String text)
    {
        checkWidget();
        if (text == null)
            SWT.error(SWT.ERROR_NULL_ARGUMENT);
        this.text = text;
        strategy.update();
        redraw();
    }

    /*
     * (non-Javadoc)
     * 
     * @see org.eclipse.swt.widgets.Control#computeSize(int, int, boolean)
     */
    public Point computeSize(int arg0, int arg1, boolean arg2)
    {
        checkWidget();
        if (getExpanded())
            return super.computeSize(arg0, arg1, arg2);

        Rectangle trim = strategy.computeTrim(0, 0, 0, 0);
        trim.width = super.computeSize(arg0, arg1, arg2).x;
        return new Point(trim.width, Math.max(trim.height, arg1));
    }

    /**
     * Returns the expanded/collapsed state.
     * 
     * @exception SWTException <ul>
     *    <li>ERROR_WIDGET_DISPOSED - if the receiver has been disposed</li>
     *    <li>ERROR_THREAD_INVALID_ACCESS - if not called from the thread that created the receiver</li>
     * </ul>     
     **/
    public boolean getExpanded()
    {
        checkWidget();
        return expanded;
    }

    /**
     * Sets the expanded state of the group.
     * 
     * @exception SWTException <ul>
     *    <li>ERROR_WIDGET_DISPOSED - if the receiver has been disposed</li>
     *    <li>ERROR_THREAD_INVALID_ACCESS - if not called from the thread that created the receiver</li>
     * </ul>
     */
    public void setExpanded(boolean expanded)
    {
        checkWidget();
        Control[] kids = getChildren();
        for (int i = 0; i < kids.length; i++)
        {
            kids[i].setVisible(expanded);
        }
        this.expanded = expanded;
        getParent().layout();
        redraw();
    }

    private void onDispose()
    {
        strategy.dispose();
        if (initialFont != null)
            initialFont.dispose();
    }

    /**
     * Adds the listener to the collection of listeners who will
     * be notified when the receiver is expanded or collapsed
     * by sending it one of the messages defined in the <code>ExpandListener</code>
     * interface.
     *
     * @param listener the listener which should be notified
     *
     * @exception IllegalArgumentException <ul>
     *    <li>ERROR_NULL_ARGUMENT - if the listener is null</li>
     * </ul>
     * @exception SWTException <ul>
     *    <li>ERROR_WIDGET_DISPOSED - if the receiver has been disposed</li>
     *    <li>ERROR_THREAD_INVALID_ACCESS - if not called from the thread that created the receiver</li>
     * </ul>
     *
     * @see ExpandListener
     * @see #removeExpandListener
     */
    public void addExpandListener(ExpandListener listener)
    {
        checkWidget();
        if (listener == null)
            SWT.error(SWT.ERROR_NULL_ARGUMENT);

        TypedListener typedListener = new TypedListener(listener);
        addListener(SWT.Expand, typedListener);
        addListener(SWT.Collapse, typedListener);
    }

    /**
     * Removes the listener from the collection of listeners who will
     * be notified when the receiver are expanded or collapsed.
     *
     * @param listener the listener which should no longer be notified
     *
     * @exception IllegalArgumentException <ul>
     *    <li>ERROR_NULL_ARGUMENT - if the listener is null</li>
     * </ul>
     * @exception SWTException <ul>
     *    <li>ERROR_WIDGET_DISPOSED - if the receiver has been disposed</li>
     *    <li>ERROR_THREAD_INVALID_ACCESS - if not called from the thread that created the receiver</li>
     * </ul>
     *
     * @see ExpandListener
     * @see #addExpandListener
     */
    public void removeExpandListener(ExpandListener listener)
    {
        checkWidget();
        if (listener == null)
            SWT.error(SWT.ERROR_NULL_ARGUMENT);

        removeListener(SWT.Expand, listener);
        removeListener(SWT.Collapse, listener);
    }

    /**
     * Returns the image position.  The image position is a combination of integer constants OR'ed 
     * together.
     * 
     * @exception SWTException <ul>
     *    <li>ERROR_WIDGET_DISPOSED - if the receiver has been disposed</li>
     *    <li>ERROR_THREAD_INVALID_ACCESS - if not called from the thread that created the receiver</li>
     * </ul>
     */
    public int getImagePosition()
    {
        checkWidget();
        return imagePosition;
    }

    /**
     * Sets the image position.  The image position is a combination of integer constants OR'ed 
     * together.  Valid values are <code>SWT.LEFT</code> and <code>SWT.RIGHT</code> (mutually exclusive).  
     * <code>SWT.TOP</code> is hint interpreted by some strategies.
     *   
     * @exception IllegalArgumentException <ul>
     *    <li>ERROR_NULL_ARGUMENT - if the strategy is null</li>
     * </ul>
     * @exception SWTException <ul>
     *    <li>ERROR_WIDGET_DISPOSED - if the receiver has been disposed</li>
     *    <li>ERROR_THREAD_INVALID_ACCESS - if not called from the thread that created the receiver</li>
     * </ul>
     */
    public void setImagePosition(int imagePosition)
    {
        checkWidget();
        this.imagePosition = imagePosition;
        strategy.update();
        redraw();
    }

    /**
     * Returns the toggle position.  The toggle position is a combination of integer constants OR'ed 
     * together.
     * 
     * @exception SWTException <ul>
     *    <li>ERROR_WIDGET_DISPOSED - if the receiver has been disposed</li>
     *    <li>ERROR_THREAD_INVALID_ACCESS - if not called from the thread that created the receiver</li>
     * </ul>
     */
    public int getTogglePosition()
    {
        checkWidget();
        return togglePosition;
    }

    /**
     * Sets the toggle position.  The toggle position is a combination of integer constants OR'ed 
     * together.  Valid values are <code>SWT.LEFT</code> and <code>SWT.RIGHT</code> (mutually exclusive).  
     *   
     * @exception SWTException <ul>
     *    <li>ERROR_WIDGET_DISPOSED - if the receiver has been disposed</li>
     *    <li>ERROR_THREAD_INVALID_ACCESS - if not called from the thread that created the receiver</li>
     * </ul>
     */
    public void setTogglePosition(int togglePosition)
    {
        checkWidget();
        this.togglePosition = togglePosition;
        strategy.update();
        redraw();
    }

    /**
     * Returns the line position.  The line position is a combination of integer constants OR'ed 
     * together.
     * 
     * @exception SWTException <ul>
     *    <li>ERROR_WIDGET_DISPOSED - if the receiver has been disposed</li>
     *    <li>ERROR_THREAD_INVALID_ACCESS - if not called from the thread that created the receiver</li>
     * </ul>
     */
    public int getLinePosition()
    {
        checkWidget();
        return linePosition;
    }

    /**
     * Sets the line position.  The line position is a combination of integer constants OR'ed 
     * together.  Valid values are <code>SWT.BOTTOM</code> and <code>SWT.CENTER</code> (mutually exclusive).  
     *   
     * @exception SWTException <ul>
     *    <li>ERROR_WIDGET_DISPOSED - if the receiver has been disposed</li>
     *    <li>ERROR_THREAD_INVALID_ACCESS - if not called from the thread that created the receiver</li>
     * </ul>
     */
    public void setLinePosition(int linePosition)
    {
        checkWidget();
        this.linePosition = linePosition;
        strategy.update();
        redraw();
    }

    /**
     * {@inheritDoc}
     */
    public Rectangle computeTrim(int x, int y, int width, int height)
    {
        checkWidget();
        return strategy.computeTrim(x, y, width, height);
    }

    /**
     * {@inheritDoc}
     */
    public Rectangle getClientArea()
    {
        checkWidget();
        if (getExpanded())
            return strategy.getClientArea();

        return new Rectangle(-10, 0, 0, 0);
    }

    /**
     * Sets the receiver's foreground color to the color specified
     * by the argument, or to the default system color for the control
     * if the argument is null.
     * <p>
     * Note: This operation is a hint and may be overridden by the platform.
     * </p>
     * <p>
     * Note: If a new strategy is set on the receiver it may overwrite the existing
     * foreground color.
     * </p>
     * @param color the new color (or null)
     *
     * @exception IllegalArgumentException <ul>
     *    <li>ERROR_INVALID_ARGUMENT - if the argument has been disposed</li> 
     * </ul>
     * @exception SWTException <ul>
     *    <li>ERROR_WIDGET_DISPOSED - if the receiver has been disposed</li>
     *    <li>ERROR_THREAD_INVALID_ACCESS - if not called from the thread that created the receiver</li>
     * </ul>
     */
	public void setForeground(Color color) {
		super.setForeground(color);
	}

}
