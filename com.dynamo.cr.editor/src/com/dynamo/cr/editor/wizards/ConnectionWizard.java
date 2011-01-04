package com.dynamo.cr.editor.wizards;

import org.eclipse.jface.preference.IPreferenceStore;
import org.eclipse.jface.wizard.Wizard;
import org.eclipse.swt.widgets.Composite;

import com.dynamo.cr.editor.Activator;
import com.dynamo.cr.editor.preferences.PreferenceConstants;


public class ConnectionWizard extends Wizard {

    private ConnectionWizardPageView view;
    private ConnectionWizardPagePresenter presenter;
    private String user;

    public ConnectionWizard() {
        setWindowTitle("Connect to resource server");
        IPreferenceStore store = Activator.getDefault().getPreferenceStore();
        user = store.getString(PreferenceConstants.P_USERNAME);
    }

    @Override
    public void addPages() {
        view = new ConnectionWizardPageView("Connect");
        presenter = new ConnectionWizardPagePresenter(view);
        presenter.setProjectResourceClient(Activator.getDefault().projectClient, user);
        view.setPresenter(presenter);
        addPage(view);
    }

    @Override
    public void createPageControls(Composite pageContainer) {
        super.createPageControls(pageContainer);
        presenter.init();
    }

    @Override
    public boolean performFinish() {
        return presenter.finish();
    }
}
