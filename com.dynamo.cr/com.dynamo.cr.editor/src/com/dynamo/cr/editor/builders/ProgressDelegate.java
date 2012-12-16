package com.dynamo.cr.editor.builders;

import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.core.runtime.SubProgressMonitor;

import com.dynamo.bob.IProgress;

public class ProgressDelegate implements IProgress {

    private IProgressMonitor monitor;

    public ProgressDelegate(IProgressMonitor monitor) {
        this.monitor = monitor;
    }

    @Override
    public IProgress subProgress(int work) {
        return new ProgressDelegate(new SubProgressMonitor(monitor, work));
    }

    @Override
    public void worked(int amount) {
        this.monitor.worked(amount);
    }

    @Override
    public void beginTask(String name, int work) {
        this.monitor.beginTask(name, work);
    }

    @Override
    public void done() {
        this.monitor.done();
    }

}
