package com.dynamo.cr.web2.client.ui;

import java.util.HashMap;
import java.util.Map;

import com.dynamo.cr.web2.client.CreditCardInfo;
import com.dynamo.cr.web2.client.ProductInfo;
import com.dynamo.cr.web2.client.ProductInfoList;
import com.dynamo.cr.web2.client.UserSubscriptionInfo;
import com.google.gwt.core.client.GWT;
import com.google.gwt.core.client.JsArray;
import com.google.gwt.dom.client.TableCellElement;
import com.google.gwt.event.dom.client.ClickEvent;
import com.google.gwt.event.dom.client.ClickHandler;
import com.google.gwt.i18n.client.NumberFormat;
import com.google.gwt.uibinder.client.UiBinder;
import com.google.gwt.uibinder.client.UiField;
import com.google.gwt.uibinder.client.UiHandler;
import com.google.gwt.user.client.DOM;
import com.google.gwt.user.client.Element;
import com.google.gwt.user.client.ui.Button;
import com.google.gwt.user.client.ui.Composite;
import com.google.gwt.user.client.ui.Grid;
import com.google.gwt.user.client.ui.HTMLPanel;
import com.google.gwt.user.client.ui.Image;
import com.google.gwt.user.client.ui.Label;
import com.google.gwt.user.client.ui.Widget;

public class SubscriptionView extends Composite implements ClickHandler {

    public interface Presenter {
        void onMigrate(UserSubscriptionInfo subscription, ProductInfo product);

        void onReactivate(UserSubscriptionInfo subscription);

        void onTerminate(UserSubscriptionInfo subscription);

        void onEditCreditCard(UserSubscriptionInfo subscription);
    }

    private static SubscriptionUiBinder uiBinder = GWT
            .create(SubscriptionUiBinder.class);
    @UiField
    HTMLPanel cancellationMessage;
    @UiField
    Label providerMessage;
    @UiField
    Grid productTable;

    @UiField
    HTMLPanel creditCard;
    @UiField
    TableCellElement maskedNumber;
    @UiField
    TableCellElement expiration;
    @UiField
    Button editCCButton;

    @UiField
    Image loader;
    @UiField
    HTMLPanel content;

    private Presenter listener;
    private ProductInfoList products;
    private UserSubscriptionInfo subscription;
    private Map<Object, ProductInfo> buttonToProduct = new HashMap<Object, ProductInfo>();
    private Button reactivateButton;
    private Button terminateButton;

    interface SubscriptionUiBinder extends UiBinder<Widget, SubscriptionView> {
    }

    public SubscriptionView() {
        initWidget(uiBinder.createAndBindUi(this));
        productTable.setStyleName("table table-striped table-pricing");
        editCCButton.addStyleName("btn btn-primary");

        setLoading(false);
        this.cancellationMessage.setVisible(false);
        Element thead = DOM.createElement("thead");
        Element tr = DOM.createTR();

        // add columns
        DOM.appendChild(thead,tr);
        String[] headers = new String[] { "Plan", "Team Size", "Status", "" };
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
        if (subscription.getState().equals("CANCELED") && subscription.getCancellationMessage() != null
                && !subscription.getCancellationMessage().isEmpty()) {
            this.cancellationMessage.setVisible(true);
            this.providerMessage.setText(subscription.getCancellationMessage());
        } else {
            this.cancellationMessage.setVisible(false);
        }
        loadProducts();
    }

    private Button createButton(String title) {
        Button b = new Button(title, this);
        b.setStyleName("btn btn-primary");
        return b;
    }

    private void loadProducts() {
        if (this.products != null && this.subscription != null) {
            setLoading(false);
            JsArray<ProductInfo> products = this.products.getProducts();
            this.productTable.resize(products.length(), 5);
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
                this.productTable.getCellFormatter().setStyleName(i, 3, "button-col");
                String state = this.subscription.getState();
                boolean canceled = state.equals("CANCELED");
                boolean active = state.equals("ACTIVE");
                if (this.subscription.getProductInfo().getId() == productInfo.getId()) {
                    this.productTable.setText(i, 2, state);
                    if (canceled) {
                        this.reactivateButton = createButton("Reactivate");
                        this.productTable.setWidget(i, 3, this.reactivateButton);
                        this.terminateButton = createButton("Terminate");
                        this.productTable.setWidget(i, 4, this.terminateButton);
                    } else {
                        this.productTable.setText(i, 3, "Your plan");
                        this.productTable.clearCell(i, 4);
                    }
                } else {
                    this.productTable.setText(i, 2, "-");
                    this.productTable.clearCell(i, 4);
                    if (active) {
                        Button button = createButton("");
                        if (this.subscription.getProductInfo().getFee() > productInfo.getFee()) {
                            button.setText("Downgrade");
                        } else {
                            button.setText("Upgrade");
                        }
                        button.addClickHandler(this);
                        this.buttonToProduct.put(button, productInfo);
                        this.productTable.setWidget(i, 3, button);
                    }
                }
            }
        }
    }

    public void clear() {
        // Remove all rows except header
        this.productTable.resizeRows(1);
        this.buttonToProduct.clear();
    }

    public void setLoading(boolean loading) {
        loader.setVisible(loading);
        content.setVisible(!loading);
    }

    @Override
    public void onClick(ClickEvent event) {
        ProductInfo product = this.buttonToProduct.get(event.getSource());
        if (product != null) {
            this.listener.onMigrate(this.subscription, product);
        } else if (event.getSource() == this.reactivateButton) {
            this.listener.onReactivate(this.subscription);
        } else if (event.getSource() == this.terminateButton) {
            this.listener.onTerminate(this.subscription);
        }
    }

    @UiHandler("editCCButton")
    void onEditCreditCardButtonClick(ClickEvent event) {
        this.listener.onEditCreditCard(subscription);
    }
}
