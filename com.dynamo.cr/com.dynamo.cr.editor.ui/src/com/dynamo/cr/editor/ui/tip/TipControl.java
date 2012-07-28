package com.dynamo.cr.editor.ui.tip;

import org.eclipse.jface.layout.GridDataFactory;
import org.eclipse.swt.SWT;
import org.eclipse.swt.events.MouseEvent;
import org.eclipse.swt.events.MouseListener;
import org.eclipse.swt.events.MouseTrackAdapter;
import org.eclipse.swt.events.PaintEvent;
import org.eclipse.swt.events.PaintListener;
import org.eclipse.swt.graphics.Color;
import org.eclipse.swt.graphics.Font;
import org.eclipse.swt.graphics.FontData;
import org.eclipse.swt.graphics.Point;
import org.eclipse.swt.graphics.RGB;
import org.eclipse.swt.layout.GridLayout;
import org.eclipse.swt.program.Program;
import org.eclipse.swt.widgets.Canvas;
import org.eclipse.swt.widgets.Composite;
import org.eclipse.swt.widgets.Control;
import org.eclipse.swt.widgets.Display;
import org.eclipse.swt.widgets.Event;
import org.eclipse.swt.widgets.Label;
import org.eclipse.swt.widgets.Link;
import org.eclipse.swt.widgets.Listener;

public class TipControl extends Composite {

    class CloseButton extends Canvas {
        private boolean highlight = false;

        public CloseButton(Composite parent, int style) {
            super(parent, style);

            addMouseTrackListener(new MouseTrackAdapter() {
                @Override
                public void mouseEnter(MouseEvent e) {
                    highlight = true;
                    redraw();
                }

                @Override
                public void mouseExit(MouseEvent e) {
                    highlight = false;
                    redraw();
                }
            });

            addPaintListener(new PaintListener() {
                @Override
                public void paintControl(PaintEvent e) {
                    int shrink = 4;
                    Display display = Display.getDefault();
                    if (highlight) {
                        e.gc.setForeground(display.getSystemColor(SWT.COLOR_GRAY));
                    } else {
                        e.gc.setForeground(display.getSystemColor(SWT.COLOR_BLACK));
                    }
                    e.gc.drawLine(shrink, shrink, 15 - shrink, 15 - shrink);
                    e.gc.drawLine(shrink, 15 - shrink, 15 - shrink, shrink);
                }
            });

            addMouseListener(new MouseListener() {
                @Override
                public void mouseUp(MouseEvent e) {}

                @Override
                public void mouseDown(MouseEvent e) {
                    tipManager.hideTip(tip);
                }

                @Override
                public void mouseDoubleClick(MouseEvent e) {}
            });
        }

        @Override
        public Point computeSize(int wHint, int hHint, boolean changed) {
            return new Point(16, 16);
        }
    }

    private ITipManager tipManager;
    private Label tipLabel;
    private Tip tip;
    private Link link;
    private Label title;

    public TipControl(ITipManager tipManager, Composite parent, int style) {
        super(parent, style);
        this.tipManager = tipManager;
        Color color = new Color(Display.getDefault(), new RGB(252, 248, 227));
        TipControl tipComposite = this;

        tipComposite.setBackground(color);
        GridLayout layout = new GridLayout(2, false);
        layout.marginLeft = layout.marginRight = layout.marginTop = layout.marginBottom = 6;
        layout.marginWidth = 0;
        layout.marginHeight = 0;
        layout.verticalSpacing = 0;
        layout.horizontalSpacing = 0;
        tipComposite.setLayout(layout);

        title = new Label(tipComposite, SWT.WRAP);
        title.setBackground(color);
        title.setText("");
        title.setLayoutData(GridDataFactory.fillDefaults().grab(true, true).align(SWT.FILL, SWT.TOP).create());
        FontData[] boldFontData = title.getFont().getFontData();
        boldFontData[0].setStyle(SWT.BOLD);
        title.setFont( new Font(Display.getDefault(), boldFontData[0]));

        Control closeButton = new CloseButton(tipComposite, SWT.NONE);
        closeButton.setBackground(color);
        closeButton.setLayoutData(GridDataFactory.fillDefaults().grab(false, false).align(SWT.CENTER, SWT.TOP).create());

        tipLabel = new Label(tipComposite, SWT.WRAP);
        tipLabel.setBackground(color);
        tipLabel.setText("");
        tipLabel.setLayoutData(GridDataFactory.fillDefaults().grab(true, true).align(SWT.FILL, SWT.TOP).span(2, 1).create());

        link = new Link(tipComposite, SWT.NONE);
        link.setText("");
        link.setBackground(color);
        link.addListener (SWT.Selection, new Listener () {
            public void handleEvent(Event event) {
                Program.launch(event.text);
            }
        });
        link.setLayoutData(GridDataFactory.fillDefaults().grab(false, false).align(SWT.BEGINNING, SWT.CENTER).create());
    }

    public void setTip(Tip tip) {
        this.tip = tip;
        title.setText(tip.getTitle());
        String message = tip.getTip().replace("\\n", "\n");
        tipLabel.setText(message);
        if (tip.getLink() != null && tip.getLink().length() > 0) {
            link.setText(tip.getLink());
        }
    }
}
