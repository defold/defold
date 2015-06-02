package com.dynamo.cr.web2.client.mvp;

import com.dynamo.cr.web2.client.ClientFactory;
import com.dynamo.cr.web2.client.activity.DashboardActivity;
import com.dynamo.cr.web2.client.activity.DownloadsActivity;
import com.dynamo.cr.web2.client.activity.InviteActivity;
import com.dynamo.cr.web2.client.activity.LoginActivity;
import com.dynamo.cr.web2.client.activity.NewProjectActivity;
import com.dynamo.cr.web2.client.activity.OAuthActivity;
import com.dynamo.cr.web2.client.activity.ProjectActivity;
import com.dynamo.cr.web2.client.activity.SettingsActivity;
import com.dynamo.cr.web2.client.place.DashboardPlace;
import com.dynamo.cr.web2.client.place.DownloadsPlace;
import com.dynamo.cr.web2.client.place.InvitePlace;
import com.dynamo.cr.web2.client.place.LoginPlace;
import com.dynamo.cr.web2.client.place.NewProjectPlace;
import com.dynamo.cr.web2.client.place.OAuthPlace;
import com.dynamo.cr.web2.client.place.ProjectPlace;
import com.dynamo.cr.web2.client.place.SettingsPlace;
import com.google.gwt.activity.shared.Activity;
import com.google.gwt.activity.shared.ActivityMapper;
import com.google.gwt.place.shared.Place;

public class AppActivityMapper implements ActivityMapper {

	private ClientFactory clientFactory;

	/**
	 * AppActivityMapper associates each Place with its corresponding
	 * {@link Activity}
	 *
	 * @param clientFactory
	 *            Factory to be passed to activities
	 */
	public AppActivityMapper(ClientFactory clientFactory) {
		super();
		this.clientFactory = clientFactory;
	}

	/**
	 * Map each Place to its corresponding Activity. This would be a great use
	 * for GIN.
	 */
	@Override
	public Activity getActivity(Place place) {
		// This is begging for GIN

        if (place instanceof LoginPlace)
            return new LoginActivity((LoginPlace) place, clientFactory);
        else if (place instanceof DashboardPlace)
            return new DashboardActivity((DashboardPlace) place, clientFactory);
        else if (place instanceof ProjectPlace)
            return new ProjectActivity((ProjectPlace) place, clientFactory);
        else if (place instanceof NewProjectPlace)
            return new NewProjectActivity((NewProjectPlace) place, clientFactory);
        else if (place instanceof OAuthPlace)
            return new OAuthActivity((OAuthPlace) place, clientFactory);
        else if (place instanceof InvitePlace)
            return new InviteActivity((InvitePlace) place, clientFactory);
        else if (place instanceof DownloadsPlace)
            return new DownloadsActivity((DownloadsPlace) place, clientFactory);
        else if (place instanceof SettingsPlace)
            return new SettingsActivity((SettingsPlace) place, clientFactory);

		return null;
	}

}
