package com.dynamo.cr.web2.client.ui;

import com.google.gwt.core.client.GWT;
import com.google.gwt.uibinder.client.UiBinder;
import com.google.gwt.user.client.ui.Composite;
import com.google.gwt.user.client.ui.Widget;

public class DocumentationView extends Composite {

    private static DashboardUiBinder uiBinder = GWT
            .create(DashboardUiBinder.class);

    interface DashboardUiBinder extends UiBinder<Widget, DocumentationView> {
    }

    public DocumentationView() {
        initWidget(uiBinder.createAndBindUi(this));
    }

}
