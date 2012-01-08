package com.dynamo.cr.web2.client.ui;

import java.util.Date;

import com.dynamo.cr.web2.client.CommitDesc;
import com.dynamo.cr.web2.client.Log;
import com.dynamo.cr.web2.client.ProjectInfo;
import com.dynamo.cr.web2.client.UserInfo;
import com.dynamo.cr.web2.client.UserInfoList;
import com.google.gwt.core.client.GWT;
import com.google.gwt.core.client.JsArray;
import com.google.gwt.dom.client.SpanElement;
import com.google.gwt.event.dom.client.BlurEvent;
import com.google.gwt.event.dom.client.BlurHandler;
import com.google.gwt.event.dom.client.ClickEvent;
import com.google.gwt.event.dom.client.FocusEvent;
import com.google.gwt.event.dom.client.FocusHandler;
import com.google.gwt.event.dom.client.KeyCodes;
import com.google.gwt.event.dom.client.KeyPressEvent;
import com.google.gwt.event.dom.client.KeyPressHandler;
import com.google.gwt.i18n.client.DateTimeFormat;
import com.google.gwt.uibinder.client.UiBinder;
import com.google.gwt.uibinder.client.UiField;
import com.google.gwt.uibinder.client.UiHandler;
import com.google.gwt.user.client.Window;
import com.google.gwt.user.client.ui.Button;
import com.google.gwt.user.client.ui.Composite;
import com.google.gwt.user.client.ui.HTMLPanel;
import com.google.gwt.user.client.ui.MultiWordSuggestOracle;
import com.google.gwt.user.client.ui.SuggestBox;
import com.google.gwt.user.client.ui.VerticalPanel;
import com.google.gwt.user.client.ui.Widget;
import com.google.gwt.user.datepicker.client.CalendarUtil;

public class ProjectView extends Composite implements KeyPressHandler {

    private static final String SUGGEST_BOX_BLUR = "enter email to add a member";
    public interface Presenter {
        void addMember(String email);
        void removeMember(int id);
        void deleteProject(ProjectInfo projectInfo);
    }

    private static ProjectDetailsUiBinder uiBinder = GWT
            .create(ProjectDetailsUiBinder.class);
    @UiField SpanElement projectName;
    @UiField Button deleteProject;
    @UiField SpanElement description;
    @UiField(provided = true) SuggestBox suggestBox;
    @UiField VerticalPanel members;
    @UiField VerticalPanel commits;
    @UiField HTMLPanel addMemberPanel;

    private final MultiWordSuggestOracle suggestions = new MultiWordSuggestOracle();
    private Presenter listener;
    private ProjectInfo projectInfo;

    interface ProjectDetailsUiBinder extends UiBinder<Widget, ProjectView> {
    }

    public ProjectView() {
        suggestBox = new SuggestBox(suggestions);
        /*
         * NOTE: Workaround for the following bug:
         * http://code.google.com/p/google-web-toolkit/issues/detail?id=3533 We
         * add handler to the textbox
         */
        suggestBox.getTextBox().addKeyPressHandler(this);
        initWidget(uiBinder.createAndBindUi(this));

        suggestBox.setText("enter email to add new member to project");
        suggestBox.getTextBox().addBlurHandler(new BlurHandler() {
            @Override
            public void onBlur(BlurEvent event) {
                suggestBox.setText(SUGGEST_BOX_BLUR);
            }
        });

        suggestBox.getTextBox().addFocusHandler(new FocusHandler() {
            @Override
            public void onFocus(FocusEvent event) {
                suggestBox.setText("");
            }
        });
    }

    public void clear() {
        this.projectName.setInnerText("");
        this.deleteProject.setVisible(false);
        this.description.setInnerText("");
        this.members.clear();
        this.commits.clear();
        this.addMemberPanel.setVisible(false);
        this.suggestBox.setText(SUGGEST_BOX_BLUR);
    }

    public void setProjectInfo(int userId, ProjectInfo projectInfo) {
        this.projectInfo = projectInfo;
        this.projectName.setInnerText(projectInfo.getName());
        String description = projectInfo.getDescription();
        if (description.isEmpty()) {
            description = "(no description)";
        }
        this.description.setInnerText(description);

        boolean isOwner = userId == projectInfo.getOwner().getId();
        this.deleteProject.setVisible(isOwner);
        this.addMemberPanel.setVisible(isOwner);

        this.members.clear();
        JsArray<UserInfo> membersList = projectInfo.getMembers();
        for (int i = 0; i < membersList.length(); ++i) {
            final UserInfo memberInfo = membersList.get(i);
            MemberBox memberBox = new MemberBox(this.listener, memberInfo, isOwner, memberInfo.getId() == userId, projectInfo.getOwner().getId() == memberInfo.getId());
            this.members.add(memberBox);
        }
    }

    public void setLog(int userId, Log log) {
        commits.clear();
        JsArray<CommitDesc> commits = log.getCommits();
        DateTimeFormat sourceDF = DateTimeFormat.getFormat("yyyy-MM-dd HH:mm:ss Z");
        Date now = new Date();
        for (int i = 0; i < commits.length(); ++i) {
            final CommitDesc commit = commits.get(i);
            Date commitDate = sourceDF.parse(commit.getDate());
            String date = formatDate(now, commitDate);
            CommitBox commitBox = new CommitBox(commit, date);
            this.commits.add(commitBox);
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

    @Override
    public void onKeyPress(KeyPressEvent event) {
        if (KeyCodes.KEY_ENTER == event.getNativeEvent().getKeyCode()) {
            listener.addMember(suggestBox.getText());
            suggestBox.setText("");
        }
    }

    public void setPresenter(Presenter listener) {
        this.listener = listener;
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

}
