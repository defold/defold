package com.dynamo.cr.web2.client.activity;

import com.dynamo.cr.web2.client.ClientFactory;
import com.dynamo.cr.web2.client.place.BlogPlace;
import com.dynamo.cr.web2.client.ui.BlogView;
import com.google.gwt.event.shared.EventBus;
import com.google.gwt.user.client.ui.AcceptsOneWidget;

public class BlogActivity extends AsciiDocActivity implements BlogView.Presenter {
    public BlogActivity(BlogPlace place, ClientFactory clientFactory) {
        super(clientFactory);
    }

    @Override
    public void start(AcceptsOneWidget containerWidget, EventBus eventBus) {
        final BlogView blogView = clientFactory.getBlogView();
        containerWidget.setWidget(blogView.asWidget());
        blogView.setPresenter(this);
        loadAsciiDoc(blogView, "blog");
    }

    @Override
    public void onBlog() {
        clientFactory.getPlaceController().goTo(new BlogPlace());
    }
}
