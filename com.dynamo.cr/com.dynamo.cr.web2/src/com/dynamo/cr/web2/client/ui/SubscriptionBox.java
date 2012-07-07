package com.dynamo.cr.web2.client.ui;

import com.dynamo.cr.web2.client.ProductInfo;
import com.dynamo.cr.web2.client.UserSubscriptionInfo;
import com.dynamo.cr.web2.client.ui.DashboardView.Presenter;
import com.google.gwt.core.client.GWT;
import com.google.gwt.dom.client.TableCellElement;
import com.google.gwt.event.dom.client.ClickEvent;
import com.google.gwt.uibinder.client.UiBinder;
import com.google.gwt.uibinder.client.UiField;
import com.google.gwt.uibinder.client.UiHandler;
import com.google.gwt.user.client.ui.Button;
import com.google.gwt.user.client.ui.Composite;
import com.google.gwt.user.client.ui.Widget;

public class SubscriptionBox extends Composite {

    private static SubscriptionBoxUiBinder uiBinder = GWT
            .create(SubscriptionBoxUiBinder.class);

    interface SubscriptionBoxUiBinder extends UiBinder<Widget, SubscriptionBox> {
    }

    @UiField TableCellElement productName;
    @UiField TableCellElement fee;
    @UiField TableCellElement maxMemberCount;
    @UiField Button editSubscriptionButton;

    private UserSubscriptionInfo subscription;

    public SubscriptionBox() {
        initWidget(uiBinder.createAndBindUi(this));
    }

    private Presenter listener;

    public void setPresenter(Presenter listener) {
        this.listener = listener;
    }

    public void setSubscription(UserSubscriptionInfo subscription) {
        this.subscription = subscription;
        ProductInfo product = subscription.getProductInfo();
        this.productName.setInnerText(product.getName());
        this.fee.setInnerText("$" + Integer.toString(product.getFee()));
        String count = "Unlimited";
        if (product.getMaxMemberCount() > 0) {
            count = Integer.toString(product.getMaxMemberCount());
        }
        this.maxMemberCount.setInnerText(count);
    }

    @UiHandler("editSubscriptionButton") void onEditSubscriptionButtonClick(ClickEvent event) {
        this.listener.onEditSubscription();
    }

}
