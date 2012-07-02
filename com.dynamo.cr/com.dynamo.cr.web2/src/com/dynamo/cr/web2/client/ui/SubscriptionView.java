package com.dynamo.cr.web2.client.ui;

import java.util.HashMap;
import java.util.Map;

import com.dynamo.cr.web2.client.ProductInfo;
import com.dynamo.cr.web2.client.ProductInfoList;
import com.dynamo.cr.web2.client.UserSubscriptionInfo;
import com.google.gwt.core.client.GWT;
import com.google.gwt.core.client.JsArray;
import com.google.gwt.event.dom.client.ClickEvent;
import com.google.gwt.event.dom.client.ClickHandler;
import com.google.gwt.uibinder.client.UiBinder;
import com.google.gwt.uibinder.client.UiField;
import com.google.gwt.user.client.DOM;
import com.google.gwt.user.client.Element;
import com.google.gwt.user.client.ui.Button;
import com.google.gwt.user.client.ui.Composite;
import com.google.gwt.user.client.ui.Grid;
import com.google.gwt.user.client.ui.Widget;

public class SubscriptionView extends Composite implements ClickHandler {

    public interface Presenter {
        void onMigrate(UserSubscriptionInfo subscription, ProductInfo product);
    }

    private static SubscriptionUiBinder uiBinder = GWT
            .create(SubscriptionUiBinder.class);
    @UiField
    Grid productTable;

    private Presenter listener;
    private ProductInfoList products;
    private UserSubscriptionInfo subscription;
    private Map<Object, ProductInfo> buttonToProduct = new HashMap<Object, ProductInfo>();

    interface SubscriptionUiBinder extends UiBinder<Widget, SubscriptionView> {
    }

    public SubscriptionView() {
        initWidget(uiBinder.createAndBindUi(this));

        Element thead = DOM.createElement("thead");
        Element tr = DOM.createTR();

        // add columns
        DOM.appendChild(thead,tr);
        String[] headers = new String[] {"Plan", "Team Size", ""};
        for (int i = 0; i < headers.length; ++i) {
                Element th = DOM.createTH();
                DOM.appendChild(tr, th);
                // add some text to the header...
                DOM.setInnerText(th, headers[i]);
        }

        // get the table element
        Element table = this.productTable.getElement();

        // and add the thead before the tbody
        DOM.insertChild(table,thead,0);
    }

    public void setPresenter(Presenter listener) {
        this.listener = listener;
    }

    public void setProductInfoList(ProductInfoList productInfoList) {
        this.products = productInfoList;
        loadProducts();
    }

    public void setUserSubscription(UserSubscriptionInfo subscription) {
        this.subscription = subscription;
        loadProducts();
    }

    private void loadProducts() {
        if (this.products != null && this.subscription != null) {
            JsArray<ProductInfo> products = this.products.getProducts();
            this.productTable.resize(products.length(), 3);
            for (int i = 0; i < products.length(); ++i) {
                final ProductInfo productInfo = products.get(i);
                StringBuilder builder = new StringBuilder();
                String name = builder.append(productInfo.getName()).append(" ($").append(productInfo.getFee())
                        .append("/month)")
                        .toString();
                String memberCount = "Unlimited";
                if (productInfo.getMaxMemberCount() > 0) {
                    memberCount = Integer.toString(productInfo.getMaxMemberCount());
                }
                this.productTable.setText(i, 0, name);
                this.productTable.setText(i, 1, memberCount);
                if (this.subscription.getProductInfo().getId() == productInfo.getId()) {
                    this.productTable.setText(i, 2, "Your plan");
                } else {
                    Button button = new Button();
                    if (this.subscription.getProductInfo().getFee() > productInfo.getFee()) {
                        button.setText("Downgrade");
                    } else {
                        button.setText("Upgrade");
                    }
                    button.addClickHandler(this);
                    this.buttonToProduct.put(button, productInfo);
                    this.productTable.setWidget(i, 2, button);
                }
            }
        }
    }

    public void clear() {
        // Remove all rows except header
        int rows = this.productTable.getRowCount();
        for (int i = 0; i < rows; ++i) {
            this.productTable.removeRow(0);
        }
        buttonToProduct.clear();
    }

    @Override
    public void onClick(ClickEvent event) {
        ProductInfo product = this.buttonToProduct.get(event.getSource());
        if (product != null) {
            this.listener.onMigrate(this.subscription, product);
        }
    }

}
