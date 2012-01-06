package com.dynamo.cr.web2.client.activity;

import com.dynamo.cr.web2.client.ClientFactory;
import com.dynamo.cr.web2.client.place.TutorialsPlace;
import com.dynamo.cr.web2.client.ui.TutorialsView;
import com.google.gwt.event.shared.EventBus;
import com.google.gwt.user.client.ui.AcceptsOneWidget;

public class TutorialsActivity extends AsciiDocActivity implements TutorialsView.Presenter {
    private TutorialsPlace place;

    public TutorialsActivity(TutorialsPlace place, ClientFactory clientFactory) {
        super(clientFactory);
        this.place = place;
    }

    @Override
    public void start(AcceptsOneWidget containerWidget, EventBus eventBus) {
        final TutorialsView tutorialsView = clientFactory.getTutorialsView();
        containerWidget.setWidget(tutorialsView.asWidget());
        tutorialsView.setPresenter(this);
        if (place.getId().length() > 0) {
            loadAsciiDoc(tutorialsView, place.getId());
        }
    }

    @Override
    public void onTutorial(String name) {
        clientFactory.getPlaceController().goTo(new TutorialsPlace(name));
    }
}
