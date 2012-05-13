package com.dynamo.cr.web2.client.activity;

import com.dynamo.cr.web2.client.ClientFactory;
import com.dynamo.cr.web2.client.place.DocumentationPlace;
import com.dynamo.cr.web2.client.ui.DocumentationView;
import com.google.gwt.activity.shared.AbstractActivity;
import com.google.gwt.event.shared.EventBus;
import com.google.gwt.user.client.ui.AcceptsOneWidget;

public class DocumentationActivity extends AbstractActivity {
    private ClientFactory clientFactory;

    public DocumentationActivity(DocumentationPlace place, ClientFactory clientFactory) {
        this.clientFactory = clientFactory;
    }

    @Override
    public void start(AcceptsOneWidget containerWidget, EventBus eventBus) {
        final DocumentationView documentationView = clientFactory.getDocumentationView();
        containerWidget.setWidget(documentationView.asWidget());
    }
}
