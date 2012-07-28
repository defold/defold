package com.dynamo.cr.editor.ui;

import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.core.runtime.NullProgressMonitor;
import org.eclipse.core.runtime.QualifiedName;
import org.eclipse.core.runtime.jobs.Job;
import org.eclipse.core.runtime.jobs.ProgressProvider;
import org.eclipse.swt.SWT;
import org.eclipse.swt.widgets.Composite;
import org.eclipse.swt.widgets.Control;
import org.eclipse.swt.widgets.Display;
import org.eclipse.swt.widgets.Label;

public class SimpleProgressProvider extends ProgressProvider {

    private Label label;
    private SimpleProgressMonitor monitor;

    /**
     * Very simple progress monitor. No group support, only capable of handling
     * a single job but it's designed to only show build-jobs
     * We have a custom workbench-window layout and the default progress-bar isn't accessible.
     * The built-in is also *very* complex.
     * @author chmu
     *
     */
    class SimpleProgressMonitor implements IProgressMonitor {

        double worked;
        double totalWork;
        boolean updateKicked = false;
        String name = "";

        @Override
        public void beginTask(String name, int totalWork) {
            this.totalWork = totalWork;
        }

        @Override
        public void done() {
            reset(null);
        }

        @Override
        public void internalWorked(double work) {
            worked += work;
            refresh();
        }

        void refresh() {
            if (!updateKicked) {
                updateKicked = true;
                Display.getDefault().asyncExec(new Runnable() {
                   @Override
                    public void run() {
                       updateKicked = false;
                       if (worked == -1) {
                           label.setText("");
                       } else {
                           label.setText(String.format("%s (%.0f %%)", name, 100 * worked/totalWork));
                       }
                    }
                });
            }
        }

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
            internalWorked(work);
        }

        public void reset(Job job) {
            worked = -1;
            if (job != null)
                name = job.getName();
            else
                name = "";
            refresh();
        }

    }

    public SimpleProgressProvider() {
        monitor = new SimpleProgressMonitor();
    }

    public Control createControl(Composite parent) {
        label = new Label(parent, SWT.NONE);
        label.setText("");
        return label;
    }

    @Override
    public IProgressMonitor createMonitor(Job job) {
        Object buildJob = job.getProperty(new QualifiedName("com.dynamo.cr.editor", "build"));
        if (buildJob != null) {
            monitor.reset(job);
            return monitor;
        } else {
            return new NullProgressMonitor();
        }
    }

}
