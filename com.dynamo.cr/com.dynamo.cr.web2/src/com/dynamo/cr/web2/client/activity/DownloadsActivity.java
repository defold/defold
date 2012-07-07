package com.dynamo.cr.web2.client.activity;

import com.dynamo.cr.web2.client.ClientFactory;
import com.dynamo.cr.web2.client.Defold;
import com.dynamo.cr.web2.client.place.DownloadsPlace;
import com.dynamo.cr.web2.client.ui.DownloadsView;
import com.google.gwt.activity.shared.AbstractActivity;
import com.google.gwt.event.shared.EventBus;
import com.google.gwt.user.client.ui.AcceptsOneWidget;

public class DownloadsActivity extends AbstractActivity implements DownloadsView.Presenter {
    private ClientFactory clientFactory;

    public DownloadsActivity(DownloadsPlace place, ClientFactory clientFactory) {
        this.clientFactory = clientFactory;
    }

    @Override
    public void start(AcceptsOneWidget containerWidget, EventBus eventBus) {
        final DownloadsView downloadsView = clientFactory.getDownloadsView();
        containerWidget.setWidget(downloadsView.asWidget());
        downloadsView.setPresenter(this);
        loadGravatar();
    }

    private void loadGravatar() {
        final DownloadsView downloadsView = clientFactory.getDownloadsView();
        Defold defold = clientFactory.getDefold();
        String email = defold.getEmail();

        String firstName = defold.getFirstName();
        String lastName = defold.getLastName();
        downloadsView.setUserInfo(firstName, lastName, email);
    }

}
