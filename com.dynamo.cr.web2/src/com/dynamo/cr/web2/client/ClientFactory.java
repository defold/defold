package com.dynamo.cr.web2.client;

import com.dynamo.cr.web2.client.ui.DashboardView;
import com.dynamo.cr.web2.client.ui.DocumentationView;
import com.dynamo.cr.web2.client.ui.LoginView;
import com.dynamo.cr.web2.client.ui.NewProjectView;
import com.dynamo.cr.web2.client.ui.OpenIDView;
import com.dynamo.cr.web2.client.ui.ProductInfoView;
import com.dynamo.cr.web2.client.ui.ProjectView;
import com.google.gwt.event.shared.EventBus;
import com.google.gwt.place.shared.PlaceController;

public interface ClientFactory
{
    void setDefold(Defold defold);
    Defold getDefold();
	EventBus getEventBus();
	PlaceController getPlaceController();
    LoginView getLoginView();
    ProductInfoView getProductInfoView();
    DashboardView getDashboardView();
    ProjectView getProjectView();
    NewProjectView getNewProjectView();
    OpenIDView getOpenIDView();
    DocumentationView getDocumentationView();
}
