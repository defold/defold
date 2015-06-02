package com.dynamo.cr.projed;

import java.util.HashMap;
import java.util.List;
import java.util.Map;

import org.eclipse.core.resources.IFile;
import org.eclipse.core.resources.IFolder;
import org.eclipse.core.resources.IResource;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.jface.dialogs.Dialog;
import org.eclipse.jface.dialogs.IMessageProvider;
import org.eclipse.swt.SWT;
import org.eclipse.swt.events.FocusEvent;
import org.eclipse.swt.events.FocusListener;
import org.eclipse.swt.events.KeyEvent;
import org.eclipse.swt.events.KeyListener;
import org.eclipse.swt.events.SelectionEvent;
import org.eclipse.swt.events.SelectionListener;
import org.eclipse.swt.layout.GridData;
import org.eclipse.swt.layout.GridLayout;
import org.eclipse.swt.widgets.Button;
import org.eclipse.swt.widgets.Composite;
import org.eclipse.swt.widgets.Label;
import org.eclipse.swt.widgets.Text;
import org.eclipse.swt.widgets.Widget;
import org.eclipse.ui.dialogs.ResourceListSelectionDialog;
import org.eclipse.ui.forms.widgets.FormToolkit;
import org.eclipse.ui.forms.widgets.ScrolledForm;
import org.eclipse.ui.forms.widgets.Section;

import com.dynamo.cr.editor.core.EditorUtil;

public class ProjectEditorView implements SelectionListener, KeyListener, FocusListener, IProjectEditorView {

    private FormToolkit toolkit;
    private ScrolledForm form;
    private IPresenter presenter;
    private Map<KeyMeta, Widget> metaToWidget = new HashMap<KeyMeta, Widget>();

    public interface IPresenter {
        void setValue(KeyMeta keyMeta, String value);
    }

    public ProjectEditorView(IPresenter presenter) {
        this.presenter = presenter;
    }

    public void createPartControl(Composite parent, List<CategoryMeta> meta) {
        toolkit = new FormToolkit(parent.getDisplay());
        toolkit.setBorderStyle(SWT.BORDER);
        form = toolkit.createScrolledForm(parent);
        form.setText("Project Settings");
        form.getBody().setLayout(new GridLayout());

        for (CategoryMeta categoryMeta : meta) {
            Section section = toolkit.createSection(form.getBody(), Section.TITLE_BAR | Section.EXPANDED | Section.DESCRIPTION);
            section.setDescription(categoryMeta.getDescription());
            section.setText(categoryMeta.getName());
            GridData gd = new GridData();
            gd.widthHint = 500;
            section.setLayoutData(gd);
            Composite sectionComposite = toolkit.createComposite(section);
            sectionComposite.setLayout(new GridLayout(3, false));
            for (KeyMeta keyMeta : categoryMeta.getKeys()) {
                createKeyControl(sectionComposite, keyMeta);
            }
            section.setClient(sectionComposite);
        }
    }

    private static GridData spanTwo() {
        GridData gd = new GridData(GridData.FILL_HORIZONTAL);
        gd.horizontalSpan = 2;
        return gd;
    }

    private Text createText(Composite parent, KeyMeta keyMeta, String value) {
        Text text = toolkit.createText(parent, value);
        text.setData(keyMeta);
        metaToWidget.put(keyMeta, text);
        text.addKeyListener(this);
        text.addFocusListener(this);
        return text;
    }

