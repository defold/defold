package com.dynamo.cr.web2.client;

import com.google.gwt.user.client.ui.PopupPanel;
import com.google.gwt.user.client.ui.Label;
import com.google.gwt.user.client.ui.HasHorizontalAlignment;

public class Notification extends PopupPanel {

    public Notification() {
        super(true);
        setSize("295px", "30px");

        Label messageLabel = new Label("MESSAGE PLACEHOLDER");
        messageLabel.setHorizontalAlignment(HasHorizontalAlignment.ALIGN_CENTER);
        setWidget(messageLabel);
        messageLabel.setSize("283px", "18px");
    }

}
