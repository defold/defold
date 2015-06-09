
package com.dynamo.cr.editor.splashHandlers;

import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.jface.resource.ImageDescriptor;
import org.eclipse.jface.window.Window;
import org.eclipse.swt.SWT;
import org.eclipse.swt.events.PaintEvent;
import org.eclipse.swt.events.PaintListener;
import org.eclipse.swt.graphics.Color;
import org.eclipse.swt.graphics.GC;
import org.eclipse.swt.graphics.Image;
import org.eclipse.swt.graphics.Point;
import org.eclipse.swt.graphics.Rectangle;
import org.eclipse.swt.layout.FillLayout;
import org.eclipse.swt.widgets.Canvas;
import org.eclipse.swt.widgets.Composite;
import org.eclipse.swt.widgets.Display;
import org.eclipse.swt.widgets.Shell;
import org.eclipse.ui.plugin.AbstractUIPlugin;
import org.eclipse.ui.splash.AbstractSplashHandler;

import com.dynamo.cr.editor.Activator;
import com.dynamo.cr.editor.core.EditorCorePlugin;

public class SplashHandler extends AbstractSplashHandler implements PaintListener {

    private Image progressFg;
    private Image progressBg;
    private Canvas canvas;
    private Monitor monitor;
    private Image ribbon;

    public SplashHandler() {
    }

    private Image loadImage(String path) {
        ImageDescriptor descriptor = AbstractUIPlugin.imageDescriptorFromPlugin(Activator.PLUGIN_ID, path);
        Image image = descriptor.createImage();
        return image;
    }

    public void init(Shell splash) {
        super.init(splash);
        monitor = new Monitor();
        configureUISplash();
        progressFg = loadImage("progress_fg.png");
        progressBg = loadImage("progress_bg.png");
        if (EditorCorePlugin.getDefault().getChannel().equals("beta")) {
            ribbon = loadImage("/icons/launching/beta.png");
        }
    }

    private class Monitor implements IProgressMonitor {

        int totalWork = 1;
        int worked;

        @Override
        public void beginTask(String name, int totalWork) {
            this.totalWork = totalWork;
            this.worked = 0;
        }

        @Override
        public void done() {}

        @Override
        public void internalWorked(double work) {}

        @Override
        public boolean isCanceled() {
            return false;
        }

        @Override
        public void setCanceled(boolean value) {}

        @Override
        public void setTaskName(String name) {}

        @Override
        public void subTask(String name) {}

        @Override
        public void worked(int work) {
            this.worked += work;

            Shell splashShell = getSplash();
            if (splashShell == null) {
                // In Eclipse 4 this one is invoke whenever a bundle change
                // and after the splash is disposed.
                // See org.eclipse.ui.internal.Workbench$StartupProgressBundleListener.bundleChanged
                return;
            }
            Display display = splashShell.getDisplay();

            if (Thread.currentThread() == display.getThread()) {
                if (!canvas.isDisposed()) {
                    canvas.redraw();
                }
            } else {
                Runnable runnable = new Runnable() {

                    @Override
                    public void run() {
                        if (!canvas.isDisposed()) {
                            canvas.redraw();
                        }
                    }
                };
                display.asyncExec(runnable);
            }
        }
    }

    @Override
    public IProgressMonitor getBundleProgressMonitor() {
        return monitor;
    }

    private void configureUISplash() {
        Composite parent = new Composite(getSplash(), Window.getDefaultOrientation());
        Point size = getSplash().getSize();
        parent.setBounds(new Rectangle(0, 0, size.x, size.y));
        parent.setLayout(new FillLayout());
        canvas = new Canvas(parent, SWT.NONE);
        canvas.addPaintListener(this);
        canvas.setBackgroundImage(getSplash().getShell().getBackgroundImage());
        parent.layout();
    }

    public void dispose() {
        super.dispose();
        progressBg.dispose();
        progressFg.dispose();
        if (ribbon != null) {
            ribbon.dispose();
        }
    }

    @Override
    public void paintControl(PaintEvent e) {

        double progress = monitor.worked / (double) monitor.totalWork;
        GC gc = new GC(canvas);
        int width = canvas.getSize().x;
        int progressWidth = progressBg.getBounds().width;
        int progressHeight = progressBg.getBounds().height;
        int progressY = canvas.getSize().y-progressHeight-1;
        int x = width / 2 - progressWidth / 2;
        int w = (int) (progressWidth * progress + 0.5);
        gc.drawImage(progressBg, x, progressY);
        gc.drawImage(progressFg, 0, 0, w, progressHeight, x, progressY, w, progressHeight);

        String text = String.format("Version %s", EditorCorePlugin.getDefault().getVersion());
        int textWidth = gc.stringExtent(text).x;
        gc.setBackground(new Color(getSplash().getDisplay(), 240, 242, 246));
        gc.drawText(text, width / 2 - textWidth / 2, 360 - gc.getFontMetrics().getHeight() / 2);

        if (ribbon != null) {
            gc.drawImage(ribbon, 0, 0);
        }
        gc.dispose();
    }
}
