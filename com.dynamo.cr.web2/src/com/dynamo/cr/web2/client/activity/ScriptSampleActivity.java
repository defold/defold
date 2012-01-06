package com.dynamo.cr.web2.client.activity;

import com.dynamo.cr.web2.client.ClientFactory;
import com.dynamo.cr.web2.client.place.ScriptSamplePlace;
import com.dynamo.cr.web2.client.ui.ScriptSampleView;
import com.google.gwt.event.shared.EventBus;
import com.google.gwt.user.client.ui.AcceptsOneWidget;

public class ScriptSampleActivity extends AsciiDocActivity implements ScriptSampleView.Presenter {

    public ScriptSampleActivity(ScriptSamplePlace place, ClientFactory clientFactory) {
        super(clientFactory);
    }

    @Override
    public void start(AcceptsOneWidget containerWidget, EventBus eventBus) {
        final ScriptSampleView ScriptSampleView = clientFactory.getScriptSampleView();
        containerWidget.setWidget(ScriptSampleView.asWidget());
        ScriptSampleView.setPresenter(this);
    }

    @Override
    public void onScriptSample() {
        clientFactory.getPlaceController().goTo(new ScriptSamplePlace());
    }

}
