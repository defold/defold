package com.dynamo.cr.editor;

import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Status;
import org.eclipse.core.runtime.jobs.Job;

import clojure.osgi.ClojureHelper;

public class ClojureBootstrap extends Job {
    public ClojureBootstrap() {
        super("Booting tools");
    }

    @Override
    protected IStatus run(IProgressMonitor monitor) {
        monitor.beginTask("Loading dev tools", 2);
        try {
            ClojureHelper.require("internal.system");
            monitor.worked(1);
            ClojureHelper.invoke("internal.system", "start");
            monitor.worked(2);
            monitor.done();
            return Status.OK_STATUS;
        } catch (Exception e) {
            return new Status(Status.ERROR, Activator.PLUGIN_ID, "Exception while loading dev tools", e);
        }
    }
}
