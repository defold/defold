package com.dynamo.cr.web2.client.activity;

import com.dynamo.cr.web2.client.ClientFactory;
import com.dynamo.cr.web2.client.Defold;
import com.dynamo.cr.web2.client.ProjectTemplateInfoList;
import com.dynamo.cr.web2.client.ResourceCallback;
import com.dynamo.cr.web2.client.place.DashboardPlace;
import com.dynamo.cr.web2.client.place.NewProjectPlace;
import com.dynamo.cr.web2.client.ui.NewProjectView;
import com.google.gwt.activity.shared.AbstractActivity;
import com.google.gwt.event.shared.EventBus;
import com.google.gwt.http.client.Request;
import com.google.gwt.http.client.Response;
import com.google.gwt.json.client.JSONObject;
import com.google.gwt.json.client.JSONString;
import com.google.gwt.user.client.ui.AcceptsOneWidget;

public class NewProjectActivity extends AbstractActivity implements NewProjectView.Presenter {
	private ClientFactory clientFactory;

	public NewProjectActivity(NewProjectPlace place, ClientFactory clientFactory) {
		this.clientFactory = clientFactory;
	}

	@Override
	public void start(AcceptsOneWidget containerWidget, EventBus eventBus) {
		final NewProjectView newProjectView = clientFactory.getNewProjectView();
		newProjectView.setPresenter(this);

		Defold defold = clientFactory.getDefold();
		defold.getResource("/repository/project_templates", new ResourceCallback<ProjectTemplateInfoList>() {

            @Override
            public void onSuccess(ProjectTemplateInfoList result, Request request,
                    Response response) {
                newProjectView.setProjectTemplates(result);
            }

            @Override
            public void onFailure(Request request, Response response) {
            }
        });

		containerWidget.setWidget(newProjectView.asWidget());
	}

    @Override
    public void createProject(String name, String description,
            String templateId) {
        System.out.println(name);
        System.out.println(description);
        System.out.println(templateId);
        final NewProjectView newProjectView = clientFactory.getNewProjectView();

        JSONObject newProject = new JSONObject();
        newProject.put("name", new JSONString(name));
        newProject.put("description", new JSONString(description));
        newProject.put("templateId", new JSONString(templateId));

        Defold defold = clientFactory.getDefold();
        defold.postResource("/projects/" + defold.getUserId(), newProject.toString(), new ResourceCallback<String>() {

            @Override
            public void onSuccess(String result, Request request,
                    Response response) {

                int statusCode = response.getStatusCode();
                System.out.println(statusCode);
                if (statusCode >= 300) {
                    newProjectView.setError(response.getText());
                } else {
                    clientFactory.getPlaceController().goTo(new DashboardPlace());
                }
            }

            @Override
            public void onFailure(Request request, Response response) {
            }
        });

    }
}