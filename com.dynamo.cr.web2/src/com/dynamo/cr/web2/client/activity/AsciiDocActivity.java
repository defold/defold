package com.dynamo.cr.web2.client.activity;

import com.dynamo.cr.web2.client.ClientFactory;
import com.dynamo.cr.web2.client.ui.IAsciiDocView;
import com.google.gwt.activity.shared.AbstractActivity;
import com.google.gwt.http.client.Request;
import com.google.gwt.http.client.RequestBuilder;
import com.google.gwt.http.client.RequestCallback;
import com.google.gwt.http.client.RequestException;
import com.google.gwt.http.client.Response;

public abstract class AsciiDocActivity extends AbstractActivity {
    protected ClientFactory clientFactory;

    public AsciiDocActivity(ClientFactory clientFactory) {
        super();
        this.clientFactory = clientFactory;
    }

    public void loadAsciiDoc(final IAsciiDocView view, String name) {
        view.setLoading(true);
        view.clear();

        RequestBuilder builder = new RequestBuilder(RequestBuilder.GET, "/site/" + name + ".html");
        try {
            builder.sendRequest("", new RequestCallback() {
                @Override
                public void onResponseReceived(Request request, Response response) {
                    int status = response.getStatusCode();
                    if (status == 200) {
                        view.setLoading(false);
                        view.setText(response.getText());

                    } else {
                        view.setLoading(false);
                        clientFactory.getDefold().showErrorMessage("Unable to load documentation");
                    }
                }

                @Override
                public void onError(Request request, Throwable exception) {
                    view.setLoading(false);
                    clientFactory.getDefold().showErrorMessage("Unable to load documentation");
                }
            });
        } catch (RequestException e) {
            view.setLoading(false);
            clientFactory.getDefold().showErrorMessage("Unable to load documentation");
        }
    }

}