package com.dynamo.cr.editor.dialogs;

import org.eclipse.jface.dialogs.IDialogConstants;
import org.eclipse.jface.dialogs.MessageDialog;
import org.eclipse.swt.SWT;
import org.eclipse.swt.layout.GridData;
import org.eclipse.swt.layout.GridLayout;
import org.eclipse.swt.widgets.Composite;
import org.eclipse.swt.widgets.Control;
import org.eclipse.swt.widgets.Shell;
import org.eclipse.swt.widgets.Text;

public class LogErrorDialog extends MessageDialog {

    private String log;

    public LogErrorDialog(Shell parentShell, String title, String message, String log) {

        super(parentShell, title, null, message,
                ERROR, new String[] { IDialogConstants.OK_LABEL }, 0);
        this.log = log;
    }

    @Override
    protected Control createCustomArea(Composite parent) {
        Composite composite = new Composite(parent, SWT.NONE);
        composite.setLayoutData(new GridData(SWT.FILL, SWT.FILL, true, true));
        composite.setLayout(new GridLayout(1, false));

        Text text = new Text(composite, SWT.MULTI | SWT.H_SCROLL | SWT.V_SCROLL);
        text.setText(log);
        GridData gd = new GridData(SWT.FILL, SWT.FILL, true, false);
        gd.widthHint = 200;;
        gd.heightHint = 200;;
        text.setLayoutData(gd);

        return composite;
    }

    public static void open(Shell parent, String title, String message,
            String log) {
        new LogErrorDialog(parent, title, message, log).open();
    }

}
