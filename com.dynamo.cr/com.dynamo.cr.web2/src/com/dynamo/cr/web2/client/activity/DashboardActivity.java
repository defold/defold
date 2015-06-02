package com.dynamo.cr.web2.client.activity;

import com.dynamo.cr.web2.client.ClientFactory;
import com.dynamo.cr.web2.client.Defold;
import com.dynamo.cr.web2.client.ProjectInfo;
import com.dynamo.cr.web2.client.ProjectInfoList;
import com.dynamo.cr.web2.client.ResourceCallback;
import com.dynamo.cr.web2.client.place.DashboardPlace;
import com.dynamo.cr.web2.client.place.NewProjectPlace;
import com.dynamo.cr.web2.client.place.ProjectPlace;
import com.dynamo.cr.web2.client.ui.DashboardView;
import com.google.gwt.activity.shared.AbstractActivity;
import com.google.gwt.event.shared.EventBus;
import com.google.gwt.http.client.Request;
import com.google.gwt.http.client.Response;
import com.google.gwt.user.client.ui.AcceptsOneWidget;

public class DashboardActivity extends AbstractActivity implements DashboardView.Presenter {
    private ClientFactory clientFactory;

    public DashboardActivity(DashboardPlace place, ClientFactory clientFactory) {
        this.clientFactory = clientFactory;
    }

    public void loadProjects() {
        final DashboardView dashboardView = clientFactory.getDashboardView();
        final Defold defold = clientFactory.getDefold();
        defold.getResource("/projects/" + defold.getUserId(), new ResourceCallback<ProjectInfoList>() {

            @Override
            public void onSuccess(ProjectInfoList projectInfoList, Request request,
                    Response response) {
                dashboardView.setProjectInfoList(defold.getUserId(), projectInfoList);
            }

            @Override
            public void onFailure(Request request, Response response) {
                dashboardView.clearProjectInfoList();
            }
        });
    }

    @Override
    public void start(AcceptsOneWidget containerWidget, EventBus eventBus) {
        final DashboardView dashboardView = clientFactory.getDashboardView();
        containerWidget.setWidget(dashboardView.asWidget());
        dashboardView.setPresenter(this);
        loadProjects();
        loadGravatar();
    }

    private void loadGravatar() {
        final DashboardView dashboardView = clientFactory.getDashboardView();
        Defold defold = clientFactory.getDefold();
        String email = defold.getEmail();

        String firstName = defold.getFirstName();
        String lastName = defold.getLastName();
        dashboardView.setUserInfo(firstName, lastName, email);
    }

    @Override
    public void showProject(ProjectInfo projectInfo) {
        clientFactory.getPlaceController().goTo(new ProjectPlace(Integer.toString(projectInfo.getId())));
    }

    @Override
    public void onNewProject() {
        clientFactory.getPlaceController().goTo(new NewProjectPlace());
    }

    @Override
    public void removeProject(ProjectInfo projectInfo) {
        final Defold defold = clientFactory.getDefold();
        defold.deleteResource("/projects/" + defold.getUserId() + "/" + projectInfo.getId(), new ResourceCallback<String>() {

            @Override
            public void onSuccess(String result, Request request,
                    Response response) {

                int statusCode = response.getStatusCode();
                if (statusCode >= 300) {
                    defold.showErrorMessage(response.getText());
                } else {
                    loadProjects();
                }
            }

            @Override
            public void onFailure(Request request, Response response) {
                defold.showErrorMessage(response.getText());
            }
        });
    }

    @Override
    public boolean isOwner(ProjectInfo projectInfo) {
        return clientFactory.getDefold().getUserId() == projectInfo.getOwner().getId();
    }

}