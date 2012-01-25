package com.dynamo.cr.web2.client.activity;

import com.dynamo.cr.web2.client.ClientFactory;
import com.dynamo.cr.web2.client.place.GettingStartedPlace;
import com.dynamo.cr.web2.client.ui.GettingStartedView;
import com.google.gwt.event.shared.EventBus;
import com.google.gwt.user.client.ui.AcceptsOneWidget;

public class GettingStartedActivity extends AsciiDocActivity implements GettingStartedView.Presenter {
    public GettingStartedActivity(GettingStartedPlace place, ClientFactory clientFactory) {
        super(clientFactory);
    }

    @Override
    public void start(AcceptsOneWidget containerWidget, EventBus eventBus) {
        final GettingStartedView gettingStartedView = clientFactory.getGettingStartedView();
        containerWidget.setWidget(gettingStartedView.asWidget());
        gettingStartedView.setPresenter(this);
        loadAsciiDoc(gettingStartedView, "getting_started");
    }

    @Override
    public void onGettingStarted() {
        clientFactory.getPlaceController().goTo(new GettingStartedPlace());
    }
}
