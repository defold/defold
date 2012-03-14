package com.dynamo.cr.web2.client;

import com.dynamo.cr.web2.client.ui.BlogView;
import com.dynamo.cr.web2.client.ui.ContentView;
import com.dynamo.cr.web2.client.ui.DashboardView;
import com.dynamo.cr.web2.client.ui.DocumentationView;
import com.dynamo.cr.web2.client.ui.GuideView;
import com.dynamo.cr.web2.client.ui.LoginView;
import com.dynamo.cr.web2.client.ui.NewProjectView;
import com.dynamo.cr.web2.client.ui.OpenIDView;
import com.dynamo.cr.web2.client.ui.ProductInfoView;
import com.dynamo.cr.web2.client.ui.ProjectView;
import com.dynamo.cr.web2.client.ui.ReferenceView;
import com.dynamo.cr.web2.client.ui.ScriptSampleView;
import com.dynamo.cr.web2.client.ui.TutorialsView;
import com.google.gwt.place.shared.PlaceController;
import com.google.web.bindery.event.shared.EventBus;

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
    TutorialsView getTutorialsView();
    BlogView getBlogView();
    ScriptSampleView getScriptSampleView();
    ReferenceView getReferenceView();
    GuideView getGuideView();
    ContentView getContentView();
}
