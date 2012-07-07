package com.dynamo.cr.web2.client;

import com.dynamo.cr.web2.client.ui.DashboardView;
import com.dynamo.cr.web2.client.ui.DownloadsView;
import com.dynamo.cr.web2.client.ui.InviteView;
import com.dynamo.cr.web2.client.ui.LoginView;
import com.dynamo.cr.web2.client.ui.NewProjectView;
import com.dynamo.cr.web2.client.ui.OpenIDView;
import com.dynamo.cr.web2.client.ui.ProjectView;
import com.google.gwt.event.shared.SimpleEventBus;
import com.google.gwt.place.shared.PlaceController;
import com.google.web.bindery.event.shared.EventBus;

public class ClientFactoryImpl implements ClientFactory {
    private static final EventBus eventBus = new SimpleEventBus();
    private static final PlaceController placeController = new PlaceController(
            eventBus);
    private static final LoginView loginView = new LoginView();
    private static final DashboardView dashboardView = new DashboardView();
    private static final ProjectView projectView = new ProjectView();
    private static final NewProjectView newProjectView = new NewProjectView();
    private static final OpenIDView openIDView = new OpenIDView();
    private static final InviteView inviteView = new InviteView();
    private static final DownloadsView downloadsView = new DownloadsView();

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
    public InviteView getInviteView() {
        return inviteView;
    }

    @Override
    public DownloadsView getDownloadsView() {
        return downloadsView;
    }

}
