package com.dynamo.cr.goeditor;

import java.util.List;

import javax.annotation.PreDestroy;
import javax.inject.Inject;

import org.eclipse.core.resources.IContainer;
import org.eclipse.core.resources.IResource;
import org.eclipse.jface.dialogs.Dialog;
import org.eclipse.jface.viewers.ArrayContentProvider;
import org.eclipse.jface.viewers.CellEditor;
import org.eclipse.jface.viewers.ICellEditorListener;
import org.eclipse.jface.viewers.ICellModifier;
import org.eclipse.jface.viewers.ISelection;
import org.eclipse.jface.viewers.ISelectionChangedListener;
import org.eclipse.jface.viewers.ISelectionProvider;
import org.eclipse.jface.viewers.IStructuredSelection;
import org.eclipse.jface.viewers.LabelProvider;
import org.eclipse.jface.viewers.TableViewer;
import org.eclipse.jface.viewers.TextCellEditor;
import org.eclipse.swt.SWT;
import org.eclipse.swt.events.KeyEvent;
import org.eclipse.swt.events.KeyListener;
import org.eclipse.swt.events.SelectionAdapter;
import org.eclipse.swt.events.SelectionEvent;
import org.eclipse.swt.graphics.Image;
import org.eclipse.swt.layout.FillLayout;
import org.eclipse.swt.layout.GridData;
import org.eclipse.swt.layout.GridLayout;
import org.eclipse.swt.widgets.Button;
import org.eclipse.swt.widgets.Composite;
import org.eclipse.swt.widgets.Table;
import org.eclipse.swt.widgets.TableItem;
import org.eclipse.ui.IWorkbenchPartSite;
import org.eclipse.ui.contexts.IContextActivation;
import org.eclipse.ui.contexts.IContextService;
import org.eclipse.ui.dialogs.ResourceListSelectionDialog;
import org.eclipse.ui.forms.widgets.Form;
import org.eclipse.ui.forms.widgets.FormToolkit;
import org.eclipse.ui.forms.widgets.Section;

import com.dynamo.cr.editor.core.EditorUtil;
import com.dynamo.cr.editor.core.IResourceType;

public class GameObjectView implements IGameObjectView, ISelectionProvider {

    private FormToolkit toolkit;
    private Form form;
    private TableViewer componentsViewer;

    @Inject private Composite parent;
    @Inject private ImageFactory imageFactory;
    @Inject private IWorkbenchPartSite site;
    @Inject private IGameObjectView.Presenter presenter;
    @Inject private IContainer contentRoot;
    @Inject private GameObjectEditor2 editor;
    @InjectImage("/icons/brick.png") private Image brickImage;
    @InjectImage("/icons/link_add.png") private Image linkAddImage;
    @InjectImage("/icons/page_add.png") private Image pageAddImage;
    @InjectImage("/icons/page_delete.png") private Image pageDeleteImage;
    private IContextActivation contextActivation;
    private boolean mac;

    class CellModifier implements ICellModifier {
        private boolean enabled;

        public CellModifier() {
            this.enabled = false;
        }

        public void setEnabled(boolean enabled) {
            this.enabled = enabled;
        }

        @Override
        public boolean canModify(Object element, String property) {
            return this.enabled;
        }

        @Override
        public Object getValue(Object element, String property) {
            Component component = (Component) element;
            return component.getId();
        }

        @Override
        public void modify(Object element, String property, Object value) {
            TableItem item = (TableItem) element;
            String id = (String)value;
            Component component = (Component)item.getData();
            if (!component.getId().equals(id)) {
                presenter.onSetComponentId(component, id);
            }
        }
    }

    class CellEditorListener implements ICellEditorListener {
        @Override
        public void applyEditorValue() {
            activateContext();
        }

        @Override
        public void cancelEditor() {
            deactivateContext();
        }

        @Override
        public void editorValueChanged(boolean oldValidState, boolean newValidState) {}
    }

    public GameObjectView() {
        this.mac = System.getProperty("os.name").toLowerCase().indexOf( "mac" ) >= 0;
    }

    @PreDestroy
    public void dispose() {
        toolkit.dispose();
        // NOTE: imageFactory is disposed by injector
    }

    @Override
    public void create(String name) {
        site.setSelectionProvider(this);
        toolkit = new FormToolkit(parent.getDisplay());

        form = toolkit.createForm(parent);
        form.setImage(brickImage);

        form.setText("Game Object Prototype");
        toolkit.decorateFormHeading(form);
        form.getBody().setLayout(new FillLayout());

        Section section1 = toolkit.createSection(form.getBody(), Section.DESCRIPTION | Section.TITLE_BAR);
        section1.setText("Components");
        section1.setDescription("Add, remove and edit components.");
        section1.marginWidth = 10;
        section1.marginHeight = 5;

        Section section2 = toolkit.createSection(form.getBody(), Section.DESCRIPTION | Section.TITLE_BAR);
        section2.setText("Component Details");
        section2.setDescription("Edit component properties.");
        section2.marginWidth = 10;
        section2.marginHeight = 5;

        Composite components = toolkit.createComposite(section1, SWT.WRAP);
        GridData gd = new GridData(GridData.FILL_BOTH);
        components.setLayoutData(gd);
        createComponentsComposite(components);

        Composite details = toolkit.createComposite(section2, SWT.WRAP);
        gd = new GridData(GridData.FILL_BOTH);
        details.setLayoutData(gd);
        createDetailsComposite(details);

        section1.setClient(components);
        section2.setClient(details);

        IContextService contextService = (IContextService) site.getService(IContextService.class);
        this.contextActivation = contextService.activateContext("com.dynamo.cr.goeditor.contexts.gameObjectEditor");
    }

