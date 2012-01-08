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
import com.google.gwt.dom.client.HeadingElement;
import com.google.gwt.dom.client.Style.Display;
import com.google.gwt.event.dom.client.ClickEvent;
import com.google.gwt.event.dom.client.ClickHandler;
import com.google.gwt.uibinder.client.UiBinder;
import com.google.gwt.uibinder.client.UiField;
import com.google.gwt.uibinder.client.UiHandler;
import com.google.gwt.user.client.ui.Anchor;
import com.google.gwt.user.client.ui.Composite;
import com.google.gwt.user.client.ui.HTML;
import com.google.gwt.user.client.ui.HTMLPanel;
import com.google.gwt.user.client.ui.Image;
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

    @UiField HTMLPanel rootDocPanel;
    @UiField HeadingElement functionSummaryHeading;
    @UiField HeadingElement messageSummaryHeading;
    @UiField HeadingElement constantSummaryHeading;
    @UiField HTMLPanel functionSummaryPanel;
    @UiField HTMLPanel messageSummaryPanel;
    @UiField HTMLPanel constantSummaryPanel;

    @UiField HTMLPanel documentationPanel;

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
        rootDocPanel.setVisible(false);
    }

    private Map<String, Element> nameToDocElement = new HashMap<String, Element>();
    private String documentName;

    public void setDocument(final String name, DocumentationDocument doc) {
        rootDocPanel.setVisible(false);
        this.documentName = name;
        this.functionSummaryPanel.clear();
        this.messageSummaryPanel.clear();
        this.functionSummaryPanel.clear();
        documentationPanel.clear();
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

        functionSummaryHeading.getStyle().setDisplay(functions.size() > 0 ? Display.BLOCK : Display.NONE);
        messageSummaryHeading.getStyle().setDisplay(messages.size() > 0 ? Display.BLOCK : Display.NONE);
        constantSummaryHeading.getStyle().setDisplay(constants.size() > 0 ? Display.BLOCK : Display.NONE);

        createElementLinks(functions, functionSummaryPanel);
        createElementLinks(messages, messageSummaryPanel);
        createElementLinks(constants, constantSummaryPanel);

        if (functions.size() > 0) {
            documentationPanel.add(new HTML("<h2>Functions</h2>"));
            for (final DocumentationElement e : functions) {
                FunctionDocumentationPanel panel = new FunctionDocumentationPanel();
                panel.setDocumentationElement(e);
                documentationPanel.add(panel);
                nameToDocElement.put(e.getName(), panel.getElement());
            }
        }

        if (messages.size() > 0) {
            documentationPanel.add(new HTML("<h2>Messages</h2>"));
            for (final DocumentationElement e : messages) {
                MessageDocumentationPanel panel = new MessageDocumentationPanel();
                panel.setDocumentationElement(e);
                documentationPanel.add(panel);
                nameToDocElement.put(e.getName(), panel.getElement());
            }
        }

        if (constants.size() > 0) {
            documentationPanel.add(new HTML("<h2>Constants</h2>"));
            for (final DocumentationElement e : constants) {
                ConstantDocumentationPanel panel = new ConstantDocumentationPanel();
                panel.setDocumentationElement(e);
                documentationPanel.add(panel);
                nameToDocElement.put(e.getName(), panel.getElement());
            }
        }

        rootDocPanel.setVisible(true);
    }

    private void createElementLinks(List<DocumentationElement> elements, HTMLPanel panel) {
        for (final DocumentationElement e : elements) {
            addElementLink(e, panel);
        }
    }

    private void addElementLink(final DocumentationElement e, HTMLPanel panel) {
        DocSummary link = new DocSummary(e.getName(), e.getBrief());

        link.anchor.addClickHandler(new ClickHandler() {
            @Override
            public void onClick(ClickEvent event) {
                nameToDocElement.get(e.getName()).scrollIntoView();
                listener.onDocumentationElement(documentName, e.getName());
            }
        });

        panel.add(link);
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
