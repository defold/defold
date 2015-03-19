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

    private String url;

    public LoginDialog(Shell parentShell, String url) {
        super(parentShell);
        this.url = url;
    }

    @Override
    protected void configureShell(Shell newShell) {
        super.configureShell(newShell);
        newShell.setText("Sign out");
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
        setMessage("Optionally sign in to your account. Press Ok when signed in.");
        return ret;
    }

    @Override
    protected boolean isResizable() {
        return true;
    }

    @Override
    protected Control createDialogArea(Composite parent) {
        Browser browser = new Browser(parent, SWT.NONE);
        browser.setUrl(url);
        browser.setLayoutData(new GridData(GridData.FILL_BOTH));
        return parent;
    }

    @Override
    protected Point getInitialSize() {
        return new Point(900, 800);
    }
}

public class SignoutHandler extends AbstractHandler {

    @Override
    public Object execute(ExecutionEvent event) throws ExecutionException {
        String url = event.getParameter("com.dynamo.crepo.commands.signOut.url");
        LoginDialog loginDialog = new LoginDialog(HandlerUtil.getActiveShell(event), url);
        loginDialog.open();
        IPreferenceStore store = Activator.getDefault().getPreferenceStore();
        store.setValue(PreferenceConstants.P_AUTH_TOKEN, "");
        return null;
    }

}
