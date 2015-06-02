package com.dynamo.cr.web2.client;

import com.dynamo.cr.web2.client.ui.DashboardView;
import com.dynamo.cr.web2.client.ui.DownloadsView;
import com.dynamo.cr.web2.client.ui.InviteView;
import com.dynamo.cr.web2.client.ui.LoginView;
import com.dynamo.cr.web2.client.ui.NewProjectView;
import com.dynamo.cr.web2.client.ui.OAuthView;
import com.dynamo.cr.web2.client.ui.ProjectView;
import com.dynamo.cr.web2.client.ui.SettingsView;
import com.google.gwt.place.shared.PlaceController;
import com.google.web.bindery.event.shared.EventBus;

public interface ClientFactory
{
    void setDefold(Defold defold);
    Defold getDefold();
	EventBus getEventBus();
	PlaceController getPlaceController();
    LoginView getLoginView();
    DashboardView getDashboardView();
    ProjectView getProjectView();
    NewProjectView getNewProjectView();
    OAuthView getOAuthView();
    InviteView getInviteView();
    DownloadsView getDownloadsView();
    SettingsView getSettingsView();
}
