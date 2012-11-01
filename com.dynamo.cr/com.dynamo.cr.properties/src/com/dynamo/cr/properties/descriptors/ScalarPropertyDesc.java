package com.dynamo.cr.properties.descriptors;

import java.util.ArrayList;
import java.util.List;

import org.eclipse.core.commands.operations.IUndoableOperation;
import org.eclipse.core.resources.IContainer;
import org.eclipse.jface.resource.ColorRegistry;
import org.eclipse.jface.resource.JFaceResources;
import org.eclipse.swt.SWT;
import org.eclipse.swt.graphics.RGB;
import org.eclipse.swt.widgets.Combo;
import org.eclipse.swt.widgets.Composite;
import org.eclipse.swt.widgets.Control;
import org.eclipse.swt.widgets.Display;
import org.eclipse.swt.widgets.Event;
import org.eclipse.swt.widgets.Listener;
import org.eclipse.swt.widgets.Text;
import org.eclipse.ui.forms.widgets.FormToolkit;

import com.dynamo.cr.editor.core.operations.IMergeableOperation;
import com.dynamo.cr.editor.core.operations.IMergeableOperation.Type;
import com.dynamo.cr.properties.IPropertyEditor;
import com.dynamo.cr.properties.IPropertyModel;
import com.dynamo.cr.properties.IPropertyObjectWorld;
import com.dynamo.cr.properties.Property.EditorType;
import com.dynamo.cr.properties.PropertyDesc;
import com.dynamo.cr.properties.PropertyUtil;

public abstract class ScalarPropertyDesc<S, T, U extends IPropertyObjectWorld> extends PropertyDesc<T, U>  {

    private EditorType editorType;

    public static final String BACKGROUND_COLOR_KEY = "com.dynamo.cr.properties.BACKGROUND_COLOR";
    public static final String OVERRIDDEN_COLOR_KEY = "com.dynamo.cr.properties.OVERRIDDEN_COLOR";

    public ScalarPropertyDesc(String id, String name, String catgory, EditorType editorType) {
        super(id, name, catgory);
        this.editorType = editorType;

        // NOTE: A temporary solution in order to avoid memory leaks (Color)
        // A ColorRegistry should probably be passed in the constructor or similar for theming support
        if (Display.getCurrent() != null) {
            // Only create colors if a display is present (i.e. not in unit-test)
            ColorRegistry r = JFaceResources.getColorRegistry();

            if (!r.hasValueFor(BACKGROUND_COLOR_KEY))
                r.put(BACKGROUND_COLOR_KEY, new RGB(255, 255, 255));

            if (!r.hasValueFor(OVERRIDDEN_COLOR_KEY))
                r.put(OVERRIDDEN_COLOR_KEY, new RGB(214, 230, 255));
        }
    }

    public abstract S fromString(String text);
    public abstract String scalarToString(S value);
    public abstract Class<?> getTypeClass();

    /**
     * Widet abstraction classs for Text/Combo-box
     * @author chmu
     */
    private class EditorWidget {
        private Text text;
        private SpinnerText spinnerText;
        private Combo combo;
        private Control control;

        public EditorWidget(Composite parent) {
            if (editorType == EditorType.DEFAULT) {
                Class<?> typeClass = getTypeClass();
                if (Number.class.isAssignableFrom(typeClass)) {
                    boolean floatOrDouble = typeClass == Float.class || typeClass == Double.class;
                    control = spinnerText = new SpinnerText(parent, SWT.BORDER, !floatOrDouble);
                    spinnerText.setMin(getMin());
                    spinnerText.setMax(getMax());
                    text = spinnerText.getText();
                } else {
                    control = text = new Text(parent, SWT.BORDER);
                }
            } else {
                control = combo = new Combo(parent, SWT.DROP_DOWN);
            }
        }

        private void setText(String text) {
            if (editorType == EditorType.DEFAULT)
                this.text.setText(text);
            else
                this.combo.setText(text);
        }

