package com.dynamo.cr.web2.client.ui;

import com.google.gwt.core.client.GWT;
import com.google.gwt.uibinder.client.UiBinder;
import com.google.gwt.uibinder.client.UiField;
import com.google.gwt.user.client.ui.Composite;
import com.google.gwt.user.client.ui.Widget;

public class DownloadsView extends Composite {

    public interface Presenter {
    }

    private static DashboardUiBinder uiBinder = GWT
            .create(DashboardUiBinder.class);
    @UiField SideBar sideBar;

    interface DashboardUiBinder extends UiBinder<Widget, DownloadsView> {
    }

    public DownloadsView() {
        initWidget(uiBinder.createAndBindUi(this));
    }

    public void setUserInfo(String firstName, String lastName, String email) {
        sideBar.setUserInfo(firstName, lastName, email);
    }

    public void setPresenter(DownloadsView.Presenter downloadsActivity) {
        sideBar.setActivePage("downloads");
    }

}
