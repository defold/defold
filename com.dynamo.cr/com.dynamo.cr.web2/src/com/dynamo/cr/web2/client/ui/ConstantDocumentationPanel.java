package com.dynamo.cr.web2.client.ui;

import com.dynamo.cr.web2.client.DocumentationElement;
import com.google.gwt.core.client.GWT;
import com.google.gwt.uibinder.client.UiBinder;
import com.google.gwt.uibinder.client.UiField;
import com.google.gwt.user.client.ui.Composite;
import com.google.gwt.user.client.ui.InlineLabel;
import com.google.gwt.user.client.ui.Widget;

public class ConstantDocumentationPanel extends Composite {

    private static FunctionDocumentationPanelUiBinder uiBinder = GWT.create(FunctionDocumentationPanelUiBinder.class);

    interface FunctionDocumentationPanelUiBinder extends UiBinder<Widget, ConstantDocumentationPanel> {
    }

    @UiField InlineLabel constantName;
    @UiField InlineLabel description;

    public ConstantDocumentationPanel() {
        initWidget(uiBinder.createAndBindUi(this));
    }

    public void setDocumentationElement(DocumentationElement element) {
        this.constantName.setText(element.getName());
        this.description.setText(element.getDescription());
    }

}
