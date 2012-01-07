package com.dynamo.cr.web2.client.activity;

import com.dynamo.cr.web2.client.ClientFactory;
import com.dynamo.cr.web2.client.DocumentationDocument;
import com.dynamo.cr.web2.client.place.ReferencePlace;
import com.dynamo.cr.web2.client.ui.ReferenceView;
import com.google.gwt.activity.shared.AbstractActivity;
import com.google.gwt.event.shared.EventBus;
import com.google.gwt.http.client.Request;
import com.google.gwt.http.client.RequestBuilder;
import com.google.gwt.http.client.RequestCallback;
import com.google.gwt.http.client.RequestException;
import com.google.gwt.http.client.Response;
import com.google.gwt.user.client.History;
import com.google.gwt.user.client.ui.AcceptsOneWidget;

public class ReferenceActivity extends AbstractActivity implements ReferenceView.Presenter {
    private ReferencePlace place;
    private String documentName;
    private String elementName;
    private ClientFactory clientFactory;

    public ReferenceActivity(ReferencePlace place, ClientFactory clientFactory) {
        this.clientFactory = clientFactory;
        this.place = place;

        String[] lst = this.place.getId().split("/");
        if (lst.length > 0)
            this.documentName = lst[0];
        if (lst.length > 1)
            this.elementName = lst[1];
    }

    @Override
    public void start(AcceptsOneWidget containerWidget, EventBus eventBus) {
        final ReferenceView referenceView = clientFactory.getReferenceView();
        containerWidget.setWidget(referenceView.asWidget());
        referenceView.setPresenter(this);
        referenceView.clear();

        if (this.documentName != null) {
            loadDocument(documentName);
        }
    }

    @Override
    public void onDocumentation(String id, String name) {
        clientFactory.getPlaceController().goTo(new ReferencePlace(id));
    }

    @Override
    public void onDocumentationElement(String documentName, String elementName) {
        final ReferenceView referenceView = clientFactory.getReferenceView();
        referenceView.scrollTo(elementName);
        History.newItem("reference:" + documentName + "/" + elementName, false);
    }

    private void loadDocument(final String name) {
        final ReferenceView referenceView = clientFactory.getReferenceView();
        referenceView.setLoading(true);
        RequestBuilder builder = new RequestBuilder(RequestBuilder.GET, "/doc/" + name
                + "_doc.json");
        try {
            builder.sendRequest("", new RequestCallback() {
                @Override
                public void onResponseReceived(Request request, Response response) {
                    referenceView.setLoading(false);
                    int status = response.getStatusCode();
                    if (status == 200) {
                        DocumentationDocument doc = DocumentationDocument.getResponse(response
                                .getText());

                        referenceView.setDocument(name, doc);
                        if (elementName != null) {
                            referenceView.scrollTo(elementName);
                        }

                    } else {
                        clientFactory.getDefold().showErrorMessage("Unable to load documentation");
                        referenceView.setLoading(false);
                    }
                }

                @Override
                public void onError(Request request, Throwable exception) {
                    clientFactory.getDefold().showErrorMessage("Unable to load documentation");
                    referenceView.setLoading(false);
                }
            });
        } catch (RequestException e) {
            clientFactory.getDefold().showErrorMessage("Unable to load documentation");
            referenceView.setLoading(false);
        }
    }
}
