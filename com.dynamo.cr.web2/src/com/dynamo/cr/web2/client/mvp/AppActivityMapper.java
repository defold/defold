package com.dynamo.cr.web2.client.mvp;

import com.dynamo.cr.web2.client.ClientFactory;
import com.dynamo.cr.web2.client.activity.BlogActivity;
import com.dynamo.cr.web2.client.activity.DashboardActivity;
import com.dynamo.cr.web2.client.activity.DocumentationActivity;
import com.dynamo.cr.web2.client.activity.LoginActivity;
import com.dynamo.cr.web2.client.activity.NewProjectActivity;
import com.dynamo.cr.web2.client.activity.OpenIDActivity;
import com.dynamo.cr.web2.client.activity.ProductInfoActivity;
import com.dynamo.cr.web2.client.activity.ProjectActivity;
import com.dynamo.cr.web2.client.activity.TutorialsActivity;
import com.dynamo.cr.web2.client.place.BlogPlace;
import com.dynamo.cr.web2.client.place.DashboardPlace;
import com.dynamo.cr.web2.client.place.DocumentationPlace;
import com.dynamo.cr.web2.client.place.LoginPlace;
import com.dynamo.cr.web2.client.place.NewProjectPlace;
import com.dynamo.cr.web2.client.place.OpenIDPlace;
import com.dynamo.cr.web2.client.place.ProductInfoPlace;
import com.dynamo.cr.web2.client.place.ProjectPlace;
import com.dynamo.cr.web2.client.place.TutorialsPlace;
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
        else if (place instanceof NewProjectPlace)
            return new NewProjectActivity((NewProjectPlace) place, clientFactory);
        else if (place instanceof OpenIDPlace)
            return new OpenIDActivity((OpenIDPlace) place, clientFactory);
        else if (place instanceof DocumentationPlace)
            return new DocumentationActivity((DocumentationPlace) place, clientFactory);
        else if (place instanceof TutorialsPlace)
            return new TutorialsActivity((TutorialsPlace) place, clientFactory);
        else if (place instanceof BlogPlace)
            return new BlogActivity((BlogPlace) place, clientFactory);

		return null;
	}

}
