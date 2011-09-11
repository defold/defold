package com.dynamo.cr.properties;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.Map;

import org.eclipse.core.resources.IContainer;
import org.eclipse.core.runtime.IAdaptable;
import org.eclipse.jface.viewers.ISelection;
import org.eclipse.jface.viewers.Viewer;
import org.eclipse.swt.SWT;
import org.eclipse.swt.custom.StackLayout;
import org.eclipse.swt.layout.GridData;
import org.eclipse.swt.layout.GridLayout;
import org.eclipse.swt.widgets.Composite;
import org.eclipse.swt.widgets.Control;
import org.eclipse.swt.widgets.Label;
import org.eclipse.ui.forms.widgets.FormToolkit;
import org.eclipse.ui.forms.widgets.ScrolledForm;

@SuppressWarnings({"rawtypes", "unchecked"})
public class FormPropertySheetViewer extends Viewer {

    private final ScrolledForm form;
    private final StackLayout stackLayout = new StackLayout();
    private final Composite propertiesComposite;
    private final Composite noSelectionComposite;
    private final Map<IPropertyDesc[], Composite> modelComposities = new HashMap<IPropertyDesc[], Composite>();
    private final FormToolkit toolkit;
    private Composite currentComposite;
    private IPropertyModel[] models;
    private IContainer contentRoot;
    private ArrayList<IPropertyEditor> editors = new ArrayList<IPropertyEditor>();

    public FormPropertySheetViewer(Composite parent, IContainer contentRoot) {
        this.contentRoot = contentRoot;
        toolkit = new FormToolkit(parent.getDisplay());
        this.form = toolkit.createScrolledForm(parent);
        form.getBody().setLayout(new GridLayout());
        propertiesComposite = toolkit.createComposite(this.form.getBody());
        propertiesComposite.setLayoutData(new GridData(GridData.FILL_BOTH));
        propertiesComposite.setLayout(stackLayout);
        noSelectionComposite = toolkit.createComposite(this.propertiesComposite);
        noSelectionComposite.setLayout(new GridLayout());

        stackLayout.topControl = noSelectionComposite;
        propertiesComposite.layout();

        this.form.reflow(true);
    }

    public void dispose() {
        for (IPropertyEditor e : editors) {
            e.dispose();
        }
    }

    @Override
    public Control getControl() {
        return this.form;
    }

    @Override
    public Object getInput() {
        return null;
    }

    @Override
    public ISelection getSelection() {
        return null;
    }

    @Override
    public void refresh() {
        if (models != null && currentComposite != null) {
            IPropertyDesc[] descs = models[0].getPropertyDescs();
            for (IPropertyDesc desc : descs) {
                IPropertyEditor editor = (IPropertyEditor) currentComposite.getData(desc.getName());
                if (editor != null) {
                    editor.setModels(models);
                    editor.refresh();
                }
            }
        }
    }

    private void setNoSelection() {
        this.models = null;
        stackLayout.topControl = noSelectionComposite;
        propertiesComposite.layout();
        this.form.reflow(true);
    }

    private Composite getPropertiesComposite(IPropertyModel model) {
        if (!modelComposities.containsKey(model.getPropertyDescs())) {
            Composite c = toolkit.createComposite(this.propertiesComposite);

            IPropertyDesc[] descs = model.getPropertyDescs();

            GridLayout layout = new GridLayout();
            layout.marginWidth = 0;
            c.setLayout(layout);
            layout.numColumns = 1;

            for (IPropertyDesc desc : descs) {
                Label label = new Label(c, SWT.NULL);
                label.setText(desc.getName() + ":");

                IPropertyEditor editor = desc.createEditor(c, contentRoot);
                this.editors.add(editor);
                Control control;
                if (editor == null) {
                    control = new Label(c, SWT.NONE);
                } else {
                    // Only map name to control for user-created editors
                    c.setData(desc.getName(), editor);
                    control = editor.getControl();
                }
                control.setLayoutData(new GridData(GridData.FILL_HORIZONTAL));
            }

            modelComposities.put(model.getPropertyDescs(), c);
        }

        return modelComposities.get(model.getPropertyDescs());
    }

    IPropertyModel getModel(Object object) {
        if (object instanceof IAdaptable) {
            IAdaptable adaptable = (IAdaptable) object;
            IPropertyModel model = (IPropertyModel) adaptable.getAdapter(IPropertyModel.class);
            return model;
        }
        return null;
    }

    @Override
    public void setInput(Object input) {
        Object[] array = (Object[]) input;
        ArrayList<IPropertyModel> modelList = new ArrayList<IPropertyModel>(array.length);
        for (Object object : array) {
            IPropertyModel m = getModel(object);
            if (m != null) {
                if (modelList.size() == 0) {
                    modelList.add(m);
                } else if (m.getPropertyDescs().equals(modelList.get(0).getPropertyDescs())) {
                    // Only keep selected with same property-descs as the first found
                    modelList.add(m);
                }
            }
        }

        if (modelList.size() == 0) {
            setNoSelection();
            return;
        }

        this.models = modelList.toArray(new IPropertyModel[modelList.size()]);
        currentComposite = getPropertiesComposite(models[0]);

        refresh();
        stackLayout.topControl = currentComposite;
        propertiesComposite.layout();
        this.form.reflow(true);
    }

    @Override
    public void setSelection(ISelection selection, boolean reveal) {
    }

}