    @Override
    public void setDirty(boolean dirty) {
        editor.fireDirty();
    }

    private void createDetailsComposite(Composite parent) {
        Button button = new Button(parent, SWT.PUSH);
        button.setText("PLACEHOLDER");
    }

    private Button createButton(Composite parent, String text, Image image) {
        Button button = toolkit.createButton(parent, null, SWT.PUSH);
        button.setToolTipText(text);
        button.setImage(image);
        GridData gd = new GridData(GridData.VERTICAL_ALIGN_CENTER);
        gd.horizontalAlignment = SWT.LEFT;
        button.setLayoutData(gd);
        button.setAlignment(SWT.CENTER);
        return button;
    }

    private void createComponentsComposite(Composite parent) {
        GridLayout layout = new GridLayout();
        layout.numColumns = 3;
        layout.marginWidth = 2;
        layout.marginHeight = 2;
        parent.setLayout(layout);

        Button addEmedded = createButton(parent, "Add Reference Component...", linkAddImage);
        Button addResource = createButton(parent, "Add Component...", pageAddImage);
        Button remove = createButton(parent, "Remove Componene", pageDeleteImage);

        addEmedded.addSelectionListener(new SelectionAdapter() {
            @Override
            public void widgetSelected(SelectionEvent e) {
                presenter.onAddEmbeddedComponent();
            }
        });

        addResource.addSelectionListener(new SelectionAdapter() {
            @Override
            public void widgetSelected(SelectionEvent e) {
                presenter.onAddResourceComponent();
            }
        });

        remove.addSelectionListener(new SelectionAdapter() {
            @Override
            public void widgetSelected(SelectionEvent e) {
                IStructuredSelection selection = (IStructuredSelection) componentsViewer.getSelection();
                Component component = (Component) selection.getFirstElement();
                presenter.onRemoveComponent(component);
            }
        });

        Table t = toolkit.createTable(parent, SWT.NULL);
        GridData gd = new GridData(GridData.FILL_BOTH);
        gd.heightHint = 20;
        gd.widthHint = 200;
        gd.horizontalSpan = 3;
        t.setLayoutData(gd);
        toolkit.paintBordersFor(parent);

        componentsViewer = new TableViewer(t);
        componentsViewer.setContentProvider(new ArrayContentProvider());
        componentsViewer.setLabelProvider(new LabelProvider() {
            @Override
            public Image getImage(Object element) {
                Component component = (Component) element;
                return imageFactory.getImageFromFilename("x." + component.getFileExtension());
            }
        });

        componentsViewer.setColumnProperties(new String[] {"column1"});


        TextCellEditor cellEditor = new TextCellEditor(componentsViewer.getTable());
        cellEditor.addListener(new CellEditorListener());
        componentsViewer.setCellEditors(new CellEditor[] {cellEditor});
        final CellModifier cellModifier = new CellModifier();
        componentsViewer.setCellModifier(cellModifier);


        componentsViewer.getTable().addKeyListener(new KeyListener() {

            @Override
            public void keyReleased(KeyEvent e) { }

            @Override
            public void keyPressed(KeyEvent e) {
                if (e.keyCode == SWT.F2 || (e.keyCode == SWT.CR && mac)) {
                    deactivateContext();

                    cellModifier.setEnabled(true);
                    ISelection selection = componentsViewer.getSelection();
                    if (!selection.isEmpty()) {
                        Object first = ((IStructuredSelection) selection).getFirstElement();
                        componentsViewer.editElement(first, 0);
                    }
                }
                cellModifier.setEnabled(false);
            }
        });

    }

    @Override
    public String openAddResourceComponentDialog() {
        ResourceListSelectionDialog dialog = new ResourceListSelectionDialog(site.getShell(), contentRoot, IResource.FILE | IResource.DEPTH_INFINITE);
        dialog.setTitle("Add Reference Component");

        int ret = dialog.open();
        if (ret == Dialog.OK) {
            IResource r = (IResource) dialog.getResult()[0];
            String resourcePath = EditorUtil.makeResourcePath(r);
            return resourcePath;
        } else {
            return null;
        }
    }

    @Override
    public IResourceType openAddEmbeddedComponentDialog(IResourceType[] resourceTypes) {
        EmbeddedComponentListDialog dialog = new EmbeddedComponentListDialog(site.getShell(), imageFactory, resourceTypes);

        int ret = dialog.open();
        if (ret == Dialog.OK) {
            Object[] result = dialog.getResult();
            IResourceType resourceType = (IResourceType) result[0];
            return resourceType;
        } else {
            return null;
        }
    }

    @Override
    public void setComponents(List<Component> components) {
        componentsViewer.setInput(components);
    }

    @Override
    public boolean setFocus() {
        return this.form.getBody().setFocus();
    }

    @Override
    public void addSelectionChangedListener(ISelectionChangedListener listener) {
        // We just pass on to the viewer here
        componentsViewer.addSelectionChangedListener(listener);
    }

    @Override
    public ISelection getSelection() {
        return componentsViewer.getSelection();
    }

    @Override
    public void removeSelectionChangedListener(
            ISelectionChangedListener listener) {
        // We just pass on to the viewer here
        componentsViewer.removeSelectionChangedListener(listener);
    }

    @Override
    public void setSelection(ISelection selection) {
        componentsViewer.setSelection(selection);
    }

    void activateContext() {
        IContextService contextService = (IContextService) site.getService(IContextService.class);
        contextActivation = contextService.activateContext("com.dynamo.cr.goeditor.contexts.gameObjectEditor");
    }

    void deactivateContext() {
        IContextService contextService = (IContextService) site.getService(IContextService.class);
        contextService.deactivateContext(this.contextActivation);
    }

}
