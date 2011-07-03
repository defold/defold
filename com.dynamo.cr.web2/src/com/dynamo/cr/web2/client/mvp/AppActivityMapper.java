package com.dynamo.cr.web2.client.mvp;

import com.dynamo.cr.web2.client.ClientFactory;
import com.dynamo.cr.web2.client.activity.DashboardActivity;
import com.dynamo.cr.web2.client.activity.LoginActivity;
import com.dynamo.cr.web2.client.activity.ProductInfoActivity;
import com.dynamo.cr.web2.client.activity.ProjectActivity;
import com.dynamo.cr.web2.client.place.DashboardPlace;
import com.dynamo.cr.web2.client.place.LoginPlace;
import com.dynamo.cr.web2.client.place.ProductInfoPlace;
import com.dynamo.cr.web2.client.place.ProjectPlace;
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
        else if (place instanceof ProductInfoPlace)
            return new ProductInfoActivity((ProductInfoPlace) place, clientFactory);
        else if (place instanceof DashboardPlace)
            return new DashboardActivity((DashboardPlace) place, clientFactory);
        else if (place instanceof ProjectPlace)
            return new ProjectActivity((ProjectPlace) place, clientFactory);

		return null;
	}

}
