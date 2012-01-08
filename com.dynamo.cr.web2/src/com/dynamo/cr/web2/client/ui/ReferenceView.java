package com.dynamo.cr.web2.client.ui;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

import com.dynamo.cr.web2.client.DocumentationDocument;
import com.dynamo.cr.web2.client.DocumentationElement;
import com.google.gwt.core.client.GWT;
import com.google.gwt.core.client.JsArray;
import com.google.gwt.dom.client.Element;
import com.google.gwt.event.dom.client.ClickEvent;
import com.google.gwt.event.dom.client.ClickHandler;
import com.google.gwt.uibinder.client.UiBinder;
import com.google.gwt.uibinder.client.UiField;
import com.google.gwt.uibinder.client.UiHandler;
import com.google.gwt.user.client.ui.Anchor;
import com.google.gwt.user.client.ui.Composite;
import com.google.gwt.user.client.ui.HTML;
import com.google.gwt.user.client.ui.Image;
import com.google.gwt.user.client.ui.VerticalPanel;
import com.google.gwt.user.client.ui.Widget;

public class ReferenceView extends Composite {

    public interface Presenter {

        void onDocumentation(String id, String name);

        void onDocumentationElement(String documentName, String name);
    }

    private static DashboardUiBinder uiBinder = GWT
            .create(DashboardUiBinder.class);

    interface DashboardUiBinder extends UiBinder<Widget, ReferenceView> {
    }

    @UiField Anchor engine;
    @UiField Anchor gameObject;
    @UiField Anchor gameSys;
    @UiField Anchor gui;
    @UiField Anchor render;
    @UiField Anchor script;

    @UiField VerticalPanel documentationList;

    @UiField Image loader;
    private Presenter listener;

    public ReferenceView() {
        initWidget(uiBinder.createAndBindUi(this));
        loader.setVisible(false);
    }

    @UiHandler({"engine", "gameObject", "gameSys", "gui", "render", "script"})
    void onDocumentation(ClickEvent event) {
        Element element = event.getRelativeElement();
        String id = element.getParentElement().getId();
        String name = element.getInnerText();
        listener.onDocumentation(id, name);
    }

    public void setPresenter(ReferenceView.Presenter listener) {
        this.listener = listener;
    }

    public void clear() {
        this.documentationList.clear();
    }

    private Map<String, Element> nameToDocElement = new HashMap<String, Element>();
    private String documentName;

    public void setDocument(final String name, DocumentationDocument doc) {
        this.documentName = name;
        documentationList.clear();
        nameToDocElement.clear();

        List<DocumentationElement> functions = new ArrayList<DocumentationElement>();
        List<DocumentationElement> messages = new ArrayList<DocumentationElement>();
        List<DocumentationElement> constants = new ArrayList<DocumentationElement>();

        JsArray<DocumentationElement> elements = doc.getElements();
        for (int i = 0; i < elements.length(); ++i) {
            DocumentationElement element = elements.get(i);
            if (element.getType().equals("FUNCTION")) {
                functions.add(element);
            }
            else if (element.getType().equals("MESSAGE")) {
                messages.add(element);
            }
            else if (element.getType().equals("VARIABLE")) {
                constants.add(element);
            }
        }

        if (functions.size() > 0) {
            documentationList.add(new HTML("<h2>Functions</h2>"));
            for (final DocumentationElement e : functions) {
                addElementLink(name, e);
            }
        }

        if (messages.size() > 0) {
            documentationList.add(new HTML("<p><h2>Messages</h2>"));
            for (final DocumentationElement e : messages) {
                addElementLink(name, e);
            }
        }

        if (constants.size() > 0) {
            documentationList.add(new HTML("<p><h2>Constants</h2>"));
            for (final DocumentationElement e : constants) {
                addElementLink(name, e);
            }
        }

        if (functions.size() > 0) {
            documentationList.add(new HTML("<p><h2>Functions</h2>"));
            for (final DocumentationElement e : functions) {
                FunctionDocumentationPanel panel = new FunctionDocumentationPanel();
                panel.setDocumentationElement(e);
                documentationList.add(panel);
                nameToDocElement.put(e.getName(), panel.getElement());
            }
        }

        if (messages.size() > 0) {
            documentationList.add(new HTML("<p><h2>Messages</h2>"));
            for (final DocumentationElement e : messages) {
                MessageDocumentationPanel panel = new MessageDocumentationPanel();
                panel.setDocumentationElement(e);
                documentationList.add(panel);
                nameToDocElement.put(e.getName(), panel.getElement());
            }
        }

        if (constants.size() > 0) {
            documentationList.add(new HTML("<p><h2>Constants</h2>"));
            for (final DocumentationElement e : constants) {
                ConstantDocumentationPanel panel = new ConstantDocumentationPanel();
                panel.setDocumentationElement(e);
                documentationList.add(panel);
                nameToDocElement.put(e.getName(), panel.getElement());
            }
        }
    }

    private void addElementLink(final String name, final DocumentationElement e) {
        final Anchor a = new Anchor(e.getName());
        a.addClickHandler(new ClickHandler() {
            @Override
            public void onClick(ClickEvent event) {
                nameToDocElement.get(e.getName()).scrollIntoView();
                listener.onDocumentationElement(documentName, e.getName());
            }
        });
        documentationList.add(a);
    }

    public void scrollTo(String elementName) {
        Element e = nameToDocElement.get(elementName);
        if (e != null) {
            e.scrollIntoView();
        } else {
            System.err.println("Unable to find: " + elementName);
        }
    }

    public void setLoading(boolean loading) {
        loader.setVisible(loading);
    }

}
