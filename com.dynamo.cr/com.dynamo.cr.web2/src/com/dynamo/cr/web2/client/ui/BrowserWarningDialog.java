package com.dynamo.cr.web2.client.ui;

import com.dynamo.cr.web2.shared.ClientUtil;
import com.google.gwt.core.client.GWT;
import com.google.gwt.dom.client.SpanElement;
import com.google.gwt.event.dom.client.ClickEvent;
import com.google.gwt.uibinder.client.UiBinder;
import com.google.gwt.uibinder.client.UiField;
import com.google.gwt.uibinder.client.UiHandler;
import com.google.gwt.user.client.ui.Button;
import com.google.gwt.user.client.ui.DialogBox;
import com.google.gwt.user.client.ui.Widget;

public class BrowserWarningDialog extends DialogBox {

    private static BrowserWarningDialogUiBinder uiBinder = GWT
            .create(BrowserWarningDialogUiBinder.class);

    interface BrowserWarningDialogUiBinder extends
            UiBinder<Widget, BrowserWarningDialog> {
    }

    @UiField
    SpanElement browser;

    @UiField
    SpanElement version;

    @UiField
    Button okButton;

    public BrowserWarningDialog() {
        setWidget(uiBinder.createAndBindUi(this));
        okButton.setStyleName("btn btn-primary");
        setModal(true);
        setGlassEnabled(true);
        browser.setInnerText(ClientUtil.getBrowser().toString());
        version.setInnerText(Integer.toString((int) ClientUtil.getBrowserVersion()));
    }

    @UiHandler("okButton")
    void onLoginGoogleButtonClick(ClickEvent event) {
        hide();
    }


}
