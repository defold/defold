package com.dynamo.cr.web2.client.ui;

import com.dynamo.cr.web2.client.DocumentationElement;
import com.dynamo.cr.web2.client.DocumentationParameter;
import com.google.gwt.core.client.GWT;
import com.google.gwt.core.client.JsArray;
import com.google.gwt.uibinder.client.UiBinder;
import com.google.gwt.uibinder.client.UiField;
import com.google.gwt.user.client.ui.Composite;
import com.google.gwt.user.client.ui.HTML;
import com.google.gwt.user.client.ui.HTMLPanel;
import com.google.gwt.user.client.ui.InlineLabel;
import com.google.gwt.user.client.ui.VerticalPanel;
import com.google.gwt.user.client.ui.Widget;
import com.google.gwt.uibinder.client.UiHandler;
import com.google.gwt.event.dom.client.ClickEvent;

public class FunctionDocumentationPanel extends Composite {

    private static FunctionDocumentationPanelUiBinder uiBinder = GWT.create(FunctionDocumentationPanelUiBinder.class);

    interface FunctionDocumentationPanelUiBinder extends UiBinder<Widget, FunctionDocumentationPanel> {
    }

    @UiField InlineLabel functionName;
    @UiField HTMLPanel content;
    @UiField HTMLPanel description;
    @UiField VerticalPanel parameterList;
    @UiField InlineLabel return_;

    public FunctionDocumentationPanel() {
        initWidget(uiBinder.createAndBindUi(this));
        content.setVisible(false);
    }

    public void setDocumentationElement(DocumentationElement element) {
        this.functionName.setText(element.getName());
        this.description.add(new HTML(element.getDescription()));
        this.return_.setText(element.getReturn());

        parameterList.clear();
        JsArray<DocumentationParameter> parameters = element.getParameters();
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
            parameterList.add(new HTML(html.toString()));
        }
    }

    @UiHandler("functionName")
    void onFunctionNameClick(ClickEvent event) {
        content.setVisible(!content.isVisible());
    }
}
