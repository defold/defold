package com.dynamo.cr.tileeditor.core;

import org.eclipse.core.commands.ExecutionException;
import org.eclipse.core.commands.operations.AbstractOperation;
import org.eclipse.core.runtime.IAdaptable;
import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Status;

public class ModifyModelOperation extends AbstractOperation {
    Model model;
    Object source;
    String key;
    Object previousValue;
    Object value;

    public ModifyModelOperation(Model model, Object source, String key, Object previousValue, Object value) {
        super("modify " + key);
        this.model = model;
        this.source = source;
        this.key = key;
        this.previousValue = previousValue;
        this.value = value;
    }

    @Override
    public IStatus execute(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        this.model.setProperty(this.source, this.key, this.value);
        return Status.OK_STATUS;
    }

    @Override
    public IStatus redo(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        this.model.setProperty(this.source, this.key, this.value);
        return Status.OK_STATUS;
    }

    @Override
    public IStatus undo(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        this.model.setProperty(this.source, this.key, this.previousValue);
        return Status.OK_STATUS;
    }

}