        private String getText() {
            if (editorType == EditorType.DEFAULT)
                return this.text.getText();
            else
                return this.combo.getText();
        }

        public Control getControl() {
            return control;
        }

        private void updateOptions(IPropertyModel<T, U>[] models) {
            if (editorType == EditorType.DROP_DOWN) {
                if (models.length == 1) {
                    /*
                     * When only set options for single selections in order
                     * to avoid potential performance problems
                     */
                    List<String> lst = new ArrayList<String>();
                    for (Object o : models[0].getPropertyOptions(getId())) {
                        lst.add(o.toString());
                    }
                    combo.setItems(lst.toArray(new String[lst.size()]));
                } else {
                    combo.setItems(new String[] {});
                }
            }
        }

        public void addListener(int eventType, Listener listener) {
            if (editorType == EditorType.DEFAULT) {
                text.addListener(eventType, listener);
            } else {
                combo.addListener(eventType, listener);
            }
        }
    }

    private class Editor implements IPropertyEditor<T, U>, Listener {

        private String oldValue;
        private IPropertyModel<T, U>[] models;
        private EditorWidget widget;
        Editor(Composite parent) {
            widget = new EditorWidget(parent);
            widget.addListener(SWT.KeyDown, this);
            widget.addListener(SWT.FocusOut, this);
            widget.addListener(SWT.DefaultSelection, this);
        }

        @Override
        public void dispose() {
        }

        @Override
        public Control getControl() {
            return widget.getControl();
        }

        @SuppressWarnings("unchecked")
        @Override
        public void refresh() {
            widget.updateOptions(models);
            boolean editable = models[0].isPropertyEditable(getId());
            getControl().setEnabled(editable);
            S firstValue = (S) models[0].getPropertyValue(getId());
            boolean overridden = models[0].isPropertyOverridden(getId());

            ColorRegistry colorRegistry = JFaceResources.getColorRegistry();

            for (int i = 1; i < models.length; ++i) {
                if (models[i].isPropertyOverridden(getId())) {
                    overridden = true;
                }
                S value = (S) models[i].getPropertyValue(getId());
                if (!firstValue.equals(value)) {
                    widget.setText("");
                    widget.getControl().setBackground(colorRegistry.get(BACKGROUND_COLOR_KEY));
                    oldValue = "";
                    return;
                }
            }
            widget.setText(scalarToString(firstValue));
            if (overridden) {
                widget.getControl().setBackground(colorRegistry.get(OVERRIDDEN_COLOR_KEY));
            } else {
                widget.getControl().setBackground(colorRegistry.get(BACKGROUND_COLOR_KEY));
            }
            oldValue = scalarToString(firstValue);
        }

        @Override
        public void setModels(IPropertyModel<T, U>[] models) {
            this.models = models;
        }

        @Override
        public void handleEvent(Event event) {
            S value = fromString(widget.getText());
            if (value == null)
                value = fromString("0");

            boolean updateValue = false;
            IMergeableOperation.Type type = Type.OPEN;
            if (event.type == SWT.KeyDown && (event.character == '\r' || event.character == '\n')) {
                updateValue = true;
            } else if (event.type == SWT.FocusOut && !value.equals(oldValue)) {
                updateValue = true;
            } else if (event.type == SWT.DefaultSelection && !value.equals(oldValue)) {
                updateValue = true;
                type = Type.INTERMEDIATE;
            }

            if (updateValue) {
                IUndoableOperation combinedOperation = PropertyUtil.setProperty(models, getId(), value, type);
                if (combinedOperation != null)
                    models[0].getCommandFactory().execute(combinedOperation, models[0].getWorld());

                oldValue = value.toString();
            }
        }
    }

    @Override
    public IPropertyEditor<T, U> createEditor(FormToolkit toolkit, Composite parent, IContainer contentRoot) {
        return new Editor(parent);
    }

}
