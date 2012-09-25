package com.dynamo.cr.properties.descriptors;

import java.util.Arrays;

import org.eclipse.core.commands.operations.IUndoableOperation;
import org.eclipse.core.resources.IContainer;
import org.eclipse.jface.resource.ColorRegistry;
import org.eclipse.jface.resource.JFaceResources;
import org.eclipse.swt.SWT;
import org.eclipse.swt.events.SelectionEvent;
import org.eclipse.swt.events.SelectionListener;
import org.eclipse.swt.graphics.Color;
import org.eclipse.swt.graphics.RGB;
import org.eclipse.swt.layout.GridData;
import org.eclipse.swt.layout.GridLayout;
import org.eclipse.swt.widgets.Composite;
import org.eclipse.swt.widgets.Control;
import org.eclipse.swt.widgets.Display;
import org.eclipse.swt.widgets.Event;
import org.eclipse.swt.widgets.Listener;
import org.eclipse.swt.widgets.Menu;
import org.eclipse.swt.widgets.MenuItem;
import org.eclipse.swt.widgets.Text;
import org.eclipse.ui.forms.widgets.FormToolkit;

import com.dynamo.cr.properties.IPropertyEditor;
import com.dynamo.cr.properties.IPropertyModel;
import com.dynamo.cr.properties.IPropertyObjectWorld;
import com.dynamo.cr.properties.PropertyDesc;
import com.dynamo.cr.properties.PropertyUtil;
import com.dynamo.cr.properties.types.ValueSpread;

public class ValueSpreadPropertyDesc<T, U extends IPropertyObjectWorld> extends PropertyDesc<T, U> {

    public static final String ANIMATED_COLOR_KEY = "com.dynamo.cr.properties.ANIMATED_COLOR";

    public ValueSpreadPropertyDesc(String id, String name) {
        super(id, name);

        ColorRegistry r = JFaceResources.getColorRegistry();

        if (!r.hasValueFor(ANIMATED_COLOR_KEY))
            r.put(ANIMATED_COLOR_KEY, new RGB(214, 230, 214));
    }

    @Override
    public IPropertyEditor<T, U> createEditor(FormToolkit toolkit,
            Composite parent, IContainer contentRoot) {
        return (IPropertyEditor<T, U>) new Editor<T, U>(parent, this);
    }

    /*
     * NOTE: This class is quite similar to ArrayPropertyEditor. We should perhaps factor out common functionality?
     * It's also a bit messy..
     */
    static class Editor<T, U extends IPropertyObjectWorld> implements IPropertyEditor<T, U>, Listener, SelectionListener {

        private Text[] textFields;
        private Composite composite;
        private IPropertyModel<T, U>[] models;
        private String[] oldValue;
        private Menu menu;
        private ValueSpreadPropertyDesc<T, U> propertyDesc;
        private MenuItem animated;
        public Editor(Composite parent, ValueSpreadPropertyDesc<T, U> propertyDesc ) {
            this.propertyDesc = propertyDesc;
            composite = new Composite(parent, SWT.NONE);
            composite.setBackground(parent.getBackground());
            GridLayout layout = new GridLayout(2, false);
            layout.marginWidth = 0;
            layout.marginHeight = 0;
            composite.setLayout(layout);
            GridData gd = new GridData();
            gd.widthHint = 54;

            menu = new Menu(parent);
            animated = new MenuItem(menu, SWT.CHECK);
            animated.setText("Animated");
            animated.addSelectionListener(this);

            textFields = new Text[2];
            for (int i = 0; i < 2; i++) {
                Text t = createText(composite);
                t.setMenu(menu);
                t.setLayoutData(gd);
                textFields[i] = t;
            }
            oldValue = new String[2];
        }

        protected void fillMenu(Menu menu) {
        }

        @Override
        public void dispose() {
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

        protected Color getColor(IPropertyModel<T, U>[] models) {
            ValueSpread vs = (ValueSpread) models[0].getPropertyValue(propertyDesc.getId());
            if (models.length == 1 && vs.isAnimated()) {
                return JFaceResources.getColorRegistry().get(ANIMATED_COLOR_KEY);
            } else {
                return Display.getDefault().getSystemColor(SWT.COLOR_WHITE);
            }
        }

        @Override
        public void refresh() {
            boolean editable = models[0].isPropertyEditable(propertyDesc.getId());
            getControl().setEnabled(editable);

            boolean[] equal = new boolean[] { true, true };
            ValueSpread firstValue = (ValueSpread) models[0].getPropertyValue(propertyDesc.getId());
            for (int i = 1; i < models.length; ++i) {
                ValueSpread value = (ValueSpread) models[i].getPropertyValue(propertyDesc.getId());
                equal[0] = firstValue.getValue() == value.getValue();
                equal[1] = firstValue.getSpread() == value.getSpread();
            }

            if (models.length == 1) {
                // No multi-selection support for the animated property
                // This is due to static merging of data in BeanPropertyAccessor, see merge*
                animated.setEnabled(true);
                animated.setSelection(firstValue.isAnimated());
            } else {
                animated.setEnabled(false);
                animated.setSelection(false);
            }

            double[] array = new double[] { firstValue.getValue(), firstValue.getSpread() };
            Color color = getColor(models);

            for (int i = 0; i < 2; ++i) {
                String s;
                if (equal[i]) {
                    s = Double.toString(array[i]);
                } else {
                    s = "";
                }
                textFields[i].setText(s);
                textFields[i].setBackground(color);
                oldValue[i] = s;
            }
        }

        @Override
        public void setModels(IPropertyModel<T, U>[] models) {
            this.models = models;
        }

        @Override
        public void handleEvent(Event event) {

            double[] newValue = new double[2];
            String[] newStringValue = new String[2];
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
                Double[] diff = new Double[2];

                for (int i = 0; i < diff.length; i++) {
                    if (event.widget == this.textFields[i]) {
                        diff[i] = newValue[i];
                    }
                }

                IUndoableOperation combinedOperation = PropertyUtil.setProperty(models, propertyDesc.getId(), diff);
                if (combinedOperation != null)
                    models[0].getCommandFactory().execute(combinedOperation, models[0].getWorld());
            }
            oldValue = newStringValue;
        }

        @Override
        public void widgetSelected(SelectionEvent e) {
            ValueSpread value = (ValueSpread) models[0].getPropertyValue(propertyDesc.getId());
            value.setAnimated(animated.getSelection());
            IUndoableOperation combinedOperation = PropertyUtil.setProperty(models, propertyDesc.getId(), value);
            if (combinedOperation != null)
                models[0].getCommandFactory().execute(combinedOperation, models[0].getWorld());

        }

        @Override
        public void widgetDefaultSelected(SelectionEvent e) {}
    }

}
