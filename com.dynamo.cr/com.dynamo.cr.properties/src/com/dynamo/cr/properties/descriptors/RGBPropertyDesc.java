package com.dynamo.cr.properties.descriptors;

import org.eclipse.core.commands.operations.IUndoableOperation;
import org.eclipse.core.resources.IContainer;
import org.eclipse.swt.SWT;
import org.eclipse.swt.graphics.FontMetrics;
import org.eclipse.swt.graphics.GC;
import org.eclipse.swt.graphics.Image;
import org.eclipse.swt.graphics.ImageData;
import org.eclipse.swt.graphics.PaletteData;
import org.eclipse.swt.graphics.RGB;
import org.eclipse.swt.layout.GridData;
import org.eclipse.swt.layout.GridLayout;
import org.eclipse.swt.widgets.Button;
import org.eclipse.swt.widgets.ColorDialog;
import org.eclipse.swt.widgets.Composite;
import org.eclipse.swt.widgets.Control;
import org.eclipse.swt.widgets.Event;
import org.eclipse.swt.widgets.Label;
import org.eclipse.swt.widgets.Listener;
import org.eclipse.ui.forms.widgets.FormToolkit;

import com.dynamo.cr.properties.IPropertyEditor;
import com.dynamo.cr.properties.IPropertyModel;
import com.dynamo.cr.properties.IPropertyObjectWorld;
import com.dynamo.cr.properties.PropertyDesc;
import com.dynamo.cr.properties.PropertyUtil;

public class RGBPropertyDesc<T, U extends IPropertyObjectWorld> extends PropertyDesc<T, U>  {

    public RGBPropertyDesc(String id, String name, String catgory) {
        super(id, name, catgory);
    }

    private class Editor implements IPropertyEditor<T, U>, Listener {

        private IPropertyModel<T, U>[] models;
        private Button selectionButton;
        private RGB oldValue;
        private Image image;
        private Composite composite;
        private Label label;
        Editor(Composite parent) {
            composite = new Composite(parent, SWT.NONE);
            composite.setBackground(parent.getBackground());
            GridLayout layout = new GridLayout(2, false);
            layout.marginWidth = 0;
            layout.marginHeight = 0;
            layout.horizontalSpacing = 0;
            composite.setLayout(layout);

            selectionButton = new Button(composite, SWT.PUSH);
            selectionButton.setFont(parent.getFont());
            selectionButton.addListener(SWT.Selection, this);
            selectionButton.setLayoutData(new GridData());

            label = new Label(composite, SWT.NONE);
            label.setLayoutData(new GridData(GridData.FILL_HORIZONTAL));
        }

        @Override
        public void dispose() {
            if (image != null) {
                image.dispose();
            }
        }

        @Override
        public Control getControl() {
            return composite;
        }

        private ImageData createColorImage(Control w, RGB color) {

            GC gc = new GC(w);
            FontMetrics fm = gc.getFontMetrics();
            int size = fm.getAscent();
            gc.dispose();

            int extent = w.getSize().y - 4;
            extent = Math.max(extent, 16);

            if (size > extent) {
                size = extent;
            }

            int width = size;
            int height = extent;

            int xoffset = 0;
            int yoffset = (height - size) / 2;

            RGB black = new RGB(0, 0, 0);
            PaletteData dataPalette = new PaletteData(new RGB[] { black, black,
                    color });
            ImageData data = new ImageData(width, height, 4, dataPalette);
            data.transparentPixel = 0;

            int end = size - 1;
            for (int y = 0; y < size; y++) {
                for (int x = 0; x < size; x++) {
                    if (x == 0 || y == 0 || x == end || y == end) {
                        data.setPixel(x + xoffset, y + yoffset, 1);
                    } else {
                        data.setPixel(x + xoffset, y + yoffset, 2);
                    }
                }
            }

            return data;
        }

        private void setButtonColor(RGB value) {
            if (value.equals(oldValue))
                return;

            if (image != null) {
                image.dispose();
            }

            ImageData id = createColorImage(selectionButton, value);
            ImageData mask = id.getTransparencyMask();
            image = new Image(selectionButton.getDisplay(), id, mask);
            selectionButton.setImage(image);
        }

        @Override
        public void refresh() {
            boolean editable = models[0].isPropertyEditable(getId());
            getControl().setEnabled(editable);

            oldValue = null;
            RGB firstValue = (RGB) models[0].getPropertyValue(getId());
            for (int i = 1; i < models.length; ++i) {
                RGB value = (RGB) models[i].getPropertyValue(getId());
                if (!firstValue.equals(value)) {
                    setButtonColor(new RGB(128, 128, 128));
                    label.setText("(-, -, -)");
                    return;
                }
            }
            setButtonColor(firstValue);
            label.setText(String.format("(%d, %d, %d)", firstValue.red, firstValue.green, firstValue.blue));
            oldValue = firstValue;
        }

        @Override
        public void setModels(IPropertyModel<T, U>[] models) {
            this.models = models;
        }

        @Override
        public void handleEvent(Event event) {

            ColorDialog dialog = new ColorDialog(selectionButton.getShell());
            if (oldValue != null) {
                dialog.setRGB(oldValue);
            }
            RGB value = dialog.open();
            if (value != null) {
                IUndoableOperation combinedOperation = PropertyUtil.setProperty(models, getId(), value);
                if (combinedOperation != null)
                    models[0].getCommandFactory().execute(combinedOperation, models[0].getWorld());

                oldValue = value;
            }
        }
    }

    @Override
    public IPropertyEditor<T, U> createEditor(FormToolkit toolkit, Composite parent, IContainer contentRoot) {
        return new Editor(parent);
    }

}
