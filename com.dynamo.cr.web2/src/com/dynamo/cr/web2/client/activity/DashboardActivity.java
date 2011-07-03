package com.dynamo.cr.web2.client.activity;

import com.dynamo.cr.web2.client.ClientFactory;
import com.dynamo.cr.web2.client.Defold;
import com.dynamo.cr.web2.client.ProjectInfo;
import com.dynamo.cr.web2.client.ProjectInfoList;
import com.dynamo.cr.web2.client.ResourceCallback;
import com.dynamo.cr.web2.client.place.DashboardPlace;
import com.dynamo.cr.web2.client.place.ProjectPlace;
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

	@Override
	public void start(AcceptsOneWidget containerWidget, EventBus eventBus) {
		final DashboardView dashboardView = clientFactory.getDashboardView();
		dashboardView.setPresenter(this);
		containerWidget.setWidget(dashboardView.asWidget());

		Defold defold = clientFactory.getDefold();
        defold.getResource("/projects/" + defold.getUserId(), new ResourceCallback<ProjectInfoList>() {

            @Override
            public void onSuccess(ProjectInfoList projectInfoList, Request request,
                    Response response) {
                dashboardView.setProjectInfoList(projectInfoList);
            }

            @Override
            public void onFailure(Request request, Response response) {
                dashboardView.clearProjectInfoList();
            }
        });
	}

    @Override
    public void showProject(ProjectInfo projectInfo) {
        clientFactory.getPlaceController().goTo(new ProjectPlace(Integer.toString(projectInfo.getId())));
    }
}