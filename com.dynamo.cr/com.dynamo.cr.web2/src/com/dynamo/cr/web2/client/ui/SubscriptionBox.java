package com.dynamo.cr.web2.client.ui;

import com.dynamo.cr.web2.client.CreditCardInfo;
import com.dynamo.cr.web2.client.ProductInfo;
import com.dynamo.cr.web2.client.UserSubscriptionInfo;
import com.dynamo.cr.web2.client.ui.DashboardView.Presenter;
import com.google.gwt.core.client.GWT;
import com.google.gwt.dom.client.TableCellElement;
import com.google.gwt.event.dom.client.ClickEvent;
import com.google.gwt.i18n.client.NumberFormat;
import com.google.gwt.uibinder.client.UiBinder;
import com.google.gwt.uibinder.client.UiField;
import com.google.gwt.uibinder.client.UiHandler;
import com.google.gwt.user.client.ui.Button;
import com.google.gwt.user.client.ui.Composite;
import com.google.gwt.user.client.ui.HTMLPanel;
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
    @UiField
    HTMLPanel creditCard;
    @UiField TableCellElement maskedNumber;
    @UiField TableCellElement expiration;
    @UiField Button editCCButton;

    public SubscriptionBox() {
        initWidget(uiBinder.createAndBindUi(this));
    }

    private Presenter listener;

    public void setPresenter(Presenter listener) {
        this.listener = listener;
    }

    public void setSubscription(UserSubscriptionInfo subscription) {
        ProductInfo product = subscription.getProductInfo();
        this.productName.setInnerText(product.getName());
        this.fee.setInnerText("$" + Integer.toString(product.getFee()));
        String count = "Unlimited";
        if (product.getMaxMemberCount() > 0) {
            count = Integer.toString(product.getMaxMemberCount());
        }
        this.maxMemberCount.setInnerText(count);
        CreditCardInfo cc = subscription.getCreditCardInfo();
        if (cc != null && cc.getMaskedNumber() != null &&
                !cc.getMaskedNumber().isEmpty()) {
            this.creditCard.setVisible(true);
            NumberFormat format = NumberFormat.getFormat("00");
            this.maskedNumber.setInnerText(cc.getMaskedNumber());
            String expiration = format.format(cc.getExpirationMonth()) + "/" +
                    format.format(cc.getExpirationYear());
            this.expiration.setInnerText(expiration);
        } else {
            this.creditCard.setVisible(false);
        }
    }

    @UiHandler("editSubscriptionButton") void onEditSubscriptionButtonClick(ClickEvent event) {
        this.listener.onEditSubscription();
    }

    @UiHandler("editCCButton") void onEditCreditCardButtonClick(ClickEvent event) {
        this.listener.onEditCreditCard();
    }
}
