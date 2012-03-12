package com.dynamo.cr.web2.client.activity;

import com.dynamo.cr.web2.client.ClientFactory;
import com.dynamo.cr.web2.client.place.GuidePlace;
import com.dynamo.cr.web2.client.ui.GuideView;
import com.google.gwt.event.shared.EventBus;
import com.google.gwt.user.client.ui.AcceptsOneWidget;

public class GuideActivity extends AsciiDocActivity implements GuideView.Presenter {
    private GuidePlace place;

    public GuideActivity(GuidePlace place, ClientFactory clientFactory) {
        super(clientFactory);
        this.place = place;
    }

    @Override
    public void start(AcceptsOneWidget containerWidget, EventBus eventBus) {
        final GuideView guideView = clientFactory.getGuideView();
        containerWidget.setWidget(guideView.asWidget());
        guideView.setPresenter(this);
        if (place.getId().length() > 0) {
            loadAsciiDoc(guideView, place.getId());
        }
    }

    @Override
    public void onGuide(String name) {
        clientFactory.getPlaceController().goTo(new GuidePlace(name));
    }
}
