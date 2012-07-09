package com.dynamo.cr.web2.client.ui;

import com.dynamo.cr.web2.client.InvitationAccountInfo;
import com.dynamo.cr.web2.client.ProjectInfo;
import com.dynamo.cr.web2.client.ProjectInfoList;
import com.dynamo.cr.web2.client.UserSubscriptionInfo;
import com.dynamo.cr.web2.shared.ClientUtil;
import com.google.gwt.core.client.GWT;
import com.google.gwt.core.client.JsArray;
import com.google.gwt.dom.client.SpanElement;
import com.google.gwt.event.dom.client.ClickEvent;
import com.google.gwt.uibinder.client.UiBinder;
import com.google.gwt.uibinder.client.UiField;
import com.google.gwt.uibinder.client.UiHandler;
import com.google.gwt.user.client.ui.Button;
import com.google.gwt.user.client.ui.Composite;
import com.google.gwt.user.client.ui.Image;
import com.google.gwt.user.client.ui.VerticalPanel;
import com.google.gwt.user.client.ui.Widget;

public class DashboardView extends Composite {

    public interface Presenter {

        void showProject(ProjectInfo projectInfo);

        void onNewProject();

        void removeProject(ProjectInfo projectInfo);

        boolean isOwner(ProjectInfo projectInfo);

        void invite(String recipient);

        void onEditSubscription();
    }

    private static DashboardUiBinder uiBinder = GWT
            .create(DashboardUiBinder.class);
    @UiField VerticalPanel projects;
    @UiField Button newProjectButton;
    @UiField Image gravatar;
    @UiField SpanElement firstName;
    @UiField SpanElement lastName;
    @UiField SubscriptionBox subscriptionBox;
    @UiField InvitationBox invitationBox;

    interface DashboardUiBinder extends UiBinder<Widget, DashboardView> {
    }

    private Presenter listener;

    public DashboardView() {
        initWidget(uiBinder.createAndBindUi(this));

        if (ClientUtil.isDev()) {
            this.subscriptionBox.setVisible(false);
        }
    }

    public void setProjectInfoList(int userId, ProjectInfoList projectInfoList) {
        projects.clear();

        JsArray<ProjectInfo> projects = projectInfoList.getProjects();
        for (int i = 0; i < projects.length(); ++i) {
            final ProjectInfo projectInfo = projects.get(i);
            ProjectBox projectBox = new ProjectBox(listener, projectInfo, projectInfo.getOwner().getId() == userId);
            this.projects.add(projectBox);
        }
    }

    public void clearProjectInfoList() {
        projects.clear();
    }

    public void setInvitationAccount(InvitationAccountInfo invitationAccount) {
        int invitationCount = invitationAccount.getCurrentCount();
        if (invitationCount > 0) {
            invitationBox.setInvitationCount(invitationCount);
            invitationBox.setVisible(true);
        } else {
            invitationBox.setVisible(false);
        }
    }

    public void setUserSubscription(UserSubscriptionInfo subscription) {
        subscriptionBox.setSubscription(subscription);
    }

    public void setPresenter(DashboardView.Presenter listener) {
        this.listener = listener;
        this.invitationBox.setPresenter(listener);
        this.invitationBox.setVisible(false);
        this.subscriptionBox.setPresenter(listener);
    }

    @UiHandler("newProjectButton")
    void onNewProjectButtonClick(ClickEvent event) {
        this.listener.onNewProject();
    }

    public void setGravatarURL(String url) {
        gravatar.setUrl(url);
    }

    public void setName(String firstName, String lastName, String email) {
        this.firstName.setInnerText(firstName);
        this.lastName.setInnerText(lastName);
    }
}
