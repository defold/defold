package com.dynamo.cr.web2.client.activity;

import com.dynamo.cr.web2.client.ClientFactory;
import com.dynamo.cr.web2.client.Defold;
import com.dynamo.cr.web2.client.ProjectInfo;
import com.dynamo.cr.web2.client.ResourceCallback;
import com.dynamo.cr.web2.client.UserInfoList;
import com.dynamo.cr.web2.client.place.ProjectPlace;
import com.dynamo.cr.web2.client.ui.ProjectView;
import com.google.gwt.activity.shared.AbstractActivity;
import com.google.gwt.event.shared.EventBus;
import com.google.gwt.http.client.Request;
import com.google.gwt.http.client.Response;
import com.google.gwt.user.client.ui.AcceptsOneWidget;

public class ProjectActivity extends AbstractActivity implements
        ProjectView.Presenter {
    private ClientFactory clientFactory;
    private String projectId;

    public ProjectActivity(ProjectPlace place, ClientFactory clientFactory) {
        projectId = place.getId();
        this.clientFactory = clientFactory;
    }

    private void loadProject() {
        final ProjectView projectView = clientFactory.getProjectView();
        Defold defold = clientFactory.getDefold();

        StringBuilder projectInfoResource = new StringBuilder();
        projectInfoResource.append("/projects/");
        projectInfoResource.append(defold.getUserId());
        projectInfoResource.append("/");
        projectInfoResource.append(projectId);
        projectInfoResource.append("/project_info");

        defold.getResource(projectInfoResource.toString(),
                new ResourceCallback<ProjectInfo>() {

                    @Override
                    public void onSuccess(ProjectInfo projectInfo,
                            Request request, Response response) {
                        projectView.setProjectInfo(projectInfo);
                    }

                    @Override
                    public void onFailure(Request request, Response response) {
                    }
                });
    }

    private void loadConnections() {
        final ProjectView projectView = clientFactory.getProjectView();
        Defold defold = clientFactory.getDefold();
        defold.getResource("/users/" + defold.getUserId() + "/connections",
                new ResourceCallback<UserInfoList>() {

                    @Override
                    public void onSuccess(UserInfoList userInfoList,
                            Request request, Response response) {
                        projectView.setConnections(userInfoList);
                    }

                    @Override
                    public void onFailure(Request request, Response response) {
                    }
                });
    }

    @Override
    public void start(AcceptsOneWidget containerWidget, EventBus eventBus) {
        final ProjectView projectView = clientFactory.getProjectView();
        projectView.setPresenter(this);
        projectView.init();
        containerWidget.setWidget(projectView.asWidget());

        loadProject();
        loadConnections();
    }

    @Override
    public void addMember(String email) {
        final Defold defold = clientFactory.getDefold();

        StringBuilder url = new StringBuilder();
        url.append("/projects");
        url.append("/");
        url.append(defold.getUserId());
        url.append("/");
        url.append(projectId);
        url.append("/members");

        defold.postResource(url.toString(), email,
                new ResourceCallback<String>() {
                    @Override
                    public void onSuccess(String result, Request request,
                            Response response) {
                        int code = response.getStatusCode();
                        if (code == 200 || code == 204) {
                            loadProject();
                        } else {
                            defold.showErrorMessage(response.getText());
                        }
                    }

                    @Override
                    public void onFailure(Request request, Response response) {
                    }
                });
    }

    @Override
    public void removeMember(int userId) {
        final Defold defold = clientFactory.getDefold();
        defold.deleteResource("/projects/" + defold.getUserId() + "/"
                + projectId + "/members/" + userId,
                new ResourceCallback<String>() {
                    @Override
                    public void onSuccess(String result, Request request,
                            Response response) {
                        if (response.getStatusCode() < 400) {
                            loadProject();
                        } else {
                            defold.showErrorMessage(response.getStatusText());
                        }
                    }

                    @Override
                    public void onFailure(Request request, Response response) {
                    }
                });

    }
}