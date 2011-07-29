package com.dynamo.cr.web2.client.activity;

import com.dynamo.cr.web2.client.ClientFactory;
import com.dynamo.cr.web2.client.place.TutorialsPlace;
import com.dynamo.cr.web2.client.ui.TutorialsView;
import com.google.gwt.activity.shared.AbstractActivity;
import com.google.gwt.event.shared.EventBus;
import com.google.gwt.http.client.Request;
import com.google.gwt.http.client.RequestBuilder;
import com.google.gwt.http.client.RequestCallback;
import com.google.gwt.http.client.RequestException;
import com.google.gwt.http.client.Response;
import com.google.gwt.user.client.ui.AcceptsOneWidget;

public class TutorialsActivity extends AbstractActivity implements TutorialsView.Presenter {
    private ClientFactory clientFactory;
    private TutorialsPlace place;

    public TutorialsActivity(TutorialsPlace place, ClientFactory clientFactory) {
        this.place = place;
        this.clientFactory = clientFactory;
    }

    @Override
    public void start(AcceptsOneWidget containerWidget, EventBus eventBus) {
        final TutorialsView tutorialsView = clientFactory.getTutorialsView();
        containerWidget.setWidget(tutorialsView.asWidget());
        tutorialsView.setPresenter(this);
        if (place.getId().length() > 0) {
            loadTutorial(place.getId());
        }
    }

    @Override
    public void onTutorial(String name) {
        clientFactory.getPlaceController().goTo(new TutorialsPlace(name));
    }

    public void loadTutorial(String name) {
        final TutorialsView tutorialsView = clientFactory.getTutorialsView();
        tutorialsView.setLoading(true);
        tutorialsView.clear();

        RequestBuilder builder = new RequestBuilder(RequestBuilder.GET, "/site/" + name + ".html");
        try {
            builder.sendRequest("", new RequestCallback() {
                @Override
                public void onResponseReceived(Request request, Response response) {
                    int status = response.getStatusCode();
                    if (status == 200) {
                        tutorialsView.setLoading(false);
                        tutorialsView.setTutorial(response.getText());

                    } else {
                        tutorialsView.setLoading(false);
                        clientFactory.getDefold().showErrorMessage("Unable to load documentation");
                    }
                }

                @Override
                public void onError(Request request, Throwable exception) {
                    tutorialsView.setLoading(false);
                    clientFactory.getDefold().showErrorMessage("Unable to load documentation");
                }
            });
        } catch (RequestException e) {
            tutorialsView.setLoading(false);
            clientFactory.getDefold().showErrorMessage("Unable to load documentation");
        }
    }
}
