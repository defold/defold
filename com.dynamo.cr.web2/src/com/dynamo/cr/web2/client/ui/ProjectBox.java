package com.dynamo.cr.web2.client.ui;

import com.dynamo.cr.web2.client.ProjectInfo;
import com.dynamo.cr.web2.client.ui.DashboardView.Presenter;
import com.google.gwt.core.client.GWT;
import com.google.gwt.event.dom.client.ClickEvent;
import com.google.gwt.uibinder.client.UiBinder;
import com.google.gwt.uibinder.client.UiField;
import com.google.gwt.uibinder.client.UiHandler;
import com.google.gwt.user.client.ui.Anchor;
import com.google.gwt.user.client.ui.Composite;
import com.google.gwt.user.client.ui.Label;
import com.google.gwt.user.client.ui.Widget;

public class ProjectBox extends Composite {

    private static ProjectBoxUiBinder uiBinder = GWT
            .create(ProjectBoxUiBinder.class);

    interface ProjectBoxUiBinder extends UiBinder<Widget, ProjectBox> {
    }

    @UiField
    Anchor name;
    @UiField
    Label memberOrOwner;
    @UiField
    Label description;
    private Presenter listener;
    private ProjectInfo projectInfo;

    public ProjectBox(Presenter listener, ProjectInfo projectInfo, boolean isOwner) {
        initWidget(uiBinder.createAndBindUi(this));
        this.listener = listener;
        this.projectInfo = projectInfo;
        this.name.setText(projectInfo.getName());
        this.memberOrOwner.setText(isOwner ? "Owner" : "User");
        this.description.setText(projectInfo.getDescription());
    }

    @UiHandler("name")
    void onNameLinkClick(ClickEvent event) {
        listener.showProject(projectInfo);
    }

}
