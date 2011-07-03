package com.dynamo.cr.web2.client;

import com.google.gwt.core.client.GWT;
import com.google.gwt.core.client.JsArray;
import com.google.gwt.event.dom.client.ClickEvent;
import com.google.gwt.http.client.Request;
import com.google.gwt.http.client.Response;
import com.google.gwt.uibinder.client.UiBinder;
import com.google.gwt.uibinder.client.UiField;
import com.google.gwt.uibinder.client.UiHandler;
import com.google.gwt.user.cellview.client.CellTable;
import com.google.gwt.user.cellview.client.TextColumn;
import com.google.gwt.user.client.ui.Anchor;
import com.google.gwt.user.client.ui.Composite;
import com.google.gwt.user.client.ui.DeckPanel;
import com.google.gwt.user.client.ui.Widget;
import com.google.gwt.view.client.ListDataProvider;
import com.google.gwt.view.client.NoSelectionModel;
import com.google.gwt.view.client.SelectionChangeEvent;
import com.google.gwt.view.client.SelectionChangeEvent.Handler;

public class Dashboard extends Composite {

    private static DashboardUiBinder uiBinder = GWT
            .create(DashboardUiBinder.class);
    @UiField CellTable<ProjectInfo> projectsTable;
    @UiField DeckPanel deckPanel;
    @UiField Anchor back;
    @UiField ProjectDetails projectDetails;

    interface DashboardUiBinder extends UiBinder<Widget, Dashboard> {
    }

    private ListDataProvider<ProjectInfo> projectTableDataProvider;
    private Defold defold;

    public Dashboard() {
        initWidget(uiBinder.createAndBindUi(this));
        TextColumn<ProjectInfo> nameColumn = new TextColumn<ProjectInfo>() {
            @Override
            public String getValue(ProjectInfo projectInfo) {
              return projectInfo.getName();
            }
        };

        TextColumn<ProjectInfo> descriptionColumn = new TextColumn<ProjectInfo>() {
            @Override
            public String getValue(ProjectInfo projectInfo) {
              return projectInfo.getDescription();
            }
        };

        TextColumn<ProjectInfo> ownerColumn = new TextColumn<ProjectInfo>() {
            @Override
            public String getValue(ProjectInfo projectInfo) {
              return projectInfo.getOwner().getEmail();
            }
        };

        projectsTable.addColumn(nameColumn, "Name");
        projectsTable.addColumn(descriptionColumn, "Description");
        projectsTable.addColumn(ownerColumn, "Owner");

        projectTableDataProvider = new ListDataProvider<ProjectInfo>();
        projectTableDataProvider.addDataDisplay(projectsTable);

        final NoSelectionModel<ProjectInfo> selectionModel = new NoSelectionModel<ProjectInfo>();
        projectsTable.setSelectionModel(selectionModel);
        projectsTable.getSelectionModel().addSelectionChangeHandler(new Handler() {

            @Override
            public void onSelectionChange(SelectionChangeEvent event) {
                ProjectInfo projectInfo = selectionModel.getLastSelectedObject();
                projectDetails.setProjectInfo(projectInfo);
                deckPanel.showWidget(1);
            }
        });

        deckPanel.showWidget(0);
    }

    void load() {
        defold.getResource("/projects/" + defold.getUserId(), new ResourceCallback<ProjectInfoList>() {

            @Override
            public void onSuccess(ProjectInfoList projectInfoList, Request request,
                    Response response) {
                projectTableDataProvider.getList().clear();
                JsArray<ProjectInfo> projects = projectInfoList.getProjects();
                for (int i = 0; i < projects.length(); ++i) {
                    projectTableDataProvider.getList().add(projects.get(i));
                }
            }

            @Override
            public void onFailure(Request request, Response response) {
                projectTableDataProvider.getList().clear();
            }
        });
    }

    @UiHandler("back")
    void onBackClick(ClickEvent event) {
        deckPanel.showWidget(0);
    }

    public void setDefold(Defold defold) {
        this.defold = defold;
        projectDetails.setDefold(defold);
    }
}
