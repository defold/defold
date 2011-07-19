package com.dynamo.cr.web2.client.ui;

import java.util.List;

import com.dynamo.cr.web2.client.DocumentationElement;
import com.google.gwt.core.client.GWT;
import com.google.gwt.uibinder.client.UiBinder;
import com.google.gwt.uibinder.client.UiField;
import com.google.gwt.user.client.ui.Anchor;
import com.google.gwt.user.client.ui.Composite;
import com.google.gwt.user.client.ui.HTML;
import com.google.gwt.user.client.ui.VerticalPanel;
import com.google.gwt.user.client.ui.Widget;
import com.google.gwt.uibinder.client.UiHandler;
import com.google.gwt.event.dom.client.ClickEvent;

public class DocumentationView extends Composite {

    public interface Presenter {

        void onDocumentation(String string);
    }

    private static DashboardUiBinder uiBinder = GWT
            .create(DashboardUiBinder.class);

    interface DashboardUiBinder extends UiBinder<Widget, DocumentationView> {
    }

    @UiField VerticalPanel documentationList;
    @UiField Anchor engineLink;
    @UiField Anchor gameObjectLink;
    @UiField Anchor gameSysLink;
    @UiField Anchor guiLink;
    @UiField Anchor renderLink;
    @UiField Anchor scriptLink;

    private Presenter listener;

    public DocumentationView() {
        initWidget(uiBinder.createAndBindUi(this));
    }

    public void setPresenter(DocumentationView.Presenter listener) {
        this.listener = listener;
    }

    public void setDocumentation(List<DocumentationElement> functions,
                                 List<DocumentationElement> messages,
                                 List<DocumentationElement> constants) {
        documentationList.clear();

        if (functions.size() > 0)
            documentationList.add(new HTML("<h2>Functions</h2>"));
        for (DocumentationElement function : functions) {
            FunctionDocumentationPanel panel = new FunctionDocumentationPanel();
            panel.setDocumentationElement(function);
            documentationList.add(panel);
        }

        if (messages.size() > 0)
            documentationList.add(new HTML("<h2>Messages</h2>"));
        for (DocumentationElement message : messages) {
            MessageDocumentationPanel panel = new MessageDocumentationPanel();
            panel.setDocumentationElement(message);
            documentationList.add(panel);
        }

        if (constants.size() > 0)
            documentationList.add(new HTML("<h2>Constants</h2>"));
        for (DocumentationElement constant : constants) {
            ConstantDocumentationPanel panel = new ConstantDocumentationPanel();
            panel.setDocumentationElement(constant);
            documentationList.add(panel);
        }
    }

    @UiHandler("engineLink")
    void onEngineLinkClick(ClickEvent event) {
        listener.onDocumentation("engine");
    }

    @UiHandler("gameObjectLink")
    void onGameObjectLinkClick(ClickEvent event) {
        listener.onDocumentation("go");
    }

    @UiHandler("gameSysLink")
    void onGameSysLinkClick(ClickEvent event) {
        listener.onDocumentation("gamesys");
    }

    @UiHandler("guiLink")
    void onGuiLinkClick(ClickEvent event) {
        listener.onDocumentation("gui");
    }

    @UiHandler("renderLink")
    void onRenderLinkClick(ClickEvent event) {
        listener.onDocumentation("render");
    }

    @UiHandler("scriptLink")
    void onScriptLinkClick(ClickEvent event) {
        listener.onDocumentation("script");
    }
}
