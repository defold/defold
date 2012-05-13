package com.dynamo.cr.web2.client.activity;

import com.dynamo.cr.web2.client.ClientFactory;
import com.dynamo.cr.web2.client.place.ContentPlace;
import com.dynamo.cr.web2.client.ui.ContentView;
import com.google.gwt.event.shared.EventBus;
import com.google.gwt.user.client.ui.AcceptsOneWidget;

public class ContentActivity extends AsciiDocActivity implements ContentView.Presenter {
    private ContentPlace place;

    public ContentActivity(ContentPlace place, ClientFactory clientFactory) {
        super(clientFactory);
        this.place = place;
    }

    @Override
    public void start(AcceptsOneWidget containerWidget, EventBus eventBus) {
        final ContentView contentView = clientFactory.getContentView();
        containerWidget.setWidget(contentView.asWidget());
        contentView.setPresenter(this);
        if (place.getId().length() > 0) {
            loadAsciiDoc(contentView, place.getId());
        }
    }

    @Override
    public void onContent(String name) {
        clientFactory.getPlaceController().goTo(new ContentPlace(name));
    }
}
