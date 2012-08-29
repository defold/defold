package com.dynamo.cr.projed;

import org.eclipse.core.runtime.IStatus;

public interface IProjectEditorView {

    public void setValue(KeyMeta keyMeta, String value);

    public void setMessage(IStatus status);

}