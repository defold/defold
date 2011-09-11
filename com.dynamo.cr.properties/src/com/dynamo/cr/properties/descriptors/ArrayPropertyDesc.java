package com.dynamo.cr.properties.descriptors;


import java.util.Arrays;

import org.eclipse.core.commands.operations.IUndoableOperation;
import org.eclipse.core.resources.IContainer;
import org.eclipse.swt.SWT;
import org.eclipse.swt.layout.GridData;
import org.eclipse.swt.layout.GridLayout;
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

public abstract class ArrayPropertyDesc<V, T, U extends IPropertyObjectWorld> extends PropertyDesc<T, U>  {

    private int count;

    public ArrayPropertyDesc(Object id, String name, int count) {
        super(id, name);
        this.count = count;
    }

    protected abstract double[] valueToArray(V value);

    protected abstract boolean isComponentEqual(V v1, V v2, int component);

    private class Editor implements IPropertyEditor<T, U>, Listener {

        private Text[] textFields;
        private Composite composite;
        private IPropertyModel<T, U>[] models;
        private String[] oldValue;
        Editor(Composite parent) {
            composite = new Composite(parent, SWT.NONE);
            composite.setBackground(parent.getBackground());
            GridLayout layout = new GridLayout(count, false);
            layout.marginWidth = 0;
            composite.setLayout(layout);
            GridData gd = new GridData();
            gd.widthHint = 54;

            textFields = new Text[count];
            for (int i = 0; i < count; i++) {
                Text t = createText(composite);
                t.setLayoutData(gd);
                textFields[i] = t;
            }
            oldValue = new String[count];
        }

        Text createText(Composite parent) {
            Text text = new Text(parent, SWT.BORDER);
            text.addListener(SWT.KeyDown, this);
            text.addListener(SWT.FocusOut, this);
            return text;
        }

        @Override
        public Control getControl() {
            return composite;
        }

        @SuppressWarnings("unchecked")
        @Override
        public void refresh() {

            boolean[] equal = new boolean[count];
            for (int i = 0; i < count; ++i) {
                equal[i] = true;
            }

            V firstValue = (V) models[0].getPropertyValue(getId());
            for (int i = 1; i < models.length; ++i) {
                V value = (V) models[i].getPropertyValue(getId());
                for (int j = 0; j < count; ++j) {
                    if (!isComponentEqual(firstValue, value, j)) {
                        equal[j] = false;
                    }
                }
            }

            double[] array = valueToArray(firstValue);

            for (int i = 0; i < count; ++i) {
                String s;
                if (equal[i]) {
                    s = Double.toString(array[i]);
                } else {
                    s = "";
                }
                textFields[i].setText(s);
                oldValue[i] = s;
            }
        }

        @Override
        public void setModels(IPropertyModel<T, U>[] models) {
            this.models = models;
        }

        @Override
        public void handleEvent(Event event) {

            double[] newValue = new double[count];
            String[] newStringValue = new String[count];
            try {
                for (int i = 0; i < newValue.length; i++) {
                    String s = this.textFields[i].getText();
                    newStringValue[i] = s;
                    // NOTE: Treat "" as 0
                    if (s.length() != 0) {
                        newValue[i] = Double.parseDouble(s);
                    }
                }
            } catch (NumberFormatException e) {
                return;
            }

            boolean updateValue = false;
            if (event.type == SWT.KeyDown && (event.character == '\r' || event.character == '\n')) {
                updateValue = true;
            } else if (event.type == SWT.FocusOut && !Arrays.equals(newStringValue, oldValue)) {
                updateValue = true;
            }
            if (updateValue) {
                Double[] diff = new Double[count];

                for (int i = 0; i < diff.length; i++) {
                    if (event.widget == this.textFields[i]) {
                        diff[i] = newValue[i];
                    }
                }

                IUndoableOperation combinedOperation = PropertyUtil.setProperty(models, getId(), diff);
                if (combinedOperation != null)
                    models[0].getCommandFactory().execute(combinedOperation, models[0].getWorld());
            }
            oldValue = newStringValue;
        }
    }

    @Override
    public IPropertyEditor<T, U> createEditor(Composite parent, IContainer contentRoot) {
        return new Editor(parent);
    }

}
