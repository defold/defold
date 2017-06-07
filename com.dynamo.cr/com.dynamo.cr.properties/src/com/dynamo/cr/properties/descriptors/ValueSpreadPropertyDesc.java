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
import org.eclipse.ui.forms.widgets.FormToolkit;

import com.dynamo.cr.editor.core.operations.IMergeableOperation;
import com.dynamo.cr.editor.core.operations.IMergeableOperation.Type;
import com.dynamo.cr.properties.IPropertyEditor;
import com.dynamo.cr.properties.IPropertyModel;
import com.dynamo.cr.properties.IPropertyObjectWorld;
import com.dynamo.cr.properties.PropertyDesc;
import com.dynamo.cr.properties.PropertyUtil;
import com.dynamo.cr.properties.types.ValueSpread;
import com.dynamo.cr.properties.util.NumberUtil;

public class ValueSpreadPropertyDesc<T, U extends IPropertyObjectWorld> extends PropertyDesc<T, U> {

    public static final String ANIMATED_COLOR_KEY = "com.dynamo.cr.properties.ANIMATED_COLOR";

    public ValueSpreadPropertyDesc(String id, String name, String catgory) {
        super(id, name, catgory);

        if (Display.getCurrent() != null) {
            ColorRegistry r = JFaceResources.getColorRegistry();
            if (!r.hasValueFor(ANIMATED_COLOR_KEY))
                r.put(ANIMATED_COLOR_KEY, new RGB(214, 230, 214));
        }
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

        private SpinnerText[] textFields;
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
            animated.setText("Curve");
            animated.addSelectionListener(this);

            textFields = new SpinnerText[2];
            for (int i = 0; i < 2; i++) {
                SpinnerText t = createText(composite);
                t.setLayoutData(gd);
                textFields[i] = t;
            }
            textFields[0].setMin(propertyDesc.getMin());
            textFields[0].setMax(propertyDesc.getMax());
            textFields[1].setMin(0);

            oldValue = new String[2];
        }

        protected void fillMenu(Menu menu) {
        }

        @Override
        public void dispose() {
        }

        SpinnerText createText(Composite parent) {
            SpinnerText text = new SpinnerText(parent, SWT.BORDER, false);
            text.getText().addListener(SWT.KeyDown, this);
            text.getText().addListener(SWT.FocusOut, this);
            text.getText().addListener(SWT.DefaultSelection, this);
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

        protected boolean isAnyAnimated(IPropertyModel<T, U>[] models) {
            for (IPropertyModel<T, U> m : models) {
                ValueSpread vs = (ValueSpread) m.getPropertyValue(propertyDesc.getId());
                if (vs.isAnimated()) {
                    return true;
                }
            }
            return false;
        }

        @Override
        public void refresh() {
            boolean editable = models[0].isPropertyEditable(propertyDesc.getId());
            textFields[0].getText().setEnabled(editable);
            textFields[1].getText().setEnabled(editable);

            boolean[] equal = new boolean[] { true, true };
            ValueSpread firstValue = (ValueSpread) models[0].getPropertyValue(propertyDesc.getId());
            textFields[1].getText().setVisible(!firstValue.hideSpread());
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

            // Add the Curve menu if this spread value is curvable
            if (firstValue.isCurvable()) {
            	textFields[0].getText().setMenu(menu);
            }

            double[] array = new double[] { firstValue.getValue(), firstValue.getSpread() };
            Color color = getColor(models);

            for (int i = 0; i < 2; ++i) {
                String s;
                if (equal[i]) {
                    s = NumberUtil.formatDouble(array[i]);
                } else {
                    s = "";
                }
                textFields[i].getText().setText(s);
                oldValue[i] = s;
            }

            textFields[0].getText().setBackground(color);
            boolean animated = isAnyAnimated(models);
            textFields[0].getText().setEditable(!animated);
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
                    String s = this.textFields[i].getText().getText();
                    newStringValue[i] = s;
                    // NOTE: Treat "" as 0
                    if (s.length() != 0) {
                        newValue[i] = NumberUtil.parseDouble(s);
                    }
                }
            } catch (NumberFormatException e) {
                return;
            }

            if ((event.type == SWT.KeyDown || event.type == SWT.KeyUp) && (event.stateMask & SWT.MOD1) == SWT.MOD1 && event.character == 'z') {
                event.doit = false;
                return;
            }

            boolean updateValue = false;
            IMergeableOperation.Type type = Type.OPEN;
            if (event.type == SWT.KeyDown && (event.character == '\r' || event.character == '\n')) {
                updateValue = true;
            } else if (event.type == SWT.FocusOut && !Arrays.equals(newStringValue, oldValue)) {
                updateValue = true;
            } else if (event.type == SWT.DefaultSelection) {
                updateValue = true;
                type = Type.INTERMEDIATE;
            }

            if (updateValue) {
                Double[] diff = new Double[2];

                for (int i = 0; i < diff.length; i++) {
                    if (event.widget == this.textFields[i].getText()) {
                        diff[i] = newValue[i];
                    }
                }

                IUndoableOperation combinedOperation = PropertyUtil.setProperty(models, propertyDesc.getId(), diff, type);
                if (combinedOperation != null)
                    models[0].getCommandFactory().execute(combinedOperation, models[0].getWorld());
            }
            oldValue = newStringValue;
        }

        @Override
        public void widgetSelected(SelectionEvent e) {
            if (e.widget == this.animated) {
                ValueSpread value = (ValueSpread) models[0].getPropertyValue(propertyDesc.getId());
                value.setAnimated(animated.getSelection());
                IUndoableOperation combinedOperation = PropertyUtil.setProperty(models, propertyDesc.getId(), value);
                if (combinedOperation != null)
                    models[0].getCommandFactory().execute(combinedOperation, models[0].getWorld());
            }
        }

        @Override
        public void widgetDefaultSelected(SelectionEvent e) {  }
    }

}
