package com.dynamo.cr.web2.client.ui;

import com.dynamo.cr.web2.client.AsciiDocUtil;
import com.google.gwt.core.client.GWT;
import com.google.gwt.uibinder.client.UiBinder;
import com.google.gwt.uibinder.client.UiField;
import com.google.gwt.user.client.ui.Composite;
import com.google.gwt.user.client.ui.HTMLPanel;
import com.google.gwt.user.client.ui.Image;
import com.google.gwt.user.client.ui.Widget;

public class ContentView extends Composite implements IAsciiDocView {

    public interface Presenter {
        void onContent(String name);
    }

    private static DashboardUiBinder uiBinder = GWT
            .create(DashboardUiBinder.class);

    interface DashboardUiBinder extends UiBinder<Widget, ContentView> {
    }

    @UiField HTMLPanel content;
    @UiField Image loader;

    public ContentView() {
        initWidget(uiBinder.createAndBindUi(this));
        loader.setVisible(false);
    }

    public void setPresenter(ContentView.Presenter listener) {
    }

    @Override
    public void setText(String html) {
        /*
         * Extract document payload, ie within <body></body>. We must
         * remove the inline css. The inline css disturb the default one. See ascii.css
         * for more information.
         */
        content.clear();
        loader.setVisible(false);
        content.add(AsciiDocUtil.extractBody(html));
    }

    @Override
    public void setLoading(boolean loading) {
        loader.setVisible(loading);
    }

    @Override
    public void clear() {
        content.clear();
    }
}
