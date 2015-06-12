package com.dynamo.cr.web2.client.ui;

import java.util.Date;

import com.dynamo.cr.web2.client.CommitDesc;
import com.dynamo.cr.web2.client.Log;
import com.dynamo.cr.web2.client.ProjectInfo;
import com.dynamo.cr.web2.client.UserInfo;
import com.dynamo.cr.web2.client.UserInfoList;
import com.dynamo.cr.web2.shared.ClientUtil;
import com.google.gwt.core.client.GWT;
import com.google.gwt.core.client.JsArray;
import com.google.gwt.dom.client.DivElement;
import com.google.gwt.dom.client.HeadingElement;
import com.google.gwt.event.dom.client.ClickEvent;
import com.google.gwt.event.dom.client.ClickHandler;
import com.google.gwt.i18n.client.DateTimeFormat;
import com.google.gwt.uibinder.client.UiBinder;
import com.google.gwt.uibinder.client.UiField;
import com.google.gwt.uibinder.client.UiHandler;
import com.google.gwt.user.client.Element;
import com.google.gwt.user.client.Window;
import com.google.gwt.user.client.ui.Anchor;
import com.google.gwt.user.client.ui.Button;
import com.google.gwt.user.client.ui.Composite;
import com.google.gwt.user.client.ui.DeckPanel;
import com.google.gwt.user.client.ui.FlexTable;
import com.google.gwt.user.client.ui.FocusPanel;
import com.google.gwt.user.client.ui.HTMLPanel;
import com.google.gwt.user.client.ui.HTMLTable.CellFormatter;
import com.google.gwt.user.client.ui.Image;
import com.google.gwt.user.client.ui.Label;
import com.google.gwt.user.client.ui.MultiWordSuggestOracle;
import com.google.gwt.user.client.ui.SuggestBox;
import com.google.gwt.user.client.ui.TextArea;
import com.google.gwt.user.client.ui.TextBox;
import com.google.gwt.user.client.ui.Widget;
import com.google.gwt.user.datepicker.client.CalendarUtil;

public class ProjectView extends Composite {
    public interface Presenter {
        void addMember(String email);
        void removeMember(int id);
        void deleteProject(ProjectInfo projectInfo);
        void setProjectInfo(ProjectInfo projectInfo);
    }

    private static ProjectDetailsUiBinder uiBinder = GWT
            .create(ProjectDetailsUiBinder.class);
    @UiField SideBar sideBar;

    @UiField DeckPanel projectNameDeck;
    @UiField FocusPanel projectNameDescription;
    @UiField HeadingElement projectName;
    @UiField DivElement description;
    @UiField TextBox projectNameTextBox;
    @UiField TextArea descriptionTextArea;
    @UiField Button updateButton;
    @UiField Button cancelButton;

    @UiField Button addMember;
    @UiField Button deleteProject;
    /*
     * NOTE: Watch out for this bug:
     * http://code.google.com/p/google-web-toolkit/issues/detail?id=3533 We
     * add handler to the textbox
     */
    @UiField(provided = true) SuggestBox suggestBox;
    @UiField FlexTable members2;
    @UiField FlexTable commits2;
    @UiField HTMLPanel addMemberPanel;
    @UiField Anchor iOSDownload;
    @UiField Label signedExeInfo;
    @UiField TextBox libraryUrl;
    @UiField Label trackingId;

    private final MultiWordSuggestOracle suggestions = new MultiWordSuggestOracle();
    private Presenter listener;
    private ProjectInfo projectInfo;
    private boolean isOwner;

    interface ProjectDetailsUiBinder extends UiBinder<Widget, ProjectView> {
    }

    public ProjectView() {
        suggestBox = new SuggestBox(suggestions);
        suggestBox.getElement().setPropertyString("placeholder", "enter email to add user");
        initWidget(uiBinder.createAndBindUi(this));
        projectNameDeck.showWidget(0);

        Element element = members2.getElement();
        element.addClassName("table");
        element.addClassName("table-members");

        element = commits2.getElement();
        element.addClassName("table");
        element.addClassName("table-striped");
        element.addClassName("table-commits");

        projectNameTextBox.addStyleName("input-xlarge");
        descriptionTextArea.addStyleName("input-xlarge");
        addMember.addStyleName("btn btn-success");
        deleteProject.addStyleName("btn btn-danger");
        updateButton.addStyleName("btn btn-success");
        cancelButton.addStyleName("btn");
        libraryUrl.setReadOnly(true);
        libraryUrl.getElement();
    }

    public void clear() {
        this.projectNameDeck.showWidget(0);
        this.projectName.setInnerText("");
        this.deleteProject.setVisible(false);
        this.description.setInnerText("");
        this.members2.removeAllRows();
        this.commits2.removeAllRows();
        this.iOSDownload.setVisible(false);
        this.signedExeInfo.setText("");
        this.libraryUrl.setText("");
        this.trackingId.setText("");
    }

