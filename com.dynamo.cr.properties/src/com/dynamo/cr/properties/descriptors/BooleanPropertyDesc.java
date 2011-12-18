package com.dynamo.cr.properties.descriptors;

import org.eclipse.core.commands.operations.IUndoableOperation;
import org.eclipse.core.resources.IContainer;
import org.eclipse.swt.SWT;
import org.eclipse.swt.widgets.Button;
import org.eclipse.swt.widgets.Composite;
import org.eclipse.swt.widgets.Control;
import org.eclipse.swt.widgets.Event;
import org.eclipse.swt.widgets.Listener;

import com.dynamo.cr.properties.IPropertyEditor;
import com.dynamo.cr.properties.IPropertyModel;
import com.dynamo.cr.properties.IPropertyObjectWorld;
import com.dynamo.cr.properties.PropertyDesc;
import com.dynamo.cr.properties.PropertyUtil;

public class BooleanPropertyDesc<T, U extends IPropertyObjectWorld> extends PropertyDesc<T, U>  {

    public BooleanPropertyDesc(String id, String name) {
        super(id, name);
    }

    private class Editor implements IPropertyEditor<T, U>, Listener {

        private Button check;
        private IPropertyModel<T, U>[] models;
        Editor(Composite parent) {
            check = new Button(parent, SWT.CHECK);
            check.addListener(SWT.Selection, this);
        }

        @Override
        public void dispose() {
        }

        @Override
        public Control getControl() {
            return check;
        }

        @Override
        public void refresh() {
            Boolean firstValue = (Boolean) models[0].getPropertyValue(getId());
            for (int i = 1; i < models.length; ++i) {
                Boolean value = (Boolean) models[i].getPropertyValue(getId());
                if (!firstValue.equals(value)) {
                    check.setSelection(false);
                    return;
                }
            }
            check.setSelection(firstValue);
        }

        @Override
        public void setModels(IPropertyModel<T, U>[] models) {
            this.models = models;
        }

        @Override
        public void handleEvent(Event event) {
            boolean value = check.getSelection();

            IUndoableOperation combinedOperation = PropertyUtil.setProperty(models, getId(), value);
            if (combinedOperation != null)
                models[0].getCommandFactory().execute(combinedOperation, models[0].getWorld());

        }
    }

    @Override
    public IPropertyEditor<T, U> createEditor(Composite parent, IContainer contentRoot) {
        return new Editor(parent);
    }

}
