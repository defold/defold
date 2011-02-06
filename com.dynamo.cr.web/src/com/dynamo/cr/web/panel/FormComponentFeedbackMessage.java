package com.dynamo.cr.web.panel;

import org.apache.wicket.Component;
import org.apache.wicket.feedback.ContainerFeedbackMessageFilter;
import org.apache.wicket.feedback.FeedbackMessage;
import org.apache.wicket.feedback.IFeedback;
import org.apache.wicket.feedback.IFeedbackMessageFilter;
import org.apache.wicket.markup.html.basic.Label;
import org.apache.wicket.markup.html.border.Border;
import org.apache.wicket.model.PropertyModel;

public class FormComponentFeedbackMessage extends Border implements IFeedback {

    private Component component;

    private static final long serialVersionUID = 1L;

    private boolean visible;

    public FormComponentFeedbackMessage(String id, Component component) {
        super(id);

        add(new Label("errorMessage", new PropertyModel<String>(this, "message")) {
            private static final long serialVersionUID = 1L;

            @Override
            public boolean isVisible() {
                return visible;
            }
        });
        this.component = component;
    }

    public String getMessage() {
        FeedbackMessage message = component.getFeedbackMessage();
        if (message != null) {
            message.markRendered();
            return message.getMessage().toString();
        }
        else
            return "";
    }

    @Override
    protected void onBeforeRender() {
        super.onBeforeRender();
        visible = component.getFeedbackMessage() != null;
    }

    protected IFeedbackMessageFilter getMessagesFilter() {
        return new ContainerFeedbackMessageFilter(this);
    }

    @Override
    public boolean isVisible() {
        return super.isVisible();
    }

}
