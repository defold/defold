package com.dynamo.cr.web2.client;

import com.dynamo.cr.web2.client.ui.BlogView;
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
import com.google.gwt.event.shared.SimpleEventBus;
import com.google.gwt.place.shared.PlaceController;
import com.google.web.bindery.event.shared.EventBus;

public class ClientFactoryImpl implements ClientFactory {
    private static final EventBus eventBus = new SimpleEventBus();
    private static final PlaceController placeController = new PlaceController(
            eventBus);
    private static final LoginView loginView = new LoginView();
    private static final ProductInfoView productInfoView = new ProductInfoView();
    private static final DashboardView dashboardView = new DashboardView();
    private static final ProjectView projectView = new ProjectView();
    private static final NewProjectView newProjectView = new NewProjectView();
    private static final OpenIDView openIDView = new OpenIDView();
    private static final DocumentationView documentationView = new DocumentationView();
    private static final TutorialsView tutorialsView = new TutorialsView();
    private static final BlogView blogView = new BlogView();
    private static final ScriptSampleView scriptSampleView = new ScriptSampleView();
    private static final ReferenceView referenceView = new ReferenceView();
    private static final GuideView guideView = new GuideView();

    private Defold defold;

    @Override
    public void setDefold(Defold defold) {
        this.defold = defold;
    }

    @Override
    public Defold getDefold() {
        return defold;
    }

    @Override
    public EventBus getEventBus() {
        return eventBus;
    }

    @Override
    public PlaceController getPlaceController() {
        return placeController;
    }

    @Override
    public LoginView getLoginView() {
        return loginView;
    }

    @Override
    public ProductInfoView getProductInfoView() {
        return productInfoView;
    }

    @Override
    public DashboardView getDashboardView() {
        return dashboardView;
    }

    @Override
    public ProjectView getProjectView() {
        return projectView;
    }

    @Override
    public NewProjectView getNewProjectView() {
        return newProjectView;
    }

    @Override
    public OpenIDView getOpenIDView() {
        return openIDView;
    }

    @Override
    public DocumentationView getDocumentationView() {
        return documentationView;
    }

    @Override
    public TutorialsView getTutorialsView() {
        return tutorialsView;
    }

    @Override
    public BlogView getBlogView() {
        return blogView;
    }

    @Override
    public ScriptSampleView getScriptSampleView() {
        return scriptSampleView;
    }

    @Override
    public ReferenceView getReferenceView() {
        return referenceView;
    }

    @Override
    public GuideView getGuideView() {
        return guideView;
    }

}
