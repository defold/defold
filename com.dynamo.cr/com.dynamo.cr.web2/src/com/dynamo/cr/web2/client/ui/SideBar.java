package com.dynamo.cr.web2.client.ui;

import com.dynamo.cr.web2.shared.ClientUtil;
import com.google.gwt.core.client.GWT;
import com.google.gwt.dom.client.ImageElement;
import com.google.gwt.uibinder.client.UiBinder;
import com.google.gwt.uibinder.client.UiField;
import com.google.gwt.user.client.ui.Composite;
import com.google.gwt.user.client.ui.Label;
import com.google.gwt.user.client.ui.Widget;

public class SideBar extends Composite {

    private static CommitBoxUiBinder uiBinder = GWT
            .create(CommitBoxUiBinder.class);

    interface CommitBoxUiBinder extends UiBinder<Widget, SideBar> {
    }

    @UiField ImageElement gravatarImage;
    @UiField Label name;

    public SideBar() {
        initWidget(uiBinder.createAndBindUi(this));
        gravatarImage.setId("gravatar");
    }

    private native void setToolTip() /*-{
        $wnd.$('#gravatar').tooltip({placement : 'bottom'});
    }-*/;

    public native void setActivePage(String page) /*-{
        $wnd.$("#sideBarList").children().removeClass("active");
        $wnd.$("#" + page).addClass("active");
    }-*/;

    public void setUserInfo(String firstName, String lastName, String email) {
        String url = ClientUtil.createGravatarUrl(email, 32);
        this.gravatarImage.setSrc(url);
        this.name.setText(firstName + " " + lastName);
        setToolTip();
    }
}

