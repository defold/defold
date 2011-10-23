package com.dynamo.cr.web2.client.activity;

import com.dynamo.cr.web2.client.ClientFactory;
import com.dynamo.cr.web2.client.place.BlogPlace;
import com.dynamo.cr.web2.client.ui.BlogView;
import com.google.gwt.activity.shared.AbstractActivity;
import com.google.gwt.event.shared.EventBus;
import com.google.gwt.http.client.Request;
import com.google.gwt.http.client.RequestBuilder;
import com.google.gwt.http.client.RequestCallback;
import com.google.gwt.http.client.RequestException;
import com.google.gwt.http.client.Response;
import com.google.gwt.user.client.ui.AcceptsOneWidget;

public class BlogActivity extends AbstractActivity implements BlogView.Presenter {
    private ClientFactory clientFactory;

    public BlogActivity(BlogPlace place, ClientFactory clientFactory) {
        this.clientFactory = clientFactory;
    }

    @Override
    public void start(AcceptsOneWidget containerWidget, EventBus eventBus) {
        final BlogView blogView = clientFactory.getBlogView();
        containerWidget.setWidget(blogView.asWidget());
        blogView.setPresenter(this);
        loadBlog("blog");
    }

    @Override
    public void onBlog() {
        clientFactory.getPlaceController().goTo(new BlogPlace());
    }

    public void loadBlog(String name) {
        final BlogView blogView = clientFactory.getBlogView();
        blogView.setLoading(true);
        blogView.clear();

        RequestBuilder builder = new RequestBuilder(RequestBuilder.GET, "/site/" + name + ".html");
        try {
            builder.sendRequest("", new RequestCallback() {
                @Override
                public void onResponseReceived(Request request, Response response) {
                    int status = response.getStatusCode();
                    if (status == 200) {
                        blogView.setLoading(false);
                        blogView.setBlog(response.getText());

                    } else {
                        blogView.setLoading(false);
                        clientFactory.getDefold().showErrorMessage("Unable to load blog");
                    }
                }

                @Override
                public void onError(Request request, Throwable exception) {
                    blogView.setLoading(false);
                    clientFactory.getDefold().showErrorMessage("Unable to load blog");
                }
            });
        } catch (RequestException e) {
            blogView.setLoading(false);
            clientFactory.getDefold().showErrorMessage("Unable to load blog");
        }
    }

}
