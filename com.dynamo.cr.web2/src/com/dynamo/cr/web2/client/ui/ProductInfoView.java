package com.dynamo.cr.web2.client.ui;

import com.google.gwt.core.client.GWT;
import com.google.gwt.event.dom.client.BlurEvent;
import com.google.gwt.event.dom.client.BlurHandler;
import com.google.gwt.event.dom.client.FocusEvent;
import com.google.gwt.event.dom.client.FocusHandler;
import com.google.gwt.event.dom.client.KeyCodes;
import com.google.gwt.event.dom.client.KeyPressEvent;
import com.google.gwt.event.dom.client.KeyPressHandler;
import com.google.gwt.uibinder.client.UiBinder;
import com.google.gwt.uibinder.client.UiField;
import com.google.gwt.user.client.ui.Composite;
import com.google.gwt.user.client.ui.DeckPanel;
import com.google.gwt.user.client.ui.TextBox;
import com.google.gwt.user.client.ui.Widget;

public class ProductInfoView extends Composite implements KeyPressHandler {

    private static final String REGISTER_BOX_BLUR = "enter your email for beta access";

    private static final Binder binder = GWT.create(Binder.class);

    public interface Presenter {
        void registerProspect(String email);
    }

    interface Binder extends UiBinder<Widget, ProductInfoView> {
    }

    @UiField DeckPanel registerDeck;
    @UiField(provided = true) TextBox registerEmail;

    private Presenter listener;

    public ProductInfoView() {
        registerEmail = new TextBox();
        /*
         * NOTE: Workaround for the following bug:
         * http://code.google.com/p/google-web-toolkit/issues/detail?id=3533 We
         * add handler to the textbox
         */
        registerEmail.addKeyPressHandler(this);
        initWidget(binder.createAndBindUi(this));

        registerEmail.addBlurHandler(new BlurHandler() {
            @Override
            public void onBlur(BlurEvent event) {
                registerEmail.setText(REGISTER_BOX_BLUR);
            }
        });

        registerEmail.addFocusHandler(new FocusHandler() {
            @Override
            public void onFocus(FocusEvent event) {
                registerEmail.setText("");
            }
        });
        reset();
    }

    public void reset() {
        registerDeck.showWidget(0);
        registerEmail.setText(REGISTER_BOX_BLUR);
    }

    public void setPresenter(Presenter listener) {
        this.listener = listener;
    }

    public void showRegisterConfirmation() {
        registerDeck.showWidget(1);
    }

    @Override
    public void onKeyPress(KeyPressEvent event) {
        if (KeyCodes.KEY_ENTER == event.getNativeEvent().getKeyCode()) {
            String prospect = this.registerEmail.getText();
            if (!prospect.isEmpty()) {
                listener.registerProspect(prospect);
            }
        }
    }
}
