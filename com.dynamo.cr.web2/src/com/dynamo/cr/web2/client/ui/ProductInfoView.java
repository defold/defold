package com.dynamo.cr.web2.client.ui;

import com.google.gwt.core.client.GWT;
import com.google.gwt.uibinder.client.UiBinder;
import com.google.gwt.user.client.ui.Composite;
import com.google.gwt.user.client.ui.Widget;

public class ProductInfoView extends Composite {

    private static final Binder binder = GWT.create(Binder.class);

    interface Binder extends UiBinder<Widget, ProductInfoView> {
    }

    public ProductInfoView() {
        initWidget(binder.createAndBindUi(this));
    }

}
