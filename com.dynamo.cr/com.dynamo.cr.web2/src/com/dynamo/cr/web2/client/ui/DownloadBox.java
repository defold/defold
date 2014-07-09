package com.dynamo.cr.web2.client.ui;

import com.dynamo.cr.web2.shared.ClientUtil;
import com.google.gwt.core.client.GWT;
import com.google.gwt.dom.client.AnchorElement;
import com.google.gwt.uibinder.client.UiBinder;
import com.google.gwt.uibinder.client.UiField;
import com.google.gwt.user.client.ui.Composite;
import com.google.gwt.user.client.ui.Widget;

public class DownloadBox extends Composite {

    private static DownloadBoxUiBinder uiBinder = GWT
            .create(DownloadBoxUiBinder.class);

    private static final String HREF_BASE = "http://d.defold.com/stable/Defold-";
    private static final String HREF_WIN32 = "win32.win32.x86";
    private static final String HREF_OSX = "macosx.cocoa.x86_64";
    private static final String HREF_LINUX = "linux.gtk.x86";
    private static final String LABEL_WIN32 = "Windows";
    private static final String LABEL_OSX = "Mac OS X";
    private static final String LABEL_LINUX = "Linux";

    interface DownloadBoxUiBinder extends UiBinder<Widget, DownloadBox> {
    }

    @UiField AnchorElement mainDownload;
    @UiField AnchorElement alt1Download;
    @UiField AnchorElement alt2Download;

    public DownloadBox() {
        initWidget(uiBinder.createAndBindUi(this));
        ClientUtil.Platform platform = ClientUtil.detectPlatform();
        final String href_ext = ".zip";
        switch (platform) {
        case OSX:
            this.mainDownload.setHref(HREF_BASE + HREF_OSX + href_ext);
            this.mainDownload.setInnerText(LABEL_OSX);
            this.alt1Download.setHref(HREF_BASE + HREF_WIN32 + href_ext);
            this.alt1Download.setInnerText(LABEL_WIN32);
            this.alt2Download.setHref(HREF_BASE + HREF_LINUX + href_ext);
            this.alt2Download.setInnerText(LABEL_LINUX);
            break;
        case Linux:
            this.mainDownload.setHref(HREF_BASE + HREF_LINUX + href_ext);
            this.mainDownload.setInnerText(LABEL_LINUX);
            this.alt1Download.setHref(HREF_BASE + HREF_WIN32 + href_ext);
            this.alt1Download.setInnerText(LABEL_WIN32);
            this.alt2Download.setHref(HREF_BASE + HREF_OSX + href_ext);
            this.alt2Download.setInnerText(LABEL_OSX);
            break;
        case Win32:
        default:
            this.mainDownload.setHref(HREF_BASE + HREF_WIN32 + href_ext);
            this.mainDownload.setInnerText(LABEL_WIN32);
            this.alt1Download.setHref(HREF_BASE + HREF_OSX + href_ext);
            this.alt1Download.setInnerText(LABEL_OSX);
            this.alt2Download.setHref(HREF_BASE + HREF_LINUX + href_ext);
            this.alt2Download.setInnerText(LABEL_LINUX);
            break;
        }
    }

}
