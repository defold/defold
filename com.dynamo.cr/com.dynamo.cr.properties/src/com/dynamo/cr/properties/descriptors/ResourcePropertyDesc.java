package com.dynamo.cr.properties.descriptors;

import org.eclipse.core.commands.operations.IUndoableOperation;
import org.eclipse.core.resources.IContainer;
import org.eclipse.core.resources.IResource;
import org.eclipse.swt.SWT;
import org.eclipse.swt.layout.GridData;
import org.eclipse.swt.layout.GridLayout;
import org.eclipse.swt.widgets.Button;
import org.eclipse.swt.widgets.Composite;
import org.eclipse.swt.widgets.Control;
import org.eclipse.swt.widgets.Event;
import org.eclipse.swt.widgets.Listener;
import org.eclipse.swt.widgets.Text;
import org.eclipse.ui.dialogs.ListDialog;
import org.eclipse.ui.dialogs.ResourceListSelectionDialog;
import org.eclipse.ui.forms.widgets.FormToolkit;

import com.dynamo.cr.editor.core.EditorUtil;
import com.dynamo.cr.editor.ui.FilteredResourceListSelectionDialog;
import com.dynamo.cr.properties.IPropertyEditor;
import com.dynamo.cr.properties.IPropertyModel;
import com.dynamo.cr.properties.IPropertyObjectWorld;
import com.dynamo.cr.properties.PropertyDesc;
import com.dynamo.cr.properties.PropertyUtil;

public class ResourcePropertyDesc<T, U extends IPropertyObjectWorld> extends PropertyDesc<T, U>  {

    private IContainer contentRoot;
    String[] extensions;

    public ResourcePropertyDesc(String id, String name, String catgory, String[] extensions) {
        super(id, name, catgory);
        this.extensions = extensions;
    }

    private class Editor implements IPropertyEditor<T, U>, Listener {

        private Text text;
        private String oldValue;
        private IPropertyModel<T, U>[] models;
        private Button selectionButton;
        private Composite composite;
        Editor(Composite parent) {
            composite = new Composite(parent, SWT.NONE);
            composite.setLayoutData(new GridData(GridData.FILL_HORIZONTAL));
            GridLayout layout = new GridLayout(2, false);
            layout.marginWidth = 0;
            layout.marginHeight = 0;
            layout.horizontalSpacing = 0;
            layout.verticalSpacing = 0;
            composite.setLayout(layout);
            composite.setBackground(parent.getBackground());

            text = new Text(composite, SWT.BORDER);
            text.setLayoutData(new GridData(GridData.FILL_HORIZONTAL));
            selectionButton = new Button(composite, SWT.PUSH);
            selectionButton.setText("...");

            text.addListener(SWT.KeyDown, this);
            text.addListener(SWT.FocusOut, this);
            selectionButton.addListener(SWT.Selection, this);
        }

        @Override
        public void dispose() {
        }

        @Override
        public Control getControl() {
            return composite;
        }

        @Override
        public void refresh() {
            boolean editable = models[0].isPropertyEditable(getId());
            getControl().setEnabled(editable);

            String firstValue = (String) models[0].getPropertyValue(getId());
            for (int i = 1; i < models.length; ++i) {
                String value = (String) models[i].getPropertyValue(getId());
                if (!firstValue.equals(value)) {
                    text.setText("");
                    oldValue = "";
                    return;
                }
            }
            text.setText(firstValue.toString());
            oldValue = firstValue.toString();
        }

        @Override
        public void setModels(IPropertyModel<T, U>[] models) {
            this.models = models;
        }

        @Override
        public void handleEvent(Event event) {
            String value = text.getText();
            if (value == null)
                return;

            boolean updateValue = false;
            if (event.type == SWT.KeyDown && (event.character == '\r' || event.character == '\n')) {
                updateValue = true;
            } else if (event.type == SWT.FocusOut && !value.equals(oldValue)) {
                updateValue = true;
            } else if (event.type == SWT.Selection) {
                FilteredResourceListSelectionDialog dialog = new FilteredResourceListSelectionDialog(
                        getControl().getShell(), contentRoot, IResource.FILE, extensions);

                int ret = dialog.open();
                if (ret == ListDialog.OK) {
                    IResource r = (IResource) dialog.getResult()[0];
                    String resource = EditorUtil.makeResourcePath(r);
                    text.setText(resource);
                    value = resource;
                    updateValue = true;
                }
            }
            if (updateValue) {
                IUndoableOperation combinedOperation = PropertyUtil.setProperty(models, getId(), value);
                if (combinedOperation != null)
                    models[0].getCommandFactory().execute(combinedOperation, models[0].getWorld());

                oldValue = value.toString();
            }
        }

    }

    @Override
    public IPropertyEditor<T, U> createEditor(FormToolkit toolkit, Composite parent, IContainer contentRoot) {
        this.contentRoot = contentRoot;
        return new Editor(parent);
    }

}
