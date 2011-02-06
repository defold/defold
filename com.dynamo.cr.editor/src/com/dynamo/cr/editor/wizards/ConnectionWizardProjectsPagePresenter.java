package com.dynamo.cr.editor.wizards;

import java.util.ArrayList;
import java.util.Collection;

import org.eclipse.jface.dialogs.MessageDialog;
import org.eclipse.swt.widgets.Shell;
import org.eclipse.ui.IWorkbenchPage;

import com.dynamo.cr.client.IProjectClient;
import com.dynamo.cr.client.IProjectsClient;
import com.dynamo.cr.client.RepositoryException;
import com.dynamo.cr.editor.Activator;
import com.dynamo.cr.protocol.proto.Protocol.ProjectInfo;
import com.dynamo.cr.protocol.proto.Protocol.ProjectInfoList;

public class ConnectionWizardProjectsPagePresenter {

    public interface IDisplay {
        ProjectInfo getSelectedProject();
        void setMessage(String message);
        void setErrorMessage(String message);
        String getErrorMessage();

        void setProjectsNames(Collection<ProjectInfo> projectNames);
        //String[] getProjectNames();
        void setPageComplete(boolean pageComplete);
        boolean isPageComplete();
        //void setSelected(String name);
        Shell getShell();
    }

    private IDisplay display;
    private IProjectsClient client;
    private boolean connectionOk;
    private IWorkbenchPage page;
    private ConnectionWizardBranchPagePresenter branchPresenter;

    public ConnectionWizardProjectsPagePresenter(IDisplay display, IWorkbenchPage page, ConnectionWizardBranchPagePresenter branchPresenter) {
        this.display = display;
        this.page = page;
        this.branchPresenter = branchPresenter;
    }

    public void onSelectProject(ProjectInfo projectInfo) {
        if (projectInfo != null) {
            IProjectClient projectClient = client.getProjectClient(projectInfo.getId());
            branchPresenter.setProjectResourceClient(projectClient);
            display.setPageComplete(canFinish());
        }
    }

    public void setProjectsResourceClient(IProjectsClient client) {
        this.client = client;
    }

    void updateProjectList() throws RepositoryException  {
        ProjectInfoList projectList = client.getProjects();
        display.setProjectsNames(projectList.getProjectsList());
    }

    public void init() {
        display.setPageComplete(false);
        connectionOk = true;
        try {
            updateProjectList();
        }
        catch (Throwable e) {
            connectionOk = false;
            display.setErrorMessage(e.getMessage());
        }
    }

    public boolean canFinish() {
        return display.getErrorMessage() == null && display.getSelectedProject() != null;
    }

}