    private void createKeyControl(Composite sectionComposite, KeyMeta keyMeta) {
        Label label = toolkit.createLabel(sectionComposite, keyMeta.getName() + ":");
        label.setToolTipText(keyMeta.getHelp());
        label.setLayoutData(new GridData());

        GridData gd;
        Button button;

        switch (keyMeta.getType()) {
        case BOOLEAN:
            button = toolkit.createButton(sectionComposite, "", SWT.CHECK);
            button.setLayoutData(spanTwo());
            button.addSelectionListener(this);
            button.setData(keyMeta);
            metaToWidget.put(keyMeta, button);
            break;

        case STRING:
            createText(sectionComposite, keyMeta, "").setLayoutData(spanTwo());
            break;

        case NUMBER:
        case INTEGER:
            createText(sectionComposite, keyMeta, "").setLayoutData(spanTwo());
            break;

        case RESOURCE:
            gd = new GridData(GridData.FILL_HORIZONTAL);
            Text text = createText(sectionComposite, keyMeta, "");
            text.setLayoutData(gd);
            gd = new GridData(SWT.DEFAULT, 1);
            gd.verticalAlignment = GridData.FILL;
            Button resourceButton = toolkit.createButton(sectionComposite, "...", SWT.FLAT);
            resourceButton.setData("resourceButton", true);
            resourceButton.setData("dataWidget", text);
            resourceButton.addSelectionListener(this);
            resourceButton.setLayoutData(gd);
        }
    }

    public void setFocus() {
        form.setFocus();
    }

    @Override
    public void widgetSelected(SelectionEvent e) {
        if (e.widget.getData("resourceButton") != null) {
            IFolder root = EditorUtil.getContentRoot(EditorUtil.getProject());
            ResourceListSelectionDialog dialog = new ResourceListSelectionDialog(form.getShell(), root, IResource.FILE);
            dialog.setTitle("Select resource");
            if (dialog.open() == Dialog.OK) {
                IFile resource = (IFile) dialog.getResult()[0];
                Text text = (Text) e.widget.getData("dataWidget");
                KeyMeta keyMeta = (KeyMeta) text.getData();
                String path = EditorUtil.makeResourcePath(resource);
                // TODO: A hack until we compile the game.project-file
                if (!path.endsWith(".icns") && !path.endsWith(".ico") && !path.endsWith(".png")
                     && !path.endsWith(".html") && !path.endsWith(".css") && !path.endsWith(".plist") && !path.endsWith(".xml")
                     && !path.endsWith(".texture_profiles")) {
                    path += "c";
                }
                presenter.setValue(keyMeta, path);
                text.setText(path);
            }
        } else {
            KeyMeta keyMeta = (KeyMeta) e.widget.getData();
            Button button = (Button) e.widget;
            presenter.setValue(keyMeta, button.getSelection() ? "1" : "0");
        }
    }

    @Override
    public void widgetDefaultSelected(SelectionEvent e) {}

    @Override
    public void keyPressed(KeyEvent e) {
        if (e.keyCode == SWT.CR) {
            KeyMeta keyMeta = (KeyMeta) e.widget.getData();
            presenter.setValue(keyMeta, ((Text) e.widget).getText());
        }
    }

    @Override
    public void keyReleased(KeyEvent e) {
    }

    @Override
    public void focusGained(FocusEvent e) {
    }

    @Override
    public void focusLost(FocusEvent e) {
        KeyMeta keyMeta = (KeyMeta) e.widget.getData();
        presenter.setValue(keyMeta, ((Text) e.widget).getText());
    }

    @Override
    public void setValue(KeyMeta keyMeta, String value) {
        if (value == null)
            value = keyMeta.getDefaultValue();

        if (value == null)
            value = "";

        Widget w = metaToWidget.get(keyMeta);
        switch (keyMeta.getType()) {
        case BOOLEAN:
            ((Button) w).setSelection(value.equals("1"));
            break;
        default:
            ((Text) w).setText(value);
        }
    }

    @Override
    public void setMessage(IStatus status) {
        if (status != null) {
            int type = IMessageProvider.ERROR;
            if (status.getSeverity() == IStatus.WARNING)
                type = IMessageProvider.WARNING;
            else if (status.getSeverity() == IStatus.INFO)
                type = IMessageProvider.INFORMATION;
            else if (status.getSeverity() == IStatus.OK)
                type = IMessageProvider.NONE;

            status.getSeverity();
            this.form.setMessage(status.getMessage(), type);
        } else {
            this.form.setMessage(null, IMessageProvider.NONE);
        }
    }

}
