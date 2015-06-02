package com.dynamo.cr.web2.client.ui;

import com.dynamo.cr.web2.client.ProjectInfo;
import com.dynamo.cr.web2.client.ProjectInfoList;
import com.dynamo.cr.web2.client.UserInfo;
import com.google.gwt.core.client.GWT;
import com.google.gwt.core.client.JsArray;
import com.google.gwt.dom.client.Document;
import com.google.gwt.dom.client.Node;
import com.google.gwt.dom.client.NodeList;
import com.google.gwt.dom.client.TableCellElement;
import com.google.gwt.dom.client.TableRowElement;
import com.google.gwt.dom.client.TableSectionElement;
import com.google.gwt.event.dom.client.ClickEvent;
import com.google.gwt.event.dom.client.ClickHandler;
import com.google.gwt.uibinder.client.UiBinder;
import com.google.gwt.uibinder.client.UiField;
import com.google.gwt.uibinder.client.UiHandler;
import com.google.gwt.user.client.Element;
import com.google.gwt.user.client.ui.Anchor;
import com.google.gwt.user.client.ui.Button;
import com.google.gwt.user.client.ui.Composite;
import com.google.gwt.user.client.ui.FlexTable;
import com.google.gwt.user.client.ui.FlowPanel;
import com.google.gwt.user.client.ui.Widget;

public class DashboardView extends Composite {

    public interface Presenter {
        void showProject(ProjectInfo projectInfo);
        void onNewProject();
        void removeProject(ProjectInfo projectInfo);
        boolean isOwner(ProjectInfo projectInfo);
    }

    private static DashboardUiBinder uiBinder = GWT
            .create(DashboardUiBinder.class);
    @UiField SideBar sideBar;
    @UiField FlexTable projects;
    @UiField Button newProjectButton;

    interface DashboardUiBinder extends UiBinder<Widget, DashboardView> {
    }

    private Presenter listener;

    public DashboardView() {
        initWidget(uiBinder.createAndBindUi(this));
        Element element = projects.getElement();
        element.addClassName("table");
        element.addClassName("table-striped");
        newProjectButton.addStyleName("btn btn-primary");
    }

    public void setProjectInfoList(int userId, ProjectInfoList projectInfoList) {
        clearProjectInfoList();

        JsArray<ProjectInfo> projects = projectInfoList.getProjects();
        for (int i = 0; i < projects.length(); ++i) {
            final ProjectInfo projectInfo = projects.get(i);
            UserInfo owner = projectInfo.getOwner();

            FlowPanel panel = new FlowPanel();
            Anchor anchor = new Anchor(projectInfo.getName());
            anchor.addClickHandler(new ClickHandler() {
                @Override
                public void onClick(ClickEvent event) {
                    listener.showProject(projectInfo);
                }
            });
            panel.add(anchor);
            this.projects.setWidget(i, 0, panel);
            this.projects.setText(i, 1, owner.getFirstName() + " " + owner.getLastName());
            this.projects.setText(i, 2, "");
        }

        /* Create table header. Not supported by the FlexGrid widget */
        Document document = Document.get();
        TableSectionElement thead = document.createTHeadElement();
        TableRowElement tr = document.createTRElement();
        thead.appendChild(tr);
        final TableCellElement th1 = document.createTHElement();
        th1.setInnerText("Name");
        final TableCellElement th2 = document.createTHElement();
        th2.setInnerText("Owner");
        tr.appendChild(th1);
        tr.appendChild(th2);
        this.projects.getElement().insertFirst(thead);
    }

    public void clearProjectInfoList() {
        projects.clear();
        projects.removeAllRows();
        /* Remove table header if exists. See above */
        Element e = projects.getElement();
        NodeList<Node> nodes = e.getChildNodes();
        if (nodes.getLength() > 0 && nodes.getItem(0).getNodeName().equalsIgnoreCase("thead")) {
            e.removeChild(nodes.getItem(0));
        }
    }

    public void setPresenter(DashboardView.Presenter listener) {
        this.listener = listener;
        sideBar.setActivePage("projects");
    }

    @UiHandler("newProjectButton")
    void onNewProjectButtonClick(ClickEvent event) {
        this.listener.onNewProject();
    }

    public void setUserInfo(String firstName, String lastName, String email) {
        sideBar.setUserInfo(firstName, lastName, email);
    }

}
