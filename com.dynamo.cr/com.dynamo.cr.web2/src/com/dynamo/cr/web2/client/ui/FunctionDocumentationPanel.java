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

public class FunctionDocumentationPanel extends Composite {

    private static FunctionDocumentationPanelUiBinder uiBinder = GWT.create(FunctionDocumentationPanelUiBinder.class);

    interface FunctionDocumentationPanelUiBinder extends UiBinder<Widget, FunctionDocumentationPanel> {
    }

    @UiField InlineLabel functionName;
    @UiField HTMLPanel content;
    @UiField HTMLPanel description;
    @UiField VerticalPanel parameterList;
    @UiField HTMLPanel return_;
    @UiField HTMLPanel examples;

    public FunctionDocumentationPanel() {
        initWidget(uiBinder.createAndBindUi(this));
    }

    public void setDocumentationElement(DocumentationElement element) {
        this.functionName.setText(element.getName());
        this.description.add(new HTML(element.getDescription()));
        if (element.getReturn().length() > 0)
            this.return_.add(new HTML(element.getReturn()));
        else
            this.return_.add(new HTML("No return value"));

        parameterList.clear();
        JsArray<DocumentationParameter> parameters = element.getParameters();

        if (element.getExamples().length() > 0) {
            this.examples.add(new HTML(element.getExamples()));
        } else {
            StringBuffer autoExample = new StringBuffer();
            autoExample.append("<pre>");
            StringBuffer functionCall = new StringBuffer();
            if (element.getReturn().length() > 0) {
                functionCall.append("local x = ");
            }
            functionCall.append(element.getName()).append("(");
            int paramCount = parameters.length();
            for (int i = 0; i < paramCount; ++i) {
                DocumentationParameter param = parameters.get(i);
                String paramName = param.getName();
                if (paramName.charAt(0) == '[') {
                    autoExample.append(functionCall).append(")<br/>");
                    paramName = paramName.substring(1, param.getName().length() - 1);
                }
                if (0 < i) {
                    functionCall.append(", ");
                }
                functionCall.append(paramName);
            }
            autoExample.append(functionCall).append(")</pre>");
            this.examples.add(new HTML(autoExample.toString()));
        }

        if (parameters.length() == 0) {
            parameterList.add(new HTML("No parameters"));
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
                parameterList.add(new HTML(html.toString()));
            }
        }
    }

}
