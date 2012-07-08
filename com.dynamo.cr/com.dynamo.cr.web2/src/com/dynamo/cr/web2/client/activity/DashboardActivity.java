package com.dynamo.cr.web2.client.activity;

import com.dynamo.cr.web2.client.ClientFactory;
import com.dynamo.cr.web2.client.Defold;
import com.dynamo.cr.web2.client.InvitationAccountInfo;
import com.dynamo.cr.web2.client.MD5;
import com.dynamo.cr.web2.client.ProjectInfo;
import com.dynamo.cr.web2.client.ProjectInfoList;
import com.dynamo.cr.web2.client.ResourceCallback;
import com.dynamo.cr.web2.client.UserSubscriptionInfo;
import com.dynamo.cr.web2.client.place.DashboardPlace;
import com.dynamo.cr.web2.client.place.NewProjectPlace;
import com.dynamo.cr.web2.client.place.ProjectPlace;
import com.dynamo.cr.web2.client.place.SubscriptionPlace;
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

    private void loadSubscription() {
        final DashboardView dashboardView = clientFactory.getDashboardView();
        final Defold defold = clientFactory.getDefold();
        defold.getResource("/users/" + defold.getUserId() + "/subscription",
                new ResourceCallback<UserSubscriptionInfo>() {

                    @Override public void onSuccess(UserSubscriptionInfo subscription,
                            Request request, Response response) {
                        dashboardView.setUserSubscription(subscription);
                    }

                    @Override public void onFailure(Request request, Response response) {
                        defold.showErrorMessage("Subscription data could not be loaded.");
                    }
                });
    }

    private void loadInvitationCount() {
        final DashboardView dashboardView = clientFactory.getDashboardView();
        final Defold defold = clientFactory.getDefold();
        defold.getResource("/users/" + defold.getUserId() + "/invitation_account",
                new ResourceCallback<InvitationAccountInfo>() {

                    @Override
                    public void onSuccess(InvitationAccountInfo invitationAccount,
                            Request request, Response response) {
                        dashboardView.setInvitationAccount(invitationAccount);
                    }

                    @Override
                    public void onFailure(Request request, Response response) {
                        defold.showErrorMessage("Invitation data could not be loaded.");
                    }
                });
    }

    @Override
    public void start(AcceptsOneWidget containerWidget, EventBus eventBus) {
        final DashboardView dashboardView = clientFactory.getDashboardView();
        dashboardView.setPresenter(this);
        containerWidget.setWidget(dashboardView.asWidget());
        loadProjects();
        loadGravatar();
        loadInvitationCount();
        loadSubscription();
    }

    private void loadGravatar() {
        final DashboardView dashboardView = clientFactory.getDashboardView();
        Defold defold = clientFactory.getDefold();
        String email = defold.getEmail();
        email = email.trim().toLowerCase();
        String md5 = MD5.md5(email);
        String url = "http://www.gravatar.com/avatar/" + md5 + "?s=80";
        dashboardView.setGravatarURL(url);

        String firstName = defold.getFirstName();
        String lastName = defold.getLastName();
        dashboardView.setName(firstName, lastName, email);
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

    @Override
    public void invite(String recipient) {
        final Defold defold = clientFactory.getDefold();
        // TODO validate email
        defold.putResource("/users/" + defold.getUserId() + "/invite/" + recipient, "",
            new ResourceCallback<String>() {

                @Override
                public void onSuccess(String result,
                        Request request, Response response) {
                    loadInvitationCount();
                }

                @Override
                public void onFailure(Request request, Response response) {
                    defold.showErrorMessage("Invitation failed: " + response.getText());
                }
            });
    }

    @Override public void onEditSubscription() {
        this.clientFactory.getPlaceController().goTo(new SubscriptionPlace());
    }

}