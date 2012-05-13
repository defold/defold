package com.dynamo.cr.web2.client.ui;

import com.dynamo.cr.web2.client.ui.DashboardView.Presenter;
import com.google.gwt.core.client.GWT;
import com.google.gwt.dom.client.SpanElement;
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
import com.google.gwt.user.client.ui.TextBox;
import com.google.gwt.user.client.ui.Widget;

public class InvitationBox extends Composite implements KeyPressHandler {

    private static final String TEXT_BOX_BLUR = "enter your friend's email";

    private static ProjectBoxUiBinder uiBinder = GWT
            .create(ProjectBoxUiBinder.class);

    interface ProjectBoxUiBinder extends UiBinder<Widget, InvitationBox> {
    }

    @UiField(provided = true)
    TextBox recipient;
    @UiField
    SpanElement invitationCount;

    private Presenter listener;

    public InvitationBox() {
        recipient = new TextBox();
        /*
         * NOTE: Workaround for the following bug:
         * http://code.google.com/p/google-web-toolkit/issues/detail?id=3533 We
         * add handler to the textbox
         */
        recipient.addKeyPressHandler(this);
        initWidget(uiBinder.createAndBindUi(this));
        recipient.setText(TEXT_BOX_BLUR);
        recipient.addBlurHandler(new BlurHandler() {
            @Override
            public void onBlur(BlurEvent event) {
                recipient.setText(TEXT_BOX_BLUR);
            }
        });

        recipient.addFocusHandler(new FocusHandler() {
            @Override
            public void onFocus(FocusEvent event) {
                recipient.setText("");
            }
        });
    }

    public void setPresenter(Presenter listener) {
        this.listener = listener;
    }

    public void setInvitationCount(int invitationCount) {
        this.invitationCount.setInnerText(new Integer(invitationCount).toString());
        this.recipient.setText(TEXT_BOX_BLUR);
    }

    @Override
    public void onKeyPress(KeyPressEvent event) {
        if (KeyCodes.KEY_ENTER == event.getNativeEvent().getKeyCode()) {
            String recipient = this.recipient.getText();
            if (!recipient.isEmpty()) {
                listener.invite(recipient);
            }
        }
    }

}
