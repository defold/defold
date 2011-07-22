package com.dynamo.cr.web2.client.ui;

import com.dynamo.cr.web2.client.DocumentationElement;
import com.dynamo.cr.web2.client.DocumentationParameter;
import com.google.gwt.core.client.GWT;
import com.google.gwt.core.client.JsArray;
import com.google.gwt.event.dom.client.ClickEvent;
import com.google.gwt.uibinder.client.UiBinder;
import com.google.gwt.uibinder.client.UiField;
import com.google.gwt.uibinder.client.UiHandler;
import com.google.gwt.user.client.ui.Composite;
import com.google.gwt.user.client.ui.HTML;
import com.google.gwt.user.client.ui.HTMLPanel;
import com.google.gwt.user.client.ui.InlineHTML;
import com.google.gwt.user.client.ui.InlineLabel;
import com.google.gwt.user.client.ui.VerticalPanel;
import com.google.gwt.user.client.ui.Widget;

public class MessageDocumentationPanel extends Composite {

    private static FunctionDocumentationPanelUiBinder uiBinder = GWT.create(FunctionDocumentationPanelUiBinder.class);

    interface FunctionDocumentationPanelUiBinder extends UiBinder<Widget, MessageDocumentationPanel> {
    }

    @UiField InlineLabel messageName;
    @UiField InlineHTML brief;
    @UiField HTMLPanel content;
    @UiField HTMLPanel description;
    @UiField VerticalPanel fieldList;
    @UiField HTMLPanel examples;

    public MessageDocumentationPanel() {
        initWidget(uiBinder.createAndBindUi(this));
        content.setVisible(false);
    }

    public void setDocumentationElement(DocumentationElement element) {
        this.messageName.setText(element.getName());
        this.brief.setHTML(element.getBrief());
        this.description.add(new HTML(element.getDescription()));

        if (element.getExamples().length() > 0) {
            this.examples.add(new HTML(element.getExamples()));
        } else {
            this.examples.add(new HTML("No examples"));
        }
        fieldList.clear();
        JsArray<DocumentationParameter> parameters = element.getParameters();
        if (parameters.length() == 0) {
            fieldList.add(new HTML("No fields"));
        } else {
            for (int i = 0; i < parameters.length(); ++i) {
                DocumentationParameter param = parameters.get(i);
                StringBuffer html = new StringBuffer();
                html.append("<div class=\"script_param_block\">");
                html.append("<span class=\"script_param\">");
                html.append(param.getName());
                html.append("</span>");
                html.append("<div class=\"script_param_desc\">");
                html.append(param.getDoc());
                html.append("</div>");
                html.append("</div>");
                fieldList.add(new HTML(html.toString()));
            }
        }
    }

    @UiHandler("messageName")
    void onConstantNameClick(ClickEvent event) {
        content.setVisible(!content.isVisible());
    }

}
