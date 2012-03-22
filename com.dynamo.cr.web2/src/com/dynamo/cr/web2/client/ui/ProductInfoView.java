package com.dynamo.cr.web2.client.ui;

import com.google.gwt.core.client.GWT;
import com.google.gwt.dom.client.Style.Unit;
import com.google.gwt.event.dom.client.BlurEvent;
import com.google.gwt.event.dom.client.BlurHandler;
import com.google.gwt.event.dom.client.ClickEvent;
import com.google.gwt.event.dom.client.ClickHandler;
import com.google.gwt.event.dom.client.FocusEvent;
import com.google.gwt.event.dom.client.FocusHandler;
import com.google.gwt.event.dom.client.KeyCodes;
import com.google.gwt.event.dom.client.KeyPressEvent;
import com.google.gwt.event.dom.client.KeyPressHandler;
import com.google.gwt.resources.client.ClientBundle;
import com.google.gwt.resources.client.ImageResource;
import com.google.gwt.uibinder.client.UiBinder;
import com.google.gwt.uibinder.client.UiField;
import com.google.gwt.user.client.Timer;
import com.google.gwt.user.client.ui.Composite;
import com.google.gwt.user.client.ui.DeckPanel;
import com.google.gwt.user.client.ui.FlowPanel;
import com.google.gwt.user.client.ui.Image;
import com.google.gwt.user.client.ui.TextBox;
import com.google.gwt.user.client.ui.Widget;

public class ProductInfoView extends Composite implements ClickHandler, KeyPressHandler {

    private static final String REGISTER_BOX_BLUR = "enter your email";

    private static final Binder binder = GWT.create(Binder.class);
    private static final Resources resources = GWT.create(Resources.class);

    public interface Presenter {
        void registerProspect(String email);
    }

    interface Binder extends UiBinder<Widget, ProductInfoView> {
    }

    interface Resources extends ClientBundle {
        @Source("spot.png")
        ImageResource spot();

        @Source("spot_hover.png")
        ImageResource spotHover();

        @Source("spot_active.png")
        ImageResource spotActive();
    }

    @UiField DeckPanel deck;
    @UiField FlowPanel buttonPanel;
    @UiField DeckPanel registerDeck;
    @UiField(provided = true) TextBox registerEmail;

    private Presenter listener;
    private Image[] images;
    private Timer timer;
    private int activeDeck;

    public ProductInfoView() {
        registerEmail = new TextBox();
        /*
         * NOTE: Workaround for the following bug:
         * http://code.google.com/p/google-web-toolkit/issues/detail?id=3533 We
         * add handler to the textbox
         */
        registerEmail.addKeyPressHandler(this);
        initWidget(binder.createAndBindUi(this));

        int n = this.deck.getWidgetCount();
        this.images = new Image[n];
        for (int i = 0; i < n; ++i) {
            this.images[i] = new Image(resources.spot());
            Image image = this.images[i];
            image.getElement().getStyle().setMargin(6, Unit.PX);
            image.addClickHandler(this);
            this.buttonPanel.add(image);
        }
        timer = new Timer() {
            @Override
            public void run() {
                // Timer is actually in the same single-threaded environment
                setActiveDeck(activeDeck + 1);
            }
        };
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
        setActiveDeck(0);
        timer.cancel();
        // Schedule the timer to run once every 5 seconds.
        timer.scheduleRepeating(5000);
        registerDeck.showWidget(0);
        registerEmail.setText(REGISTER_BOX_BLUR);
    }

    public void setPresenter(Presenter listener) {
        this.listener = listener;
    }

    public void setActiveDeck(int activeDeck) {
        images[this.activeDeck].setResource(resources.spot());
        this.activeDeck = activeDeck % deck.getWidgetCount();
        images[this.activeDeck].setResource(resources.spotActive());
        deck.showWidget(this.activeDeck);
    }

    public void showRegisterConfirmation() {
        registerDeck.showWidget(1);
    }

    @Override
    public void onClick(ClickEvent event) {
        for (int i = 0; i < this.images.length; ++i) {
            if (images[i] == event.getSource()) {
                timer.cancel();
                setActiveDeck(i);
            }
        }
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
