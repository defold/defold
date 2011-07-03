package com.dynamo.cr.web2.client.ui;

import com.dynamo.cr.web2.client.ProjectInfo;
import com.dynamo.cr.web2.client.UserInfo;
import com.dynamo.cr.web2.client.UserInfoList;
import com.google.gwt.cell.client.ButtonCell;
import com.google.gwt.cell.client.FieldUpdater;
import com.google.gwt.core.client.GWT;
import com.google.gwt.core.client.JsArray;
import com.google.gwt.event.dom.client.KeyCodes;
import com.google.gwt.event.dom.client.KeyPressEvent;
import com.google.gwt.event.dom.client.KeyPressHandler;
import com.google.gwt.resources.client.CssResource;
import com.google.gwt.safehtml.shared.SafeHtml;
import com.google.gwt.safehtml.shared.SafeHtmlBuilder;
import com.google.gwt.uibinder.client.UiBinder;
import com.google.gwt.uibinder.client.UiField;
import com.google.gwt.user.cellview.client.CellTable;
import com.google.gwt.user.cellview.client.Column;
import com.google.gwt.user.cellview.client.TextColumn;
import com.google.gwt.user.client.ui.Composite;
import com.google.gwt.user.client.ui.InlineLabel;
import com.google.gwt.user.client.ui.Label;
import com.google.gwt.user.client.ui.MultiWordSuggestOracle;
import com.google.gwt.user.client.ui.SuggestBox;
import com.google.gwt.user.client.ui.Widget;
import com.google.gwt.view.client.ListDataProvider;

public class ProjectView extends Composite implements KeyPressHandler {

    public interface Presenter {
        void addMember(String email);
        void removeMember(int id);
    }

    private static ProjectDetailsUiBinder uiBinder = GWT
            .create(ProjectDetailsUiBinder.class);
    @UiField
    InlineLabel projectName;
    @UiField
    Label description;
    @UiField(provided = true)
    SuggestBox suggestBox;
    @UiField(provided = true)
    CellTable<UserInfo> membersTable = new CellTable<UserInfo>();

    interface Css extends CssResource {
        String member();
    }

    @UiField
    Css style;

    private final MultiWordSuggestOracle suggestions = new MultiWordSuggestOracle();
    private ListDataProvider<UserInfo> membersProvider;
    private Presenter listener;

    interface ProjectDetailsUiBinder extends UiBinder<Widget, ProjectView> {
    }

    public ProjectView() {
        suggestBox = new SuggestBox(suggestions);
        /*
         * NOTE: Workaround for the followin bug:
         * http://code.google.com/p/google-web-toolkit/issues/detail?id=3533 We
         * add handler to the textbox
         */
        suggestBox.getTextBox().addKeyPressHandler(this);
        initWidget(uiBinder.createAndBindUi(this));

        membersProvider = new ListDataProvider<UserInfo>();

        TextColumn<UserInfo> firstNameColumn = new TextColumn<UserInfo>() {
            @Override
            public String getValue(UserInfo userInfo) {
                return userInfo.getFirstName();
            }
        };

        TextColumn<UserInfo> lastNameColumn = new TextColumn<UserInfo>() {
            @Override
            public String getValue(UserInfo userInfo) {
                return userInfo.getLastName();
            }
        };

        TextColumn<UserInfo> emailColumn = new TextColumn<UserInfo>() {
            @Override
            public String getValue(UserInfo userInfo) {
                return userInfo.getEmail();
            }
        };
        ButtonCell buttonCell = new ButtonCell() {
            @Override
            public void render(Context context, SafeHtml data,
                    SafeHtmlBuilder sb) {

                sb.appendHtmlConstant("<button type=\"button\" tabindex=\"-1\">");

                // TODO: Fix this stuff back...
                /*
                 * if (projectInfo.getOwner().getId() == defold.getUserId()) {
                 * sb
                 * .appendHtmlConstant("<button type=\"button\" tabindex=\"-1\">"
                 * ); } else { sb.appendHtmlConstant(
                 * "<button disabled=\"disabled\" type=\"button\" tabindex=\"-1\">"
                 * ); }
                 */

                if (data != null) {
                    sb.append(data);
                }
                sb.appendHtmlConstant("</button>");
            }
        };

        Column<UserInfo, String> removeColumn = new Column<UserInfo, String>(
                buttonCell) {
            @Override
            public String getValue(UserInfo object) {
                return "x";
            }
        };

        removeColumn.setFieldUpdater(new FieldUpdater<UserInfo, String>() {
            @Override
            public void update(final int index, UserInfo userInfo, String value) {
                listener.removeMember(userInfo.getId());
            }
        });

        membersTable.addColumn(firstNameColumn);
        membersTable.addColumn(lastNameColumn);
        membersTable.addColumn(emailColumn);
        membersTable.addColumn(removeColumn);
        membersProvider.addDataDisplay(membersTable);
    }

    public void setProjectInfo(ProjectInfo projectInfo) {
        projectName.setText(projectInfo.getName());
        description.setText(projectInfo.getDescription());
        suggestBox.setText("");
        JsArray<UserInfo> membersList = projectInfo.getMembers();
        membersProvider.getList().clear();
        for (int i = 0; i < membersList.length(); ++i) {
            UserInfo member = membersList.get(i);
            if (member.getId() != projectInfo.getOwner().getId()) {
                membersProvider.getList().add(member);
            }
        }
    }

    @Override
    public void onKeyPress(KeyPressEvent event) {
        if (KeyCodes.KEY_ENTER == event.getCharCode()) {
            listener.addMember(suggestBox.getText());
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

    public void init() {
        membersProvider.getList().clear();
        suggestions.clear();
        projectName.setText("...");
        description.setText("...");
        suggestBox.setText("");
    }
}
