package com.dynamo.cr.web.pages;

import java.io.IOException;
import java.util.ArrayList;
import java.util.Date;
import java.util.List;

import org.apache.wicket.PageParameters;
import org.apache.wicket.authorization.strategies.role.annotations.AuthorizeInstantiation;
import org.apache.wicket.markup.html.basic.Label;
import org.apache.wicket.markup.html.link.Link;
import org.apache.wicket.markup.html.list.ListItem;
import org.apache.wicket.markup.html.list.ListView;

import com.dynamo.cr.protocol.proto.Protocol.ProjectInfo;
import com.dynamo.cr.protocol.proto.Protocol.ProjectList;
import com.dynamo.cr.web.Session;
import com.dynamo.cr.web.User;
import com.dynamo.cr.web.util.SerializableMessage;
import com.ocpsoft.pretty.time.PrettyTime;
import com.sun.jersey.api.client.WebResource;

@AuthorizeInstantiation("USER")
public class DashboardPage extends BasePage {

    public DashboardPage() throws IOException {

        Session session = (Session) getSession();
        WebResource webResource = session.getWebResource();
        User user = session.getUser();

        ProjectList projectList = webResource.path("/projects/"  + user.getUserId())
                .accept("application/x-protobuf")
                .get(ProjectList.class);

        List<SerializableMessage<ProjectInfo>> list = new ArrayList<SerializableMessage<ProjectInfo>>();
        for (ProjectInfo pi : projectList.getProjectsList()) {
            list.add(new SerializableMessage<ProjectInfo>(pi));
        }

        ListView<SerializableMessage<ProjectInfo>> listView = new ListView<SerializableMessage<ProjectInfo>>("projects") {
            private static final long serialVersionUID = 1L;

            @Override
            protected void populateItem(final ListItem<SerializableMessage<ProjectInfo>> project) {
                ProjectInfo projectInfo = project.getModelObject().message;

                Link<ProjectPage> link = new Link<ProjectPage>("project") {

                    private static final long serialVersionUID = 1L;

                    @Override
                    public void onClick() {
                        PageParameters params = new PageParameters();
                        long id = project.getModelObject().message.getId();
                        params.add("id", Long.toString(id));
                        setResponsePage(ProjectPage.class, params);
                    }
                };

                project.add(link);
                link.add(new Label("projectName", projectInfo.getName() + " / " + projectInfo.getOwner().getEmail()));
                project.add(new Label("description", projectInfo.getDescription()));

                PrettyTime pretty = new PrettyTime();
                Date created = new Date(projectInfo.getCreated());
                Date lastUpdated = new Date(projectInfo.getLastUpdated());
                project.add(new Label("created", pretty.format(created)));
                project.add(new Label("lastUpdated", pretty.format(lastUpdated)));

            }
        };
        listView.setList(list);
        add(listView);
    }
}
