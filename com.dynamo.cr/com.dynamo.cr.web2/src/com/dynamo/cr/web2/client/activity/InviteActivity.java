package com.dynamo.cr.web2.client.activity;

import com.dynamo.cr.web2.client.ClientFactory;
import com.dynamo.cr.web2.client.Defold;
import com.dynamo.cr.web2.client.InvitationAccountInfo;
import com.dynamo.cr.web2.client.ResourceCallback;
import com.dynamo.cr.web2.client.place.InvitePlace;
import com.dynamo.cr.web2.client.ui.InviteView;
import com.google.gwt.activity.shared.AbstractActivity;
import com.google.gwt.event.shared.EventBus;
import com.google.gwt.http.client.Request;
import com.google.gwt.http.client.Response;
import com.google.gwt.user.client.ui.AcceptsOneWidget;

public class InviteActivity extends AbstractActivity implements InviteView.Presenter {
    private ClientFactory clientFactory;

    public InviteActivity(InvitePlace place, ClientFactory clientFactory) {
        this.clientFactory = clientFactory;
    }

    private void loadInvitationCount() {
        final InviteView inviteView = clientFactory.getInviteView();
        final Defold defold = clientFactory.getDefold();
        defold.getResource("/users/" + defold.getUserId() + "/invitation_account",
                new ResourceCallback<InvitationAccountInfo>() {

                    @Override
                    public void onSuccess(InvitationAccountInfo invitationAccount,
                            Request request, Response response) {
                        inviteView.setInvitationAccount(invitationAccount);
                    }

                    @Override
                    public void onFailure(Request request, Response response) {
                        defold.showErrorMessage("Invitation data could not be loaded.");
                    }
                });
    }

    @Override
    public void start(AcceptsOneWidget containerWidget, EventBus eventBus) {
        final InviteView inviteView = clientFactory.getInviteView();
        containerWidget.setWidget(inviteView.asWidget());
        inviteView.setPresenter(this);
        loadGravatar();
        loadInvitationCount();
    }

    private void loadGravatar() {
        final InviteView inviteView = clientFactory.getInviteView();
        Defold defold = clientFactory.getDefold();
        String email = defold.getEmail();

        String firstName = defold.getFirstName();
        String lastName = defold.getLastName();
        inviteView.setUserInfo(firstName, lastName, email);
    }

    @Override
    public void invite(String recipient) {
        final Defold defold = clientFactory.getDefold();
        // TODO validate email
        defold.putResource("/users/" + defold.getUserId() + "/invite/" + recipient, "",
            new ResourceCallback<String>() {

                @Override
                public void onSuccess(String result,
                        Request request, Response response) {
                    loadInvitationCount();
                }

                @Override
                public void onFailure(Request request, Response response) {
                    defold.showErrorMessage("Invitation failed: " + response.getText());
                }
            });
    }
}
