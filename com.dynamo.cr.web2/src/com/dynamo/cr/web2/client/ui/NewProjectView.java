package com.dynamo.cr.web2.client.ui;

import com.dynamo.cr.web2.client.ProjectTemplateInfo;
import com.dynamo.cr.web2.client.ProjectTemplateInfoList;
import com.google.gwt.core.client.GWT;
import com.google.gwt.core.client.JsArray;
import com.google.gwt.event.dom.client.ClickEvent;
import com.google.gwt.uibinder.client.UiBinder;
import com.google.gwt.uibinder.client.UiField;
import com.google.gwt.uibinder.client.UiHandler;
import com.google.gwt.user.client.ui.Button;
import com.google.gwt.user.client.ui.Composite;
import com.google.gwt.user.client.ui.ListBox;
import com.google.gwt.user.client.ui.TextBox;
import com.google.gwt.user.client.ui.Widget;
import com.google.gwt.user.client.ui.Label;

public class NewProjectView extends Composite {

    public interface Presenter {

        void createProject(String name, String description, String templateId);

    }

    private static final Binder binder = GWT.create(Binder.class);
    @UiField Button createButton;
    @UiField ListBox templatesCombo;
    @UiField TextBox nameTextBox;
    @UiField TextBox descriptionTextBox;
    @UiField Label errorLabel;

    interface Binder extends UiBinder<Widget, NewProjectView> {
    }

    private Presenter listener;

    public NewProjectView() {
        initWidget(binder.createAndBindUi(this));
    }

    public void setPresenter(Presenter listener) {
        this.listener = listener;
    }

    public void setProjectTemplates(ProjectTemplateInfoList result) {
        JsArray<ProjectTemplateInfo> list = result.getTemplates();
        for (int i = 0; i < list.length(); ++i) {
            ProjectTemplateInfo projectTemplate = list.get(i);
            templatesCombo.addItem(projectTemplate.getDescription(), projectTemplate.getId());
        }
    }

    @UiHandler("createButton")
    void onCreateButtonClick(ClickEvent event) {
        listener.createProject(nameTextBox.getText(), descriptionTextBox.getText(), templatesCombo.getValue(templatesCombo.getSelectedIndex()));
    }

    public void setError(String message) {
        errorLabel.setText(message);
    }
}
