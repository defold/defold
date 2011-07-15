package com.dynamo.cr.web2.client;

import com.dynamo.cr.web2.client.ui.DashboardView;
import com.dynamo.cr.web2.client.ui.LoginView;
import com.dynamo.cr.web2.client.ui.NewProjectView;
import com.dynamo.cr.web2.client.ui.OpenIDView;
import com.dynamo.cr.web2.client.ui.ProductInfoView;
import com.dynamo.cr.web2.client.ui.ProjectView;
import com.google.gwt.event.shared.EventBus;
import com.google.gwt.event.shared.SimpleEventBus;
import com.google.gwt.place.shared.PlaceController;

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

}
