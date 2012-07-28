package com.dynamo.cr.editor.ui.tip;

import org.eclipse.swt.widgets.Control;
import org.eclipse.swt.widgets.Shell;

public interface ITipManager {

    public Control createControl(Shell shell);

    public void hideTip(Tip tip);

    public void dispose();

    public boolean tipAvailable();

    public void showTip(boolean force);

}