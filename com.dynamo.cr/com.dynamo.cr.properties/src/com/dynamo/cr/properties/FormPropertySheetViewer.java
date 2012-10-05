package com.dynamo.cr.properties;

import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.Map;

import org.eclipse.core.commands.operations.IUndoableOperation;
import org.eclipse.core.resources.IContainer;
import org.eclipse.core.runtime.IAdaptable;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.jface.viewers.ISelection;
import org.eclipse.jface.viewers.Viewer;
import org.eclipse.swt.SWT;
import org.eclipse.swt.custom.StackLayout;
import org.eclipse.swt.graphics.Color;
import org.eclipse.swt.graphics.RGB;
import org.eclipse.swt.layout.GridData;
import org.eclipse.swt.layout.GridLayout;
import org.eclipse.swt.widgets.Composite;
import org.eclipse.swt.widgets.Control;
import org.eclipse.swt.widgets.Label;
import org.eclipse.ui.forms.events.HyperlinkAdapter;
import org.eclipse.ui.forms.events.HyperlinkEvent;
import org.eclipse.ui.forms.widgets.FormToolkit;
import org.eclipse.ui.forms.widgets.Hyperlink;
import org.eclipse.ui.forms.widgets.ScrolledForm;
import org.eclipse.ui.forms.widgets.Section;

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

        public Entry(Label label, Hyperlink link, IPropertyEditor editor, StatusLabel statusLabel,
                Label dummyLabel) {
            this.label = label;
            this.link = link;
            this.editor = editor;
            this.statusLabel = statusLabel;
            this.dummyLabel = dummyLabel;
        }

        Label label;
        Hyperlink link;
        IPropertyEditor editor;
        StatusLabel statusLabel;
        Label dummyLabel;
    }

    public FormPropertySheetViewer(Composite parent, IContainer contentRoot) {
        this.contentRoot = contentRoot;
        toolkit = new FormToolkit(parent.getDisplay());
        this.form = toolkit.createScrolledForm(parent);
        this.form.setText("Properties"); //$NON-NLS-1$
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

                    boolean overridden = false;
                    for (int i = 0; i < models.length; ++i) {
                        if (models[i].isPropertyOverridden(desc.getId())) {
                            overridden = true;
                            break;
                        }
                    }
                    StackLayout stack = (StackLayout)entry.label.getParent().getLayout();
                    if (overridden) {
                        stack.topControl = entry.link;
                    } else {
                        stack.topControl = entry.label;
                    }
                    entry.label.getParent().layout();
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

    private Composite getPropertiesComposite(final IPropertyModel model) {
        MessageDigest digest;
        try {
            digest = MessageDigest.getInstance("MD5"); //$NON-NLS-1$
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
            Composite modelComposite = c;

            GridLayout layout = new GridLayout();
            layout.marginWidth = 0;
            c.setLayout(layout);
            layout.numColumns = 2;

            String category = "";
            for (final IPropertyDesc desc : descs) {

                if (!model.isPropertyVisible(desc.getId()))
                    continue;

                if (!category.equals(desc.getCategory()) && !desc.getCategory().equals("")) {
                    category = desc.getCategory();
                    Section section = toolkit.createSection(modelComposite,  Section.TITLE_BAR| Section.TWISTIE|Section.EXPANDED);
                    section.setText(category);
                    GridData gd = new GridData(GridData.FILL_HORIZONTAL);
                    gd.horizontalSpan = 2;
                    section.setLayoutData(gd);

                    Composite sectionClient = toolkit.createComposite(section);
                    GridLayout sectionClientLayout = new GridLayout();
                    sectionClientLayout.marginWidth = 0;
                    sectionClient.setLayout(sectionClientLayout);
                    sectionClientLayout.numColumns = 2;

                    section.setClient(sectionClient);
                    c = sectionClient;
                }

                String labelText = niceifyLabel(desc.getName());
                final Composite labelComposite = toolkit.createComposite(c, SWT.NONE);
                final StackLayout labelLayout = new StackLayout();
                labelComposite.setLayout(labelLayout);
                final Label label = toolkit.createLabel(labelComposite, labelText, SWT.NONE);

                Hyperlink link = toolkit.createHyperlink(labelComposite, labelText, SWT.NONE);
                link.addHyperlinkListener(new HyperlinkAdapter() {
                    @Override
                    public void linkActivated(HyperlinkEvent e) {
                        for (IPropertyModel m : models) {
                            IUndoableOperation operation = m.resetPropertyValue(desc.getId());
                            m.getCommandFactory().execute(operation, m.getWorld());
                        }
                    }
                });
                link.setForeground(new Color(c.getDisplay(), new RGB(0, 0, 255)));
                link.setUnderlined(true);
                link.setToolTipText(Messages.FormPropertySheetViewer_RESET_VALUE);

                IPropertyEditor editor = desc.createEditor(toolkit, c, contentRoot);
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
                Entry entry = new Entry(label, link, editor, statusLabel, dummyLabel);
                modelComposite.setData(desc.getId(), entry);
            }

            modelComposities.put(hashString, modelComposite);
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
                } else {
                    IPropertyDesc[] lhs = m.getPropertyDescs();
                    IPropertyDesc[] rhs = modelList.get(0).getPropertyDescs();
                    boolean equal = true;
                    for (int i = 0; i < lhs.length; ++i) {
                        if (!lhs[i].getClass().equals(rhs[i].getClass())) {
                            equal = false;
                            break;
                        }
                    }
                    if (equal) {
                        // Only keep selected with same property-descs as the first found
                        modelList.add(m);
                    }
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
