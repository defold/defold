package com.dynamo.cr.web.pages;

import java.io.Serializable;
import java.util.ArrayList;

import org.apache.wicket.feedback.FeedbackMessage;
import org.apache.wicket.feedback.IFeedbackMessageFilter;
import org.apache.wicket.markup.html.form.RadioChoice;
import org.apache.wicket.markup.html.form.StatelessForm;
import org.apache.wicket.markup.html.form.TextField;
import org.apache.wicket.markup.html.panel.FeedbackPanel;
import org.apache.wicket.model.Model;
import org.apache.wicket.model.PropertyModel;
import org.apache.wicket.util.value.ValueMap;

import com.dynamo.cr.protocol.proto.Protocol.NewProject;
import com.dynamo.cr.protocol.proto.Protocol.ProjectInfo;
import com.dynamo.cr.protocol.proto.Protocol.ProjectTemplateInfo;
import com.dynamo.cr.protocol.proto.Protocol.ProjectTemplateInfoList;
import com.dynamo.cr.web.Session;
import com.dynamo.cr.web.User;
import com.dynamo.cr.web.panel.FormComponentFeedbackMessage;
import com.dynamo.cr.web.providers.ProtobufProviders;
import com.sun.jersey.api.client.WebResource;

public class NewProjectPage extends BasePage {

    private TextField<String> name;
    private TextField<String> description;
    private RadioChoice<Template> projectTemplate;

    static class Template implements Serializable {
        private static final long serialVersionUID = 1L;

        String id;
        String description;

        public Template(String id, String description) {
            this.id = id;
            this.description = description;
        }

        @Override
        public String toString() {
            return description;
        }

    }

    public final class NewProjectForm extends StatelessForm<Void>
    {
        private static final long serialVersionUID = 1L;

        private final ValueMap properties = new ValueMap();

        public NewProjectForm(final String id)
        {
            super(id);

            Session session = (Session) getSession();
            WebResource webResource = session.getWebResource();

            ProjectTemplateInfoList templateList = webResource
                    .path("repository/project_templates")
                    .accept(ProtobufProviders.APPLICATION_XPROTOBUF)
                    .get(ProjectTemplateInfoList.class);

            ArrayList<Template> templates = new ArrayList<Template>();
            for (ProjectTemplateInfo info : templateList.getTemplatesList()) {
                templates.add(new Template(info.getId(), info.getDescription()));
            }

            name = new TextField<String>("name", new PropertyModel<String>(properties, "name"));
            name.setRequired(true);
            description = new TextField<String>("description", new PropertyModel<String>(properties, "description"));

            projectTemplate = new RadioChoice<Template>("projectTemplate", new Model<Template>(), templates );
            projectTemplate.setRequired(true);

            FormComponentFeedbackMessage nameBorder = new FormComponentFeedbackMessage("nameBorder", name);
            add(nameBorder);
            nameBorder.add(name);

            FormComponentFeedbackMessage descriptionBorder = new FormComponentFeedbackMessage("descriptionBorder", description);
            add(descriptionBorder);
            descriptionBorder.add(description);

            FormComponentFeedbackMessage projectTemplateBorder = new FormComponentFeedbackMessage("projectTemplateBorder", projectTemplate);
            add(projectTemplateBorder);
            projectTemplateBorder.add(projectTemplate);

            name.setType(String.class);
        }

        /**
         * @see org.apache.wicket.markup.html.form.Form#onSubmit()
         */
        @Override
        public final void onSubmit()
        {
            Template template = projectTemplate.getModelObject();

            Session session = (Session) getSession();
            WebResource webResource = session.getWebResource();
            User user = session.getUser();

            System.out.println(projectTemplate);
            NewProject newProject = NewProject.newBuilder()
                .setName(name.getDefaultModelObjectAsString())
                .setTemplateId(template.id)
                .setDescription(description.getDefaultModelObjectAsString()).build();

            webResource
                .path("projects/" + user.getUserId())
                .accept(ProtobufProviders.APPLICATION_XPROTOBUF)
                .type(ProtobufProviders.APPLICATION_XPROTOBUF)
                .post(ProjectInfo.class, newProject);

            if (!continueToOriginalDestination())
            {
                setResponsePage(DashboardPage.class);
            }
        }
    }

    public NewProjectPage() {
        final FeedbackPanel feedback = new FeedbackPanel("feedback");
        feedback.setFilter(new IFeedbackMessageFilter() {
            private static final long serialVersionUID = 1L;

            @Override
            public boolean accept(FeedbackMessage msg) {
                // Only show messages related to form
                // Form-component messages are handled by FormComponentFeedbackMessage
                return msg.getReporter().getClass() == NewProjectForm.class;
            }
        });

        add(feedback);
        add(new NewProjectForm("newProjectForm"));
    }

    public String getName() {
        return name.getDefaultModelObjectAsString();
    }

    public String getDescription() {
        return description.getDefaultModelObjectAsString();
    }

    public RadioChoice<Template> getProjectTemplate() {
        return projectTemplate;
    }

}
