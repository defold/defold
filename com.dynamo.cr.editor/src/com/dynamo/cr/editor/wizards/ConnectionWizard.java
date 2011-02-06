package com.dynamo.cr.editor.wizards;

import org.eclipse.jface.wizard.Wizard;
import org.eclipse.swt.widgets.Composite;
import org.eclipse.ui.IWorkbenchPage;

import com.dynamo.cr.editor.Activator;


public class ConnectionWizard extends Wizard {

    private ConnectionWizardBranchPageView branchView;
    private ConnectionWizardBranchPagePresenter branchPresenter;
    private IWorkbenchPage page;
    private ConnectionWizardProjectsPageView projectView;
    private ConnectionWizardProjectsPagePresenter projectPresenter;

    public ConnectionWizard(IWorkbenchPage page) {
        this.page = page;
        setWindowTitle("Connect to resource server");
    }

    @Override
    public void addPages() {
        branchView = new ConnectionWizardBranchPageView("Project");
        branchPresenter = new ConnectionWizardBranchPagePresenter(branchView, page);
        branchView.setPresenter(branchPresenter);

        projectView = new ConnectionWizardProjectsPageView("Branch");
        projectPresenter = new ConnectionWizardProjectsPagePresenter(projectView, page, branchPresenter);
        projectPresenter.setProjectsResourceClient(Activator.getDefault().projectsClient);
        projectView.setPresenter(projectPresenter);

        addPage(projectView);
        addPage(branchView);
    }

    @Override
    public void createPageControls(Composite pageContainer) {
        super.createPageControls(pageContainer);
        branchPresenter.init();
        projectPresenter.init();
    }

    @Override
    public boolean performFinish() {
        return branchPresenter.finish();
    }
}
