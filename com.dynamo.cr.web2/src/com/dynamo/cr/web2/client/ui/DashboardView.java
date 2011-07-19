package com.dynamo.cr.web2.client.ui;

import com.dynamo.cr.web2.client.ProjectInfo;
import com.dynamo.cr.web2.client.ProjectInfoList;
import com.dynamo.cr.web2.client.UserInfo;
import com.google.gwt.cell.client.ActionCell;
import com.google.gwt.cell.client.ClickableTextCell;
import com.google.gwt.cell.client.FieldUpdater;
import com.google.gwt.core.client.GWT;
import com.google.gwt.core.client.JsArray;
import com.google.gwt.event.dom.client.ClickEvent;
import com.google.gwt.safehtml.shared.SafeHtmlBuilder;
import com.google.gwt.uibinder.client.UiBinder;
import com.google.gwt.uibinder.client.UiField;
import com.google.gwt.uibinder.client.UiHandler;
import com.google.gwt.user.cellview.client.CellTable;
import com.google.gwt.user.cellview.client.Column;
import com.google.gwt.user.cellview.client.IdentityColumn;
import com.google.gwt.user.client.ui.Button;
import com.google.gwt.user.client.ui.Composite;
import com.google.gwt.user.client.ui.Label;
import com.google.gwt.user.client.ui.Widget;
import com.google.gwt.view.client.ListDataProvider;

public class DashboardView extends Composite {

    public interface Presenter {

        void showProject(ProjectInfo projectInfo);

        void onNewProject();

        void removeProject(ProjectInfo projectInfo);

        boolean isOwner(ProjectInfo projectInfo);

    }

    private static DashboardUiBinder uiBinder = GWT
            .create(DashboardUiBinder.class);
    @UiField CellTable<ProjectInfo> projectsTable;
    @UiField Button newProjectButton;
    @UiField Label errorLabel;

    interface DashboardUiBinder extends UiBinder<Widget, DashboardView> {
    }

    private ListDataProvider<ProjectInfo> projectTableDataProvider;
    private Presenter listener;

    public DashboardView() {
        initWidget(uiBinder.createAndBindUi(this));

        errorLabel.setText("");

        FieldUpdater<ProjectInfo, String> fieldUpdater = new FieldUpdater<ProjectInfo, String>() {
            @Override
            public void update(int index, ProjectInfo object, String value) {
                listener.showProject(object);
            }
        };

        ClickableTextCell nameCell = new ClickableTextCell();
        Column<ProjectInfo, String> nameColumn = new Column<ProjectInfo, String>(nameCell) {
            @Override
            public String getValue(ProjectInfo object) {
                return object.getName();
            }
        };
        nameColumn.setFieldUpdater(fieldUpdater);

        ClickableTextCell descCell = new ClickableTextCell();
        Column<ProjectInfo, String> descColumn = new Column<ProjectInfo, String>(descCell) {
            @Override
            public String getValue(ProjectInfo object) {
                return object.getDescription();
            }
        };
        descColumn.setFieldUpdater(fieldUpdater);

        ClickableTextCell ownerCell = new ClickableTextCell();
        Column<ProjectInfo, String> ownerColumn = new Column<ProjectInfo, String>(ownerCell) {
            @Override
            public String getValue(ProjectInfo object) {
                return object.getName();
            }
        };
        ownerColumn.setFieldUpdater(fieldUpdater);

        ActionCell.Delegate<ProjectInfo> delegate = new ActionCell.Delegate<ProjectInfo>() {

            @Override
            public void execute(ProjectInfo object) {
                listener.removeProject(object);
            }
        };
        final ActionCell<ProjectInfo> deleteCell = new ActionCell<ProjectInfo>("x", delegate) {
            @Override
            public void render(com.google.gwt.cell.client.Cell.Context context,
                    ProjectInfo value, SafeHtmlBuilder sb) {
                if (listener.isOwner(value)) {
                    sb.appendHtmlConstant("<button type=\"button\" tabindex=\"-1\" title=\"Delete\">x</button>");
                } else {
                    sb.appendHtmlConstant("<button disabled=\"disabled\" type=\"button\" title=\"Only the project owner can delete a project\">x</button>");
                }
            }
        };

        IdentityColumn<ProjectInfo> deleteColumn = new IdentityColumn<ProjectInfo>(deleteCell);

        projectsTable.addColumn(nameColumn, "Name");
        projectsTable.addColumn(descColumn, "Description");
        projectsTable.addColumn(ownerColumn, "Owner");
        projectsTable.addColumn(deleteColumn);

        projectTableDataProvider = new ListDataProvider<ProjectInfo>();
        projectTableDataProvider.addDataDisplay(projectsTable);

    }

    public void setProjectInfoList(ProjectInfoList projectInfoList) {
        projectTableDataProvider.getList().clear();
        JsArray<ProjectInfo> projects = projectInfoList.getProjects();
        for (int i = 0; i < projects.length(); ++i) {
            projectTableDataProvider.getList().add(projects.get(i));
        }
    }

    public void clearProjectInfoList() {
        projectTableDataProvider.getList().clear();
    }

    public void setPresenter(DashboardView.Presenter listener) {
        this.listener = listener;
    }

    @UiHandler("newProjectButton")
    void onNewProjectButtonClick(ClickEvent event) {
        this.listener.onNewProject();
    }

    public void setError(String message) {
        errorLabel.setText(message);
    }
}
