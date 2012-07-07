package com.dynamo.cr.web2.client.ui;

import com.dynamo.cr.web2.client.InvitationAccountInfo;
import com.google.gwt.core.client.GWT;
import com.google.gwt.dom.client.DivElement;
import com.google.gwt.dom.client.Style.Visibility;
import com.google.gwt.uibinder.client.UiBinder;
import com.google.gwt.uibinder.client.UiField;
import com.google.gwt.user.client.ui.Composite;
import com.google.gwt.user.client.ui.Widget;

public class InviteView extends Composite {

    public interface Presenter {
        void invite(String recipient);
    }

    private static DashboardUiBinder uiBinder = GWT
            .create(DashboardUiBinder.class);
    @UiField SideBar sideBar;
    @UiField InvitationBox invitationBox;
    @UiField DivElement noInvitationsLeft;

    interface DashboardUiBinder extends UiBinder<Widget, InviteView> {
    }

    public InviteView() {
        initWidget(uiBinder.createAndBindUi(this));
        noInvitationsLeft.getStyle().setVisibility(Visibility.HIDDEN);
    }

    public void setInvitationAccount(InvitationAccountInfo invitationAccount) {
        int invitationCount = invitationAccount.getCurrentCount();
        if (invitationCount > 0) {
            invitationBox.setInvitationCount(invitationCount);
            invitationBox.setVisible(true);
            noInvitationsLeft.getStyle().setVisibility(Visibility.HIDDEN);
        } else {
            invitationBox.setVisible(false);
            noInvitationsLeft.getStyle().setVisibility(Visibility.VISIBLE);
        }
    }

    public void setPresenter(InviteView.Presenter listener) {
        this.invitationBox.setPresenter(listener);
        this.invitationBox.setVisible(false);
        sideBar.setActivePage("invite");
    }

    public void setUserInfo(String firstName, String lastName, String email) {
        sideBar.setUserInfo(firstName, lastName, email);
    }

}
