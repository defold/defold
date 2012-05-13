package com.dynamo.cr.web2.client.ui;

import com.dynamo.cr.web2.client.CommitDesc;
import com.dynamo.cr.web2.client.MD5;
import com.google.gwt.core.client.GWT;
import com.google.gwt.dom.client.SpanElement;
import com.google.gwt.uibinder.client.UiBinder;
import com.google.gwt.uibinder.client.UiField;
import com.google.gwt.user.client.ui.Composite;
import com.google.gwt.user.client.ui.Image;
import com.google.gwt.user.client.ui.Widget;

public class CommitBox extends Composite {

    private static CommitBoxUiBinder uiBinder = GWT
            .create(CommitBoxUiBinder.class);

    interface CommitBoxUiBinder extends UiBinder<Widget, CommitBox> {
    }

    @UiField Image gravatar;
    @UiField SpanElement name;
    @UiField SpanElement date;
    @UiField SpanElement message;

    public CommitBox(CommitDesc commit, String date) {
        initWidget(uiBinder.createAndBindUi(this));
        String email = commit.getEmail();
        email = email.trim().toLowerCase();
        String md5 = MD5.md5(email);
        String url = "http://www.gravatar.com/avatar/" + md5 + "?s=32";
        this.gravatar.setUrl(url);
        this.name.setInnerText(commit.getName());
        this.date.setInnerText(date);
        this.message.setInnerText(commit.getMessage());
    }

}
