package com.dynamo.cr.properties.descriptors;

import org.eclipse.core.commands.operations.IUndoableOperation;
import org.eclipse.swt.SWT;
import org.eclipse.swt.widgets.Composite;
import org.eclipse.swt.widgets.Control;
import org.eclipse.swt.widgets.Event;
import org.eclipse.swt.widgets.Listener;
import org.eclipse.swt.widgets.Text;

import com.dynamo.cr.properties.IPropertyEditor;
import com.dynamo.cr.properties.IPropertyModel;
import com.dynamo.cr.properties.IPropertyObjectWorld;
import com.dynamo.cr.properties.PropertyDesc;
import com.dynamo.cr.properties.PropertyUtil;

public abstract class ScalarPropertyDesc<S, T, U extends IPropertyObjectWorld> extends PropertyDesc<T, U>  {

    public ScalarPropertyDesc(Object id, String name) {
        super(id, name);
    }

    public abstract S fromString(String text);

    private class Editor implements IPropertyEditor<T, U>, Listener {

        private Text text;
        private String oldValue;
        private IPropertyModel<T, U>[] models;
        Editor(Composite parent) {
            text = new Text(parent, SWT.BORDER);
            text.addListener(SWT.KeyDown, this);
            text.addListener(SWT.FocusOut, this);
        }

        @Override
        public Control getControl() {
            return text;
        }

        @SuppressWarnings("unchecked")
        @Override
        public void refresh() {
            S firstValue = (S) models[0].getPropertyValue(getId());
            for (int i = 1; i < models.length; ++i) {
                S value = (S) models[i].getPropertyValue(getId());
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
            S value = fromString(text.getText());
            if (value == null)
                return;

            boolean updateValue = false;
            if (event.type == SWT.KeyDown && (event.character == '\r' || event.character == '\n')) {
                updateValue = true;
            } else if (event.type == SWT.FocusOut && !value.equals(oldValue)) {
                updateValue = true;
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
    public IPropertyEditor<T, U> createEditor(Composite parent) {
        return new Editor(parent);
    }

}
