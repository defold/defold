package com.dynamo.cr.editor.ui;

import org.eclipse.swt.widgets.Shell;

public interface IEditorWindow {

    public abstract void setMessageAreaVisible(boolean visible);

    public abstract void dispose();

    public abstract void createContents(final Shell shell);

}
