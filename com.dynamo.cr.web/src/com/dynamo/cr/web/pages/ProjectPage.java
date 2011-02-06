package com.dynamo.cr.web.pages;

import java.util.ArrayList;
import java.util.Iterator;
import java.util.List;

import org.apache.wicket.PageParameters;
import org.apache.wicket.Response;
import org.apache.wicket.authorization.strategies.role.annotations.AuthorizeInstantiation;
import org.apache.wicket.extensions.ajax.markup.html.autocomplete.AbstractAutoCompleteRenderer;
import org.apache.wicket.extensions.ajax.markup.html.autocomplete.AutoCompleteTextField;
import org.apache.wicket.feedback.FeedbackMessage;
import org.apache.wicket.feedback.IFeedbackMessageFilter;
import org.apache.wicket.markup.html.basic.Label;
import org.apache.wicket.markup.html.form.Form;
import org.apache.wicket.markup.html.link.Link;
import org.apache.wicket.markup.html.list.ListItem;
import org.apache.wicket.markup.html.list.ListView;
import org.apache.wicket.markup.html.panel.FeedbackPanel;
import org.apache.wicket.model.IModel;
import org.apache.wicket.model.Model;

import com.dynamo.cr.protocol.proto.Protocol.ProjectInfo;
import com.dynamo.cr.protocol.proto.Protocol.UserInfo;
import com.dynamo.cr.protocol.proto.Protocol.UserInfoList;
import com.dynamo.cr.web.Session;
import com.dynamo.cr.web.User;
import com.dynamo.cr.web.panel.FormComponentFeedbackMessage;
import com.dynamo.cr.web.providers.ProtobufProviders;
import com.dynamo.cr.web.util.SerializableMessage;
import com.sun.jersey.api.client.UniformInterfaceException;
import com.sun.jersey.api.client.WebResource;

// TODO: must be a member or own the project!
// Should be protected at db-level as well
@AuthorizeInstantiation("USER")
public class ProjectPage extends BasePage {

    private SerializableMessage<ProjectInfo> projectInfo;
    private AutoCompleteMember addMember;
    private Integer projectId;

    static class Renderer extends AbstractAutoCompleteRenderer<SerializableMessage<UserInfo>> {
        private static final long serialVersionUID = 1L;

        @Override
        protected String getTextValue(SerializableMessage<UserInfo> object) {
            UserInfo m = object.message;
            return m.getEmail();
        }

        @Override
        protected void renderChoice(SerializableMessage<UserInfo> object,
                Response response, String critera) {
            UserInfo m = object.message;
            String s = String.format("%s %s (%s)", m.getFirstName(), m.getLastName(), m.getEmail());
            response.write(s);
        }
    }

    static class AutoCompleteMember extends AutoCompleteTextField<SerializableMessage<UserInfo>> {
        private static final long serialVersionUID = 1L;

        String firstName;
        private List<SerializableMessage<UserInfo>> userInfoList;

        public AutoCompleteMember(String id, IModel<SerializableMessage<UserInfo>> object) {
            super(id, object, new Renderer());
            Session session = (Session) getSession();
            User user = session.getUser();
            WebResource webResource = session.getWebResource();
            UserInfoList lst = webResource
                .path(String.format("/users/%d/connections", user.getUserId()))
                .accept(ProtobufProviders.APPLICATION_XPROTOBUF)
                .type(ProtobufProviders.APPLICATION_XPROTOBUF)
                .get(UserInfoList.class);

            this.userInfoList = SerializableMessage.fromList(lst.getUsersList());
        }

        @Override
        protected Iterator<SerializableMessage<UserInfo>> getChoices(String input) {
            List<SerializableMessage<UserInfo>> ret = new ArrayList<SerializableMessage<UserInfo>>();
            input = input.toLowerCase();
            for (SerializableMessage<UserInfo> userInfo : userInfoList) {
                UserInfo m = userInfo.message;
                if (m.getFirstName().toLowerCase().indexOf(input) != -1 ||
                    m.getLastName().toLowerCase().indexOf(input) != -1 ||
                    m.getEmail().toLowerCase().indexOf(input) != -1) {
                    ret.add(userInfo);
                }
            }
            return ret.iterator();
        }
    }