    public void setProjectInfo(int userId, ProjectInfo projectInfo, String iOSUrl) {
        this.projectInfo = projectInfo;
        this.projectName.setInnerText(projectInfo.getName());
        String description = projectInfo.getDescription();
        this.description.setInnerText(description);

        isOwner = userId == projectInfo.getOwner().getId();
        this.deleteProject.setVisible(isOwner);
        if (isOwner) {
            this.projectNameDescription.getElement().addClassName("project-owner");
        }
        else {
            this.projectNameDescription.getElement().removeClassName("project-owner");
        }

        this.members2.removeAllRows();
        JsArray<UserInfo> membersList = projectInfo.getMembers();
        for (int i = 0; i < membersList.length(); ++i) {
            final UserInfo memberInfo = membersList.get(i);

            Image gravatar = new Image(ClientUtil.createGravatarUrl(memberInfo.getEmail(), 48));
            this.members2.setWidget(i, 0, gravatar);
            this.members2.setText(i, 1, memberInfo.getFirstName() + " " + memberInfo.getLastName()  + " (" + memberInfo.getEmail() +")");

            if (isOwner && !(memberInfo.getId() == userId)) {
                Button removeButton = new Button("Remove");
                removeButton.addClickHandler(new ClickHandler() {
                    @Override
                    public void onClick(ClickEvent event) {
                        listener.removeMember(memberInfo.getId());
                    }
                });
                removeButton.addStyleName("btn btn-danger");
                this.members2.setWidget(i, 2, removeButton);
            } else {
                this.members2.setText(i, 2, " ");
            }

            CellFormatter cellFormatter = this.members2.getCellFormatter();
            cellFormatter.addStyleName(i, 0, "table-members-col0");
            cellFormatter.addStyleName(i, 1, "table-members-col1");
        }

        boolean hasIOSUrl = iOSUrl != null && !iOSUrl.isEmpty();
        iOSDownload.setVisible(hasIOSUrl);
        if (hasIOSUrl) {
            signedExeInfo.setText("Access this link with your iOS device for direct installation:");
        } else {
            signedExeInfo
                    .setText("No Defold App found. Use the Defold Editor to sign and upload it for everyone in your project to access.");
        }

        iOSDownload.setHref("itms-services://?action=download-manifest&url=" + iOSUrl);
        libraryUrl.setText("http://" + Window.Location.getHost() + "/p/" + projectInfo.getOwner().getId() + "/" + projectInfo.getId() + "/archive");
        trackingId.setText(projectInfo.getTrackingId());
    }

    public void setLog(int userId, Log log) {
        this.commits2.clear();
        JsArray<CommitDesc> commits = log.getCommits();
        DateTimeFormat sourceDF = DateTimeFormat.getFormat("yyyy-MM-dd HH:mm:ss Z");
        Date now = new Date();
        for (int i = 0; i < commits.length(); ++i) {
            final CommitDesc commit = commits.get(i);
            Date commitDate = sourceDF.parse(commit.getDate());
            String date = formatDate(now, commitDate);

            String msg = commit.getName() + " at " + date + "<br/>";
            msg += commit.getMessage().replace("\n", "<br/>");

            Image gravatar = new Image(ClientUtil.createGravatarUrl(commit.getEmail(), 32));
            this.commits2.setWidget(i, 0, gravatar);
            this.commits2.setHTML(i, 1, msg);
            CellFormatter cellFormatter = this.commits2.getCellFormatter();
            cellFormatter.addStyleName(i, 0, "table-commits-col0");
            cellFormatter.addStyleName(i, 1, "table-commits-col1");
        }
    }

    private String formatDate(Date now, Date before) {
        StringBuffer format = new StringBuffer("HH:mm");
        int days = CalendarUtil.getDaysBetween(before, now);
        if (days > 0) {
            format.append(", MMM d");
        }
        if (days >= 365) {
            format.append(", yyyy");
        }
        return DateTimeFormat.getFormat(format.toString()).format(before);
    }

    public void setPresenter(Presenter listener) {
        this.listener = listener;
        sideBar.setActivePage("projects");
    }

    public void setConnections(UserInfoList userInfoList) {
        suggestions.clear();
        JsArray<UserInfo> users = userInfoList.getUsers();
        for (int i = 0; i < users.length(); ++i) {
            suggestions.add(users.get(i).getEmail());
        }
    }

    @UiHandler("deleteProject")
    void onDeleteProjectClick(ClickEvent event) {
        if (Window.confirm("Are you sure you want to delete the project?")) {
            listener.deleteProject(this.projectInfo);
        }
    }

    @UiHandler("addMember")
    void onAddMemberClick(ClickEvent event) {
        String email = suggestBox.getText();
        if (email != null && email.length() > 0) {
            listener.addMember(email);
            suggestBox.setText("");
        }
    }

    public void setUserInfo(String firstName, String lastName, String email) {
        sideBar.setUserInfo(firstName, lastName, email);
    }

    @UiHandler("projectNameDescription")
    void onProjectNameDescription(ClickEvent event) {
        if (isOwner) {
            this.projectNameTextBox.setText(projectInfo.getName());
            this.descriptionTextArea.setText(projectInfo.getDescription());
            projectNameDeck.showWidget(1);
        }
    }

    @UiHandler("updateButton")
    void onUpdate(ClickEvent event) {
        String newName = this.projectNameTextBox.getText();
        String newDesc = this.descriptionTextArea.getText();
        this.projectInfo.setName(newName);
        this.projectInfo.setDescription(newDesc);
        this.projectName.setInnerText(newName);
        this.description.setInnerText(newDesc);
        projectNameDeck.showWidget(0);
        this.listener.setProjectInfo(projectInfo);
    }

    @UiHandler("cancelButton")
    void onCancel(ClickEvent event) {
        projectNameDeck.showWidget(0);
    }


}
