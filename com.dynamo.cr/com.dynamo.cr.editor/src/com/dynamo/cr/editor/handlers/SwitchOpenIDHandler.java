package com.dynamo.cr.editor.handlers;

import org.eclipse.core.commands.AbstractHandler;
import org.eclipse.core.commands.ExecutionEvent;
import org.eclipse.core.commands.ExecutionException;
import org.eclipse.jface.dialogs.IDialogConstants;
import org.eclipse.jface.dialogs.TitleAreaDialog;
import org.eclipse.jface.preference.IPreferenceStore;
import org.eclipse.swt.SWT;
import org.eclipse.swt.browser.Browser;
import org.eclipse.swt.graphics.Point;
import org.eclipse.swt.layout.GridData;
import org.eclipse.swt.widgets.Composite;
import org.eclipse.swt.widgets.Control;
import org.eclipse.swt.widgets.Shell;
import org.eclipse.ui.handlers.HandlerUtil;

import com.dynamo.cr.editor.Activator;
import com.dynamo.cr.editor.preferences.PreferenceConstants;

class LoginDialog extends TitleAreaDialog {

    public LoginDialog(Shell parentShell) {
        super(parentShell);
    }

    @Override
    protected void configureShell(Shell newShell) {
        super.configureShell(newShell);
        newShell.setText("Sign in to your Google account");
    }

    @Override
    protected void createButtonsForButtonBar(Composite parent) {
        createButton(parent, IDialogConstants.OK_ID,
                IDialogConstants.OK_LABEL, false);
    }

    @Override
    protected Control createContents(Composite parent) {
        Control ret = super.createContents(parent);
        setTitle("Sign in");
        setMessage("Sign in to your Google account. Press Ok when signed in.");
        return ret;
    }

    @Override
    protected boolean isResizable() {
        return true;
    }

    @Override
    protected Control createDialogArea(Composite parent) {
        Browser browser = new Browser(parent, SWT.NONE);
        browser.setUrl("https://www.google.com/accounts/Logout");
        browser.setLayoutData(new GridData(GridData.FILL_BOTH));
        return parent;
    }

    @Override
    protected Point getInitialSize() {
        return new Point(700, 800);
    }
}

public class SwitchOpenIDHandler extends AbstractHandler {

    @Override
    public Object execute(ExecutionEvent event) throws ExecutionException {
        LoginDialog loginDialog = new LoginDialog(HandlerUtil.getActiveShell(event));
        loginDialog.open();
        IPreferenceStore store = Activator.getDefault().getPreferenceStore();
        store.setValue(PreferenceConstants.P_AUTH_COOKIE, "");
        return null;
    }

}
