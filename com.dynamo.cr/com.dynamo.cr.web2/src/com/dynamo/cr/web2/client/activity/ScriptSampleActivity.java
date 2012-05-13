package com.dynamo.cr.web2.client.activity;

import com.dynamo.cr.web2.client.ClientFactory;
import com.dynamo.cr.web2.client.place.ScriptSamplePlace;
import com.dynamo.cr.web2.client.ui.ScriptSampleView;
import com.google.gwt.event.shared.EventBus;
import com.google.gwt.user.client.ui.AcceptsOneWidget;

public class ScriptSampleActivity extends AsciiDocActivity implements ScriptSampleView.Presenter {

    private ScriptSamplePlace place;

    public ScriptSampleActivity(ScriptSamplePlace place, ClientFactory clientFactory) {
        super(clientFactory);
        this.place = place;
    }

    @Override
    public void start(AcceptsOneWidget containerWidget, EventBus eventBus) {
        final ScriptSampleView scriptSampleView = clientFactory.getScriptSampleView();
        containerWidget.setWidget(scriptSampleView.asWidget());
        scriptSampleView.setPresenter(this);
        String id = place.getId();
        if (id.length() > 0) {
            loadAsciiDoc(scriptSampleView, id);
        }
    }
}
