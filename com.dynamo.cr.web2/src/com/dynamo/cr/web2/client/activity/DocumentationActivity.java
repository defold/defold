package com.dynamo.cr.web2.client.activity;

import java.util.ArrayList;
import java.util.List;

import com.dynamo.cr.web2.client.ClientFactory;
import com.dynamo.cr.web2.client.DocumentationDocument;
import com.dynamo.cr.web2.client.DocumentationElement;
import com.dynamo.cr.web2.client.place.DocumentationPlace;
import com.dynamo.cr.web2.client.ui.DocumentationView;
import com.google.gwt.activity.shared.AbstractActivity;
import com.google.gwt.core.client.JsArray;
import com.google.gwt.event.shared.EventBus;
import com.google.gwt.http.client.Request;
import com.google.gwt.http.client.RequestBuilder;
import com.google.gwt.http.client.RequestCallback;
import com.google.gwt.http.client.RequestException;
import com.google.gwt.http.client.Response;
import com.google.gwt.user.client.ui.AcceptsOneWidget;

public class DocumentationActivity extends AbstractActivity implements DocumentationView.Presenter {
    private ClientFactory clientFactory;

    public DocumentationActivity(DocumentationPlace place, ClientFactory clientFactory) {
        this.clientFactory = clientFactory;
    }

    private void loadDocument(String name) {
        final DocumentationView documentationView = clientFactory.getDocumentationView();

        RequestBuilder builder = new RequestBuilder(RequestBuilder.GET, "/doc/" + name
                + "_doc.json");
        try {
            builder.sendRequest("", new RequestCallback() {
                @Override
                public void onResponseReceived(Request request, Response response) {
                    int status = response.getStatusCode();
                    if (status == 200) {
                        DocumentationDocument doc = DocumentationDocument.getResponse(response
                                .getText());

                        List<DocumentationElement> functions = new ArrayList<DocumentationElement>();
                        List<DocumentationElement> constants = new ArrayList<DocumentationElement>();

                        JsArray<DocumentationElement> elements = doc.getElements();
                        for (int i = 0; i < elements.length(); ++i) {
                            DocumentationElement element = elements.get(i);
                            if (element.getType().equals("FUNCTION")) {
                                functions.add(element);
                            }
                        }

                        for (int i = 0; i < elements.length(); ++i) {
                            DocumentationElement element = elements.get(i);
                            if (element.getType().equals("VARIABLE")) {
                                constants.add(element);
                            }
                        }

                        documentationView.setDocumentation(functions, constants);

                    } else {
                        clientFactory.getDefold().showErrorMessage("Unable to load documentation");
                    }
                }

                @Override
                public void onError(Request request, Throwable exception) {
                    clientFactory.getDefold().showErrorMessage("Unable to load documentation");
                }
            });
        } catch (RequestException e) {
            clientFactory.getDefold().showErrorMessage("Unable to load documentation");
        }
    }

    @Override
    public void start(AcceptsOneWidget containerWidget, EventBus eventBus) {
        final DocumentationView documentationView = clientFactory.getDocumentationView();
        containerWidget.setWidget(documentationView.asWidget());
        documentationView.setPresenter(this);
    }

    @Override
    public void onDocumentation(String name) {
        loadDocument(name);
    }

}
