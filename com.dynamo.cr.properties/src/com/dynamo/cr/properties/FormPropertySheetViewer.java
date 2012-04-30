package com.dynamo.cr.properties;

import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.Map;

import org.eclipse.core.resources.IContainer;
import org.eclipse.core.runtime.IAdaptable;
import org.eclipse.core.runtime.IStatus;
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
    private final Map<String, Composite> modelComposities = new HashMap<String, Composite>();
    private final FormToolkit toolkit;
    private Composite currentComposite;
    private IPropertyModel[] models;
    private IContainer contentRoot;
    private ArrayList<IPropertyEditor> editors = new ArrayList<IPropertyEditor>();

    static private class Entry {

        public Entry(IPropertyEditor editor, StatusLabel statusLabel,
                Label dummyLabel) {
            this.editor = editor;
            this.statusLabel = statusLabel;
            this.dummyLabel = dummyLabel;
        }

        IPropertyEditor editor;
        StatusLabel statusLabel;
        Label dummyLabel;
    }

    public FormPropertySheetViewer(Composite parent, IContainer contentRoot) {
        this.contentRoot = contentRoot;
        toolkit = new FormToolkit(parent.getDisplay());
        this.form = toolkit.createScrolledForm(parent);
        this.form.setText("Properties");
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
        boolean changed = false;

        if (models != null && currentComposite != null) {
            IPropertyDesc[] descs = models[0].getPropertyDescs();
            for (IPropertyDesc desc : descs) {
                Entry entry = (Entry) currentComposite.getData(desc.getId());
                // Skip hidden (ie not visible) properties
                if (entry == null)
                    continue;
                IPropertyEditor editor = entry.editor;
                if (editor != null) {
                    editor.setModels(models);
                    editor.refresh();
                    StatusLabel statusLabel = entry.statusLabel;
                    Label dummyLabel = entry.dummyLabel;
                    GridData gdStatusLabel = (GridData) statusLabel.getLayoutData();
                    GridData gdDummyLabel = (GridData) dummyLabel.getLayoutData();
                    boolean exclude = true;
                    if (models.length == 1) {
                        IStatus status = models[0].getPropertyStatus(desc.getId());
                        if (status != null && status.getSeverity() >= IStatus.INFO) {
                            exclude = false;
                            statusLabel.setStatus(status);
                        }
                    } else {
                        exclude = true;
                    }
                    statusLabel.setVisible(!exclude);
                    dummyLabel.setVisible(!exclude);

                    if (exclude != gdStatusLabel.exclude) {
                        changed = true;
                    }
                    gdStatusLabel.exclude = exclude;
                    gdDummyLabel.exclude = exclude;
                }
            }
        }

        if (changed) {
            currentComposite.pack();
            currentComposite.layout(true);
            propertiesComposite.layout(true);

            this.form.reflow(true);
        }
    }

    private void setNoSelection() {
        this.models = null;
        stackLayout.topControl = noSelectionComposite;
        propertiesComposite.layout();
        this.form.reflow(true);
    }

    private static String niceifyLabel(String label) {
        StringBuilder ret = new StringBuilder();
        for (int i = 0; i < label.length(); ++i) {
            char c = label.charAt(i);
            if (i == 0) {
                c = Character.toUpperCase(c);
            } else {
                if (Character.isUpperCase(c)) {
                    ret.append(' ');
                }
            }
            ret.append(c);
        }

        return ret.toString();
    }

    private Composite getPropertiesComposite(IPropertyModel model) {
        MessageDigest digest;
        try {
            digest = MessageDigest.getInstance("MD5");
        } catch (NoSuchAlgorithmException e) {
            throw new RuntimeException(e);
        }

        /*
         * NOTE: We cache properties composite for performce reasons
         * We previously used the array of property descriptors as key but when dynamic
         * properties where introduced the assumption about the key isn't true anymore.
         * We used to assume that model.getPropertyDescs() returned the same reference
         * for every invocation
         * The new scheme is to hash to property id and property-descriptor-class.
         * This is hopefully unique enough.
         */
        IPropertyDesc[] descs = model.getPropertyDescs();
        for (IPropertyDesc pd : descs) {
            digest.update(pd.getId().getBytes());
            digest.update(pd.getClass().toString().getBytes());
        }
        String hashString = new String(digest.digest());

        if (!modelComposities.containsKey(hashString)) {
            Composite c = toolkit.createComposite(this.propertiesComposite);

            GridLayout layout = new GridLayout();
            layout.marginWidth = 0;
            c.setLayout(layout);
            layout.numColumns = 2;

            for (IPropertyDesc desc : descs) {

                if (!model.isPropertyVisible(desc.getId()))
                    continue;

                Label label = new Label(c, SWT.NONE);
                label.setText(niceifyLabel(desc.getName()));

                IPropertyEditor editor = desc.createEditor(c, contentRoot);
                Control control;
                if (editor == null) {
                    control = new Label(c, SWT.NONE);
                } else {
                    control = editor.getControl();
                    this.editors.add(editor);
                }
                GridData gd = new GridData(GridData.FILL_HORIZONTAL);
                gd.widthHint = 50;
                control.setLayoutData(gd);

                gd = new GridData();
                gd.exclude = true;
                Label dummyLabel = new Label(c, SWT.NONE);
                dummyLabel.setLayoutData(gd);

                StatusLabel statusLabel = new StatusLabel(c, SWT.BORDER);
                gd = new GridData(GridData.FILL_HORIZONTAL);
                gd.exclude = true;
                gd.horizontalSpan = 1;
                statusLabel.setLayoutData(gd);
                Entry entry = new Entry(editor, statusLabel, dummyLabel);
                c.setData(desc.getId(), entry);
            }

            modelComposities.put(hashString, c);
        }

        return modelComposities.get(hashString);
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
        boolean relayout = stackLayout.topControl != currentComposite;
        stackLayout.topControl = currentComposite;

        if (relayout) {
            propertiesComposite.layout();
            this.form.reflow(true);
        }
    }

    @Override
    public void setSelection(ISelection selection, boolean reveal) {
    }

}
