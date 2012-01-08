package com.dynamo.cr.web2.client.ui;

import com.google.gwt.core.client.GWT;
import com.google.gwt.uibinder.client.UiBinder;
import com.google.gwt.uibinder.client.UiField;
import com.google.gwt.user.client.ui.Anchor;
import com.google.gwt.user.client.ui.Composite;
import com.google.gwt.user.client.ui.Label;
import com.google.gwt.user.client.ui.Widget;

public class DocSummary extends Composite {

    private static CommitBoxUiBinder uiBinder = GWT
            .create(CommitBoxUiBinder.class);

    interface CommitBoxUiBinder extends UiBinder<Widget, DocSummary> {
    }

    @UiField Anchor anchor;
    @UiField Label summary;

    public DocSummary(String name, String summary) {
        initWidget(uiBinder.createAndBindUi(this));
        this.anchor.setText(name);
        this.summary.setText(summary);
    }

}
