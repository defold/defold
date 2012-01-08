package com.dynamo.cr.web2.client.ui;

import com.google.gwt.core.client.GWT;
import com.google.gwt.dom.client.Style.Unit;
import com.google.gwt.event.dom.client.ClickEvent;
import com.google.gwt.event.dom.client.ClickHandler;
import com.google.gwt.resources.client.ClientBundle;
import com.google.gwt.resources.client.ImageResource;
import com.google.gwt.uibinder.client.UiBinder;
import com.google.gwt.uibinder.client.UiField;
import com.google.gwt.user.client.Timer;
import com.google.gwt.user.client.ui.Composite;
import com.google.gwt.user.client.ui.DeckPanel;
import com.google.gwt.user.client.ui.FlowPanel;
import com.google.gwt.user.client.ui.Image;
import com.google.gwt.user.client.ui.Widget;

public class ProductInfoView extends Composite implements ClickHandler {

    private static final Binder binder = GWT.create(Binder.class);
    private static final Resources resources = GWT.create(Resources.class);

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

    private Image[] images;
    private Timer timer;
    private int activeDeck;

    public ProductInfoView() {
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
        reset();
    }

    public void reset() {
        setActiveDeck(0);
        timer.cancel();
        // Schedule the timer to run once every 5 seconds.
        timer.scheduleRepeating(5000);
    }

    public void setActiveDeck(int activeDeck) {
        images[this.activeDeck].setResource(resources.spot());
        this.activeDeck = activeDeck % deck.getWidgetCount();
        images[this.activeDeck].setResource(resources.spotActive());
        deck.showWidget(this.activeDeck);
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
}
