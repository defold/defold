package com.dynamo.cr.web2.client.ui;

import com.dynamo.cr.web2.client.AsciiDocUtil;
import com.google.gwt.core.client.GWT;
import com.google.gwt.uibinder.client.UiBinder;
import com.google.gwt.uibinder.client.UiField;
import com.google.gwt.user.client.ui.Composite;
import com.google.gwt.user.client.ui.HTMLPanel;
import com.google.gwt.user.client.ui.Image;
import com.google.gwt.user.client.ui.Widget;

public class BlogView extends Composite {

    public interface Presenter {
        void onBlog();
    }

    private static DashboardUiBinder uiBinder = GWT
            .create(DashboardUiBinder.class);

    interface DashboardUiBinder extends UiBinder<Widget, BlogView> {
    }

    @UiField HTMLPanel blog;
    @UiField Image loader;

    public BlogView() {
        initWidget(uiBinder.createAndBindUi(this));
        loader.setVisible(false);
    }

    public void setPresenter(BlogView.Presenter listener) {
    }

    public void setBlog(String html) {
        /*
         * Extract document payload, ie within <body></body>. We must
         * remove the inline css. The inline css disturb the default one. See ascii.css
         * for more information.
         */
        blog.clear();
        loader.setVisible(false);
        blog.add(AsciiDocUtil.extractBody(html));
    }

    public void setLoading(boolean loading) {
        loader.setVisible(loading);
    }

    public void clear() {
        blog.clear();
    }
}
