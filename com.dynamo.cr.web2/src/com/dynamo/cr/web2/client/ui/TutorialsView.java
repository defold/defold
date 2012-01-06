package com.dynamo.cr.web2.client.ui;

import com.dynamo.cr.web2.client.AsciiDocUtil;
import com.google.gwt.core.client.GWT;
import com.google.gwt.event.dom.client.ClickEvent;
import com.google.gwt.uibinder.client.UiBinder;
import com.google.gwt.uibinder.client.UiField;
import com.google.gwt.uibinder.client.UiHandler;
import com.google.gwt.user.client.ui.Anchor;
import com.google.gwt.user.client.ui.Composite;
import com.google.gwt.user.client.ui.HTMLPanel;
import com.google.gwt.user.client.ui.Image;
import com.google.gwt.user.client.ui.Widget;

public class TutorialsView extends Composite implements IAsciiDocView {

    public interface Presenter {
        void onTutorial(String string);
    }

    private static DashboardUiBinder uiBinder = GWT
            .create(DashboardUiBinder.class);

    interface DashboardUiBinder extends UiBinder<Widget, TutorialsView> {
    }

    @UiField Anchor tutorial01Link;
    @UiField Anchor tutorial02Link;
    @UiField HTMLPanel tutorial;
    @UiField Image loader;

    private Presenter listener;

    public TutorialsView() {
        initWidget(uiBinder.createAndBindUi(this));
        loader.setVisible(false);
    }

    public void setPresenter(TutorialsView.Presenter listener) {
        this.listener = listener;
    }

    @UiHandler("tutorial01Link")
    void onTutorial01LinkClick(ClickEvent event) {
        listener.onTutorial("tutorial_falling_box");
    }

    @UiHandler("tutorial02Link")
    void onTutorial02LinkClick(ClickEvent event) {
        listener.onTutorial("tutorial02");
    }

    /* (non-Javadoc)
     * @see com.dynamo.cr.web2.client.ui.AsciiDocView#setTutorial(java.lang.String)
     */
    @Override
    public void setText(String html) {
        /*
         * Extract document payload, ie within <body></body>. We must
         * remove the inline css. The inline css disturb the default one. See ascii.css
         * for more information.
         */
        tutorial.clear();
        loader.setVisible(false);
        tutorial.add(AsciiDocUtil.extractBody(html));
    }

    /* (non-Javadoc)
     * @see com.dynamo.cr.web2.client.ui.AsciiDocView#setLoading(boolean)
     */
    @Override
    public void setLoading(boolean loading) {
        loader.setVisible(loading);
    }

    /* (non-Javadoc)
     * @see com.dynamo.cr.web2.client.ui.AsciiDocView#clear()
     */
    @Override
    public void clear() {
        tutorial.clear();
    }
}