    public ProjectPage(PageParameters parmeters) {
        projectId = parmeters.getAsInteger("id");

        Session session = (Session) getSession();
        User user = session.getUser();
        WebResource webResource = session.getWebResource();
        ProjectInfo p = webResource
            .path(String.format("/projects/%d/%d/project_info", user.getUserId(), projectId))
            .accept(ProtobufProviders.APPLICATION_XPROTOBUF)
            .type(ProtobufProviders.APPLICATION_XPROTOBUF)
            .get(ProjectInfo.class);

        this.projectInfo = new SerializableMessage<ProjectInfo>(p);

        final FeedbackPanel feedback = new FeedbackPanel("feedback");
        feedback.setFilter(new IFeedbackMessageFilter() {
            private static final long serialVersionUID = 1L;

            @Override
            public boolean accept(FeedbackMessage msg) {
                ///return true;
                // Only show messages related to form
                // Form-component messages are handled by FormComponentFeedbackMessage
                return msg.getReporter().getClass() == Form.class;
            }
        });

        add(feedback);

        Form<Void> form = new Form<Void>("form") {
            private static final long serialVersionUID = 1L;

            protected void onSubmit() {
                String email = addMember.getDefaultModelObjectAsString();

                Session session = (Session) getSession();
                User user = session.getUser();
                try {
                    WebResource webResource = session.getWebResource();
                    webResource
                        .path(String.format("/projects/%d/%d/members", user.getUserId(), projectId))
                        .accept(ProtobufProviders.APPLICATION_XPROTOBUF)
                        .post(email);
                }
                catch (UniformInterfaceException e) {
                    if (e.getResponse().getStatus() == 404) {
                        addMember.error("User not found");
                        return;                    }
                    else {
                        throw new RuntimeException(e);
                    }
                }

                PageParameters params = new PageParameters();
                params.add("id", projectId.toString());
                setResponsePage(ProjectPage.class, params);
            }
        };
        add(form);

        add(new Label("name", projectInfo.message.getName()));
        add(new Label("description", projectInfo.message.getDescription()));
        addMember = new AutoCompleteMember("addMember", new Model<SerializableMessage<UserInfo>>());

        FormComponentFeedbackMessage addMemberBorder = new FormComponentFeedbackMessage("addMemberBorder", addMember);
        form.add(addMemberBorder);
        addMemberBorder.add(addMember);

//        form.add(addMember);
        List<SerializableMessage<UserInfo>> list = new ArrayList<SerializableMessage<UserInfo>>();

        for (UserInfo userInfo : projectInfo.message.getMembersList()) {
            if (projectInfo.message.getOwner().getId() != userInfo.getId())
                list.add(new SerializableMessage<UserInfo>(userInfo));
        }

        ListView<SerializableMessage<UserInfo>> listView = new ListView<SerializableMessage<UserInfo>>("members") {
            private static final long serialVersionUID = 1L;

            @Override
            protected void populateItem(final ListItem<SerializableMessage<UserInfo>> userInfoItem) {
                SerializableMessage<UserInfo> userInfo = userInfoItem.getModelObject();
                userInfoItem.add(new Label("memberName", String.format("%s %s", userInfo.message.getFirstName(), userInfo.message.getLastName())));
                userInfoItem.add(new Label("memberEmail", userInfo.message.getEmail()));
                Link<String> removeLink = new Link<String>("removeMember") {

                    private static final long serialVersionUID = 1L;

                    @Override
                    public void onClick() {
                        Session session = (Session) getSession();
                        User user = session.getUser();
                        WebResource webResource = session.getWebResource();
                        webResource
                            .path(String.format("/projects/%d/%d/members/%d", user.getUserId(), projectId, userInfoItem.getModel().getObject().message.getId()))
                            .accept(ProtobufProviders.APPLICATION_XPROTOBUF)
                            .delete();

                        PageParameters params = new PageParameters();
                        params.add("id", projectId.toString());
                        setResponsePage(ProjectPage.class, params);
                    }
                };
                userInfoItem.add(removeLink);
                //userInfoItem.add(new Label("removeMember", ""));

            }
        };
        listView.setList(list);
        add(listView);
    }
}
