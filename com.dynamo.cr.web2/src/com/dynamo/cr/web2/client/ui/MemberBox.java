package com.dynamo.cr.web2.client.ui;

import com.dynamo.cr.web2.client.MD5;
import com.dynamo.cr.web2.client.UserInfo;
import com.dynamo.cr.web2.client.ui.ProjectView.Presenter;
import com.google.gwt.core.client.GWT;
import com.google.gwt.dom.client.SpanElement;
import com.google.gwt.event.dom.client.ClickEvent;
import com.google.gwt.uibinder.client.UiBinder;
import com.google.gwt.uibinder.client.UiField;
import com.google.gwt.uibinder.client.UiHandler;
import com.google.gwt.user.client.ui.Button;
import com.google.gwt.user.client.ui.Composite;
import com.google.gwt.user.client.ui.Image;
import com.google.gwt.user.client.ui.Widget;

public class MemberBox extends Composite {

    private static MemberBoxUiBinder uiBinder = GWT
            .create(MemberBoxUiBinder.class);

    interface MemberBoxUiBinder extends UiBinder<Widget, MemberBox> {
    }

    @UiField Image gravatar;
    @UiField SpanElement firstName;
    @UiField SpanElement lastName;
    @UiField SpanElement youOrMemberOrOwner;
    @UiField Button removeMember;
    private Presenter listener;
    private UserInfo memberInfo;

    public MemberBox(Presenter listener, UserInfo memberInfo, boolean userIsOwner, boolean isYou, boolean isOwner) {
        initWidget(uiBinder.createAndBindUi(this));
        this.listener = listener;
        this.memberInfo = memberInfo;
        this.removeMember.setVisible(userIsOwner && !isOwner);
        String email = memberInfo.getEmail();
        email = email.trim().toLowerCase();
        String md5 = MD5.md5(email);
        String url = "http://www.gravatar.com/avatar/" + md5 + "?s=80";
        this.gravatar.setUrl(url);
        this.firstName.setInnerText(memberInfo.getFirstName());
        this.lastName.setInnerText(memberInfo.getLastName());
        this.youOrMemberOrOwner.setInnerText(isOwner ? "Owner" : isYou ? "You" : "Member");
    }

    @UiHandler("removeMember")
    void onRemoveMemberClick(ClickEvent event) {
        listener.removeMember(this.memberInfo.getId());
    }

}
