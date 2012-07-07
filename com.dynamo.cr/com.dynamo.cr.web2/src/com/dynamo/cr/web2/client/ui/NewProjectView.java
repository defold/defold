package com.dynamo.cr.web2.client.ui;

import com.dynamo.cr.web2.client.ProjectTemplateInfo;
import com.dynamo.cr.web2.client.ProjectTemplateInfoList;
import com.google.gwt.core.client.GWT;
import com.google.gwt.core.client.JsArray;
import com.google.gwt.dom.client.DivElement;
import com.google.gwt.dom.client.SpanElement;
import com.google.gwt.dom.client.Style.Visibility;
import com.google.gwt.event.dom.client.ClickEvent;
import com.google.gwt.uibinder.client.UiBinder;
import com.google.gwt.uibinder.client.UiField;
import com.google.gwt.uibinder.client.UiHandler;
import com.google.gwt.user.client.ui.Button;
import com.google.gwt.user.client.ui.Composite;
import com.google.gwt.user.client.ui.ListBox;
import com.google.gwt.user.client.ui.TextBox;
import com.google.gwt.user.client.ui.Widget;

public class NewProjectView extends Composite {

    public interface Presenter {
        void createProject(String name, String description, String templateId);
    }

    private static final Binder binder = GWT.create(Binder.class);
    @UiField Button createButton;
    @UiField ListBox templatesCombo;
    @UiField TextBox nameTextBox;
    @UiField TextBox descriptionTextBox;
    @UiField SpanElement nameHelp;
    @UiField DivElement nameGroup;

    interface Binder extends UiBinder<Widget, NewProjectView> {
    }

    private Presenter listener;

    public NewProjectView() {
        initWidget(binder.createAndBindUi(this));
        createButton.addStyleName("btn btn-success");
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
        String name = nameTextBox.getText();
        if (name.length() == 0) {
            nameGroup.addClassName("error");
            nameHelp.getStyle().setVisibility(Visibility.VISIBLE);
        } else {
            listener.createProject(name, descriptionTextBox.getText(), templatesCombo.getValue(templatesCombo.getSelectedIndex()));
        }
    }

    public void init() {
        templatesCombo.clear();
        nameGroup.removeClassName("error");
        nameHelp.getStyle().setVisibility(Visibility.HIDDEN);
    }
}
