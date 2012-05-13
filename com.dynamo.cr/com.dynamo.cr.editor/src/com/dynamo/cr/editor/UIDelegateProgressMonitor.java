package com.dynamo.cr.editor;

import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.core.runtime.ProgressMonitorWrapper;
import org.eclipse.swt.widgets.Display;

/**
 * IProgressMonitor wrapper to run progress-monitor from non-ui thread.
 * Which methods we wrap is rather arbitrary. The code is inspired by AccumulatingProgressMonitor
 * @author chmu
 *
 */
public class UIDelegateProgressMonitor extends ProgressMonitorWrapper {

    private Display display;

    /*
     * NOTE: We neither wrap setCanceled nor isCanceled
     * Got strange problem in p2 update manager. The operation got cancelled.
     * These method are hopefully thread safe
     */
    public UIDelegateProgressMonitor(IProgressMonitor monitor, Display display) {
        super(monitor);
        this.display = display;
    }

    @Override
    public void beginTask(final String name, final int totalWork) {
        display.syncExec(new Runnable() {
            @Override
            public void run() {
                getWrappedProgressMonitor().beginTask(name, totalWork);
            }
        });
    }

    @Override
    public void done() {
        display.syncExec(new Runnable() {
            @Override
            public void run() {
                getWrappedProgressMonitor().done();
            }
        });
    }

    @Override
    public void internalWorked(final double work) {
        // NOTE: asyncExec on this one
        display.asyncExec(new Runnable() {
            @Override
            public void run() {
                getWrappedProgressMonitor().internalWorked(work);
            }
        });
    }

    @Override
    public void setTaskName(final String name) {
        // NOTE: asyncExec on this one
        display.asyncExec(new Runnable() {
            @Override
            public void run() {
                getWrappedProgressMonitor().setTaskName(name);
            }
        });
    }

    @Override
    public void subTask(final String name) {
        // NOTE: asyncExec on this one
        display.asyncExec(new Runnable() {
            @Override
            public void run() {
                getWrappedProgressMonitor().subTask(name);
            }
        });
    }

    @Override
    public void worked(final int work) {
     // NOTE: asyncExec on this one
        display.asyncExec(new Runnable() {
            @Override
            public void run() {
                getWrappedProgressMonitor().worked(work);
            }
        });
    }

}
