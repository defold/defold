package com.dynamo.cr.web2.client.activity;

import com.dynamo.cr.web2.client.ClientFactory;
import com.dynamo.cr.web2.client.place.ProductInfoPlace;
import com.dynamo.cr.web2.client.ui.ProductInfoView;
import com.google.gwt.activity.shared.AbstractActivity;
import com.google.gwt.event.shared.EventBus;
import com.google.gwt.user.client.ui.AcceptsOneWidget;

public class ProductInfoActivity extends AbstractActivity {
	private ClientFactory clientFactory;

	public ProductInfoActivity(ProductInfoPlace place, ClientFactory clientFactory) {
		this.clientFactory = clientFactory;
	}

	@Override
	public void start(AcceptsOneWidget containerWidget, EventBus eventBus) {
		ProductInfoView ProductInfoView = clientFactory.getProductInfoView();
		containerWidget.setWidget(ProductInfoView.asWidget());
	}
}