package com.dynamo.cr.ddfeditor;

import java.io.ByteArrayInputStream;
import java.io.IOException;
import java.io.InputStreamReader;
import java.io.Reader;
import java.io.StringReader;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;

import org.eclipse.core.commands.ExecutionException;
import org.eclipse.core.commands.operations.AbstractOperation;
import org.eclipse.core.commands.operations.IOperationApprover;
import org.eclipse.core.commands.operations.IOperationHistory;
import org.eclipse.core.commands.operations.IOperationHistoryListener;
import org.eclipse.core.commands.operations.IUndoableOperation;
import org.eclipse.core.commands.operations.OperationHistoryEvent;
import org.eclipse.core.commands.operations.UndoContext;
import org.eclipse.core.resources.IContainer;
import org.eclipse.core.resources.IFile;
import org.eclipse.core.resources.IResource;
import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.IAdaptable;
import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Path;
import org.eclipse.core.runtime.Status;
import org.eclipse.jface.dialogs.Dialog;
import org.eclipse.jface.dialogs.ErrorDialog;
import org.eclipse.jface.dialogs.MessageDialog;
import org.eclipse.jface.resource.ImageDescriptor;
import org.eclipse.jface.resource.ImageRegistry;
import org.eclipse.jface.viewers.ArrayContentProvider;
import org.eclipse.jface.viewers.CellEditor;
import org.eclipse.jface.viewers.ICellEditorListener;
import org.eclipse.jface.viewers.ICellModifier;
import org.eclipse.jface.viewers.ISelection;
import org.eclipse.jface.viewers.ISelectionChangedListener;
import org.eclipse.jface.viewers.IStructuredContentProvider;
import org.eclipse.jface.viewers.IStructuredSelection;
import org.eclipse.jface.viewers.LabelProvider;
import org.eclipse.jface.viewers.SelectionChangedEvent;
import org.eclipse.jface.viewers.StructuredSelection;
import org.eclipse.jface.viewers.TableViewer;
import org.eclipse.jface.viewers.TextCellEditor;
import org.eclipse.jface.viewers.Viewer;
import org.eclipse.jface.viewers.ViewerCell;
import org.eclipse.jface.viewers.ViewerFilter;
import org.eclipse.jface.viewers.ViewerSorter;
import org.eclipse.swt.SWT;
import org.eclipse.swt.events.KeyEvent;
import org.eclipse.swt.events.KeyListener;
import org.eclipse.swt.events.MouseAdapter;
import org.eclipse.swt.events.MouseEvent;
import org.eclipse.swt.events.SelectionEvent;
import org.eclipse.swt.events.SelectionListener;
import org.eclipse.swt.graphics.Image;
import org.eclipse.swt.graphics.Point;
import org.eclipse.swt.layout.FillLayout;
import org.eclipse.swt.layout.GridData;
import org.eclipse.swt.layout.GridLayout;
import org.eclipse.swt.widgets.Button;
import org.eclipse.swt.widgets.Composite;
import org.eclipse.swt.widgets.Display;
import org.eclipse.swt.widgets.Table;
import org.eclipse.swt.widgets.TableItem;
import org.eclipse.ui.IActionBars;
import org.eclipse.ui.IEditorInput;
import org.eclipse.ui.IEditorSite;
import org.eclipse.ui.IFileEditorInput;
import org.eclipse.ui.IWorkbenchPage;
import org.eclipse.ui.PartInitException;
import org.eclipse.ui.PlatformUI;
import org.eclipse.ui.actions.ActionFactory;
import org.eclipse.ui.contexts.IContextActivation;
import org.eclipse.ui.contexts.IContextService;
import org.eclipse.ui.dialogs.ListDialog;
import org.eclipse.ui.dialogs.ResourceListSelectionDialog;
import org.eclipse.ui.forms.widgets.Form;
import org.eclipse.ui.forms.widgets.FormToolkit;
import org.eclipse.ui.forms.widgets.Section;
import org.eclipse.ui.ide.IDE;
import org.eclipse.ui.operations.LinearUndoViolationUserApprover;
import org.eclipse.ui.operations.RedoActionHandler;
import org.eclipse.ui.operations.UndoActionHandler;
import org.eclipse.ui.part.EditorPart;

import com.dynamo.cr.editor.core.EditorCorePlugin;
import com.dynamo.cr.editor.core.EditorUtil;
import com.dynamo.cr.editor.core.IResourceType;
import com.dynamo.cr.editor.core.IResourceTypeRegistry;
import com.dynamo.cr.protobind.IPath;
import com.dynamo.cr.protobind.MessageNode;
import com.dynamo.gameobject.proto.GameObject.ComponentDesc;
import com.dynamo.gameobject.proto.GameObject.EmbeddedComponentDesc;
import com.dynamo.gameobject.proto.GameObject.PrototypeDesc;
import com.google.protobuf.Descriptors.Descriptor;
import com.google.protobuf.DynamicMessage;
import com.google.protobuf.Message;
import com.google.protobuf.TextFormat;
import com.google.protobuf.UninitializedMessageException;

public class GameObjectEditor extends EditorPart implements IOperationHistoryListener, ISelectionChangedListener, ICellEditorListener, IProtoListener {

    private FormToolkit toolkit;
    private Form form;
    private UndoContext undoContext;
    private UndoActionHandler undoAction;
    private RedoActionHandler redoAction;
    private int cleanUndoStackDepth = 0;
    private TableViewer componentsViewer;
    private ArrayList<Component> allComponents;
    private ProtoTreeEditor protoTreeEditor;
    private IContainer contentRoot;
    private Button removeButton;
    private HashSet<String> idSet = new HashSet<String>();
    private IContextActivation contextActivation;
    private boolean mac;
    private Map<ImageDescriptor, Image> imageDescToImage = new HashMap<ImageDescriptor, Image>();

    class AddComponentOperation extends AbstractOperation {
        private Component component;

        public AddComponentOperation(String label, Component component) {
            super(label);
            this.component = component;
        }

        @Override
        public IStatus execute(IProgressMonitor monitor, IAdaptable info)
                throws ExecutionException {
            addComponent(this.component);
            return Status.OK_STATUS;
        }

        @Override
        public IStatus redo(IProgressMonitor monitor, IAdaptable info)
                throws ExecutionException {
            return execute(monitor, info);
        }

        @Override
        public IStatus undo(IProgressMonitor monitor, IAdaptable info)
                throws ExecutionException {
            removeComponent(this.component);
            return Status.OK_STATUS;
        }
    }

    class RemoveComponentOperation extends AbstractOperation {
        private int index;
        private Component component;

        public RemoveComponentOperation(String label, int index, Component component) {
            super(label);
            this.index = index;
            this.component = component;
        }

        @Override
        public IStatus execute(IProgressMonitor monitor, IAdaptable info)
                throws ExecutionException {
            removeComponent(this.component);
            return Status.OK_STATUS;
        }

        @Override
        public IStatus redo(IProgressMonitor monitor, IAdaptable info)
                throws ExecutionException {
            return execute(monitor, info);
        }

        @Override
        public IStatus undo(IProgressMonitor monitor, IAdaptable info)
                throws ExecutionException {
            addComponent(this.index, this.component);
            return Status.OK_STATUS;
        }
    }

    class SetIdOperation extends AbstractOperation {
        private GameObjectEditor editor;
        private Component component;
        private String oldId;
        private String newId;

        public SetIdOperation(GameObjectEditor editor, Component component, String newId) {
            super("Set Id");
            this.editor = editor;
            this.component = component;
            this.oldId = component.getId();
            this.newId = newId;
        }

        @Override
        public IStatus execute(IProgressMonitor monitor, IAdaptable info)
                throws ExecutionException {
            if (this.component == null) {
                throw new ExecutionException("No item was selected when setting id.");
            } else if (this.newId == null || this.newId.isEmpty()) {
                throw new ExecutionException("Identifier can not be empty.");
            } else if (this.editor != null && this.editor.isIdUsed(this.newId)) {
                throw new ExecutionException(String.format("Identifier '%s' is already used.", this.newId));
            }
            this.component.setId(this.newId);
            return Status.OK_STATUS;
        }

        @Override
        public IStatus redo(IProgressMonitor monitor, IAdaptable info)
                throws ExecutionException {
            return execute(monitor, info);
        }

        @Override
        public IStatus undo(IProgressMonitor monitor, IAdaptable info)
                throws ExecutionException {
            this.component.setId(this.oldId);
            return Status.OK_STATUS;
        }
    }

    class CellModifier implements ICellModifier {
        private GameObjectEditor editor;
        private boolean enabled;

        public CellModifier(GameObjectEditor editor) {
            this.editor = editor;
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
            if (this.enabled) {
                return component.getId();
            } else {
                return component.getLabel();
            }
        }

        @Override
        public void modify(Object element, String property, Object value) {
            TableItem item = (TableItem) element;
            // Check if new value equals old
            String id = (String)value;
            Component component = (Component)item.getData();
            if (!component.getId().equals(id)) {
                executeOperation(new SetIdOperation(this.editor, component, id));
            }
        }
    }

    static abstract class Component {
        private GameObjectEditor editor;
        private String id;
        private IResourceType resourceType;
        public static final String ID_PROPERTY = "id";
        public static final String COMPONENT_PROPERTY = "component";

        public Component(GameObjectEditor editor, String id, String ext) {
            this.editor = editor;
            this.id = id;
            if (ext != null)
                this.resourceType = EditorCorePlugin.getDefault().getResourceTypeRegistry().getResourceTypeFromExtension(ext);
        }

        public String getId() {
            return this.id;
        }

        public void setId(String id) {
            editor.removeId(this.id);
            editor.addId(id);
            this.id = id;
            MessageNode messageNode = getEditableMessageNode();
            IPath idPath = messageNode.getPathTo(ID_PROPERTY);
            if (idPath != null) {
                messageNode.setField(idPath, id);
            }
            editor.componentChanged(this, new String[] {ID_PROPERTY});
        }

        public IResourceType getResourceType() {
            return this.resourceType;
        }

        @Override
        public String toString() {
            return getLabel();
        }

        public abstract String getExtension();
        public abstract MessageNode getEditableMessageNode();
        public abstract Message getSavableMessage();
        public abstract String getLabel();

        public Image getImage() {
            String ext = getExtension();
            if (ext != null) {
                return editor.getImageFromFilename(ext);
            } else {
                ImageRegistry imageRegistry = Activator.getDefault().getImageRegistry();
                return imageRegistry.get("exclamation");
            }
        }

        protected static String getExtension(String resource) {
            int index = resource.lastIndexOf(".");
            if (index != -1)
                return resource.substring(index);
            else
                return null;
        }
    }

    static class ResourceComponent extends Component {
        private MessageNode messageNode;

        public ResourceComponent(GameObjectEditor editor, ComponentDesc desc) {
            super(editor, desc.getId(), getExtension(desc.getComponent()));
            this.messageNode = new MessageNode(desc);
        }

        @Override
        public String getLabel() {
            return getId() + " (" + getResource() + ")";
        }

        @Override
        public String getExtension() {
            return getExtension(getResource());
        }

        @Override
        public MessageNode getEditableMessageNode() {
            return messageNode;
        }

        @Override
        public Message getSavableMessage() {
            return messageNode.build();
        }

        public String getResource() {
            return (String)this.messageNode.getField(Component.COMPONENT_PROPERTY);
        }
    }

    static class EmbeddedComponent extends Component {

        private String type;
        private MessageNode messageNode;

        public EmbeddedComponent(GameObjectEditor editor, EmbeddedComponentDesc desc) {
            super(editor, desc.getId(), desc.getType());
            this.type = desc.getType();

            Descriptor protoDescriptor = getResourceType().getMessageDescriptor();

            if (protoDescriptor == null) {
                throw new RuntimeException(String.format("Unknown type: %s", type));
            }
            DynamicMessage.Builder builder = DynamicMessage.newBuilder(protoDescriptor);

            try {
                TextFormat.merge(new StringReader(desc.getData()), builder);
                this.messageNode = new MessageNode(builder.build());
            } catch (IOException e) {
                throw new RuntimeException(e);
            }
        }

        @Override
        public String getLabel() {
            return getId() + " (embedded " + type + ")";
        }

        @Override
        public String getExtension() {
            return "." + type;
        }

        @Override
        public MessageNode getEditableMessageNode() {
            return this.messageNode;
        }

        @Override
        public Message getSavableMessage() {
            String data = TextFormat.printToString(this.messageNode.build());
            return EmbeddedComponentDesc.newBuilder().setId(getId()).setType(type).setData(data).build();
        }

    }

    public void executeOperation(IUndoableOperation operation) {
        IOperationHistory history = PlatformUI.getWorkbench().getOperationSupport().getOperationHistory();
        operation.addContext(undoContext);
        IStatus status = null;
        try
        {
            status = history.execute(operation, null, null);
        } catch (ExecutionException e)
        {
            MessageDialog.openError(getSite().getShell(), operation.getLabel(), e.getMessage());
            // if-clause below will trigger
        }

        if (status != Status.OK_STATUS)
        {
            System.err.println("Failed to execute operation: " + operation);
        }
    }

    public GameObjectEditor() {
        this.mac = System.getProperty("os.name").toLowerCase().indexOf( "mac" ) >= 0;
    }

    @Override
    public void dispose() {
        super.dispose();

        for (Image image : imageDescToImage.values()) {
            image.dispose();
        }
    }

    public Image getImageFromFilename(String filename) {
        ImageDescriptor imageDesc = PlatformUI.getWorkbench().getEditorRegistry().getImageDescriptor(filename);

        if (!imageDescToImage.containsKey(imageDesc)) {
            Image image = imageDesc.createImage();
            imageDescToImage.put(imageDesc, image);
        }

        return imageDescToImage.get(imageDesc);
    }

    @Override
    public void doSave(IProgressMonitor monitor) {

        IFileEditorInput input = (IFileEditorInput) getEditorInput();

        PrototypeDesc.Builder builder = PrototypeDesc.newBuilder();
        for (Component component : this.allComponents) {
            Message m = component.getSavableMessage();
            if (m instanceof ComponentDesc) {
                ComponentDesc componentDesc = (ComponentDesc) m;
                builder.addComponents(componentDesc);
            }
            else if (m instanceof EmbeddedComponentDesc) {
                EmbeddedComponentDesc embedabbleComponentDesc = (EmbeddedComponentDesc) m;
                builder.addEmbeddedComponents(embedabbleComponentDesc);
            }
            else {
                assert false;
            }
        }

        String messageString = TextFormat.printToString(builder.build());
        ByteArrayInputStream stream = new ByteArrayInputStream(messageString.getBytes());

        try {
            input.getFile().setContents(stream, false, true, monitor);
            IOperationHistory history = PlatformUI.getWorkbench().getOperationSupport().getOperationHistory();
            cleanUndoStackDepth = history.getUndoHistory(undoContext).length;
            firePropertyChange(PROP_DIRTY);
        } catch (CoreException e) {
            e.printStackTrace();
            Status status = new Status(IStatus.ERROR, "com.dynamo.cr.ddfeditor", 0, e.getMessage(), null);
            ErrorDialog.openError(Display.getCurrent().getActiveShell(), "Unable to save file", "Unable to save file", status);
        }
    }

    @Override
    public void doSaveAs() {
    }

    @Override
    public void init(IEditorSite site, IEditorInput input)
            throws PartInitException {
        setSite(site);
        setInput(input);
        setPartName(input.getName());

        IFileEditorInput i = (IFileEditorInput) input;
        IFile file = i.getFile();

        this.contentRoot = EditorUtil.findContentRoot(file);

        try {
            Reader reader = new InputStreamReader(file.getContents());

            PrototypeDesc.Builder builder = PrototypeDesc.newBuilder();
            try {
                try {
                    TextFormat.merge(reader, builder);

                    //MessageNode message = new MessageNode(builder.build());
                    PrototypeDesc message = builder.build();

                    List<ComponentDesc> components = message.getComponentsList();
                    List<EmbeddedComponentDesc> embeddedComponents = message.getEmbeddedComponentsList();
                    this.allComponents = new ArrayList<Component>(components.size() + embeddedComponents.size());

                    for (ComponentDesc componentDesc : components) {
                        this.allComponents.add(new ResourceComponent(this, componentDesc));
                    }
                    for (EmbeddedComponentDesc embeddedComponentDesc : embeddedComponents) {
                        this.allComponents.add(new EmbeddedComponent(this, embeddedComponentDesc));
                    }
                } catch (UninitializedMessageException e) {
                    throw new PartInitException(e.getMessage(), e);
                } finally {
                    reader.close();
                }
            } catch (IOException e) {
                throw new PartInitException(e.getMessage(), e);
            }

        } catch (CoreException e) {
            throw new PartInitException(e.getMessage(), e);
        }

        this.undoContext = new UndoContext();
        IOperationHistory history = PlatformUI.getWorkbench().getOperationSupport().getOperationHistory();
        history.setLimit(this.undoContext, 100);
        history.addOperationHistoryListener(this);

        @SuppressWarnings("unused")
        IOperationApprover approver = new LinearUndoViolationUserApprover(this.undoContext, this);

        this.undoAction = new UndoActionHandler(this.getEditorSite(), this.undoContext);
        this.redoAction = new RedoActionHandler(this.getEditorSite(), this.undoContext);
    }

    @Override
    public boolean isDirty() {
        IOperationHistory history = PlatformUI.getWorkbench().getOperationSupport().getOperationHistory();
        return history.getUndoHistory(undoContext).length != cleanUndoStackDepth;
    }

    @Override
    public boolean isSaveAsAllowed() {
        return false;
    }

    public void onAddResourceComponent() {
        ResourceListSelectionDialog dialog = new ResourceListSelectionDialog(getSite().getShell(), contentRoot, IResource.FILE | IResource.DEPTH_INFINITE);
        dialog.setTitle("Add Resource Component");

        int ret = dialog.open();
        if (ret == Dialog.OK) {
            IResource r = (IResource) dialog.getResult()[0];

            org.eclipse.core.runtime.IPath fullPath = r.getFullPath();
            String resourcePath = EditorUtil.makeResourcePath(r);

            String id = fullPath.getFileExtension();
            id = getUniqueId(id);
            ComponentDesc componentDesc = ComponentDesc.newBuilder().setId(id).setComponent(resourcePath).build();
            ResourceComponent resourceComponent = new ResourceComponent(this, componentDesc);
            AddComponentOperation op = new AddComponentOperation("Add " + r.getName(), resourceComponent);
            executeOperation(op);
        }
    }

    public void onAddEmbeddedComponent() {
        IResourceTypeRegistry registry = EditorCorePlugin.getDefault().getResourceTypeRegistry();
        IResourceType[] resourceTypes = registry.getResourceTypes();
        List<IResourceType> embedabbleTypes = new ArrayList<IResourceType>();
        for (IResourceType t : resourceTypes) {
            if (t.isEmbeddable()) {
                embedabbleTypes.add(t);
            }
        }

        ListDialog dialog = new ListDialog(getSite().getShell());
        dialog.setTitle("Add Embedded Resource");
        dialog.setMessage("Select resource type from list to embed:");
        dialog.setContentProvider(new ArrayContentProvider());
        dialog.setInput(embedabbleTypes.toArray());
        dialog.setLabelProvider(new LabelProvider() {
            @Override
            public Image getImage(Object element) {
                IResourceType resourceType = (IResourceType) element;
                return getImageFromFilename("dummy." + resourceType.getFileExtension());
            }

            @Override
            public String getText(Object element) {
                IResourceType resourceType = (IResourceType) element;
                return resourceType.getName();
            }
        });

        int ret = dialog.open();
        if (ret == Dialog.OK) {
            Object[] result = dialog.getResult();
            IResourceType resourceType = (IResourceType) result[0];
            Message templateMessage = resourceType.createTemplateMessage();
            String id = resourceType.getFileExtension();
            id = getUniqueId(id);
            EmbeddedComponentDesc embeddedComponentDesc = EmbeddedComponentDesc.newBuilder().setId(id).setData(TextFormat.printToString(templateMessage)).setType(resourceType.getFileExtension()).build();
            EmbeddedComponent embeddedComponent = new EmbeddedComponent(this, embeddedComponentDesc);
            AddComponentOperation op = new AddComponentOperation("Add " + resourceType.getName(), embeddedComponent);
            executeOperation(op);
        }
    }

    public void onRemoveComponent() {
        ISelection selection = componentsViewer.getSelection();
        if (!selection.isEmpty()) {
            Component component = (Component) ((IStructuredSelection) selection).getFirstElement();
            int index = allComponents.indexOf(component);
            assert index != -1;
            RemoveComponentOperation op = new RemoveComponentOperation("Remove " + component.getId(), index, component);
            executeOperation(op);
        }
    }

    void createComponentsComposite(Composite parent) {
        GridLayout layout = new GridLayout();
        layout.numColumns = 3;
        layout.marginWidth = 2;
        layout.marginHeight = 2;
        parent.setLayout(layout);

        ImageRegistry imageRegistry = Activator.getDefault().getImageRegistry();
        Button addResource = toolkit.createButton(parent, null, SWT.PUSH);
        addResource.setToolTipText("Add Component From Resource...");
        addResource.setImage(imageRegistry.get("link_add"));

        addResource.addSelectionListener(new SelectionListener() {
            @Override
            public void widgetSelected(SelectionEvent e) {
                onAddResourceComponent();
            }

            @Override
            public void widgetDefaultSelected(SelectionEvent e) {}
        });

        GridData gd = new GridData(GridData.VERTICAL_ALIGN_CENTER);
        gd.horizontalAlignment = SWT.LEFT;
        addResource.setLayoutData(gd);
        addResource.setAlignment(SWT.CENTER);

        Button add = toolkit.createButton(parent, null, SWT.PUSH);
        add.setToolTipText("Add Embedded Resource...");
        add.setImage(imageRegistry.get("page_add"));
        add.addSelectionListener(new SelectionListener() {

            @Override
            public void widgetSelected(SelectionEvent e) {
                onAddEmbeddedComponent();
            }

            @Override
            public void widgetDefaultSelected(SelectionEvent e) {}
        });

        gd = new GridData(GridData.VERTICAL_ALIGN_CENTER);
        gd.grabExcessHorizontalSpace = false;
        gd.horizontalAlignment = SWT.CENTER;
        add.setLayoutData(gd);
        add.setAlignment(SWT.CENTER);

        this.removeButton = toolkit.createButton(parent, null, SWT.PUSH);
        removeButton.setEnabled(false);
        removeButton.setToolTipText("Remove Component");
        removeButton.setImage(imageRegistry.get("delete"));
        removeButton.addSelectionListener(new SelectionListener() {

            @Override
            public void widgetSelected(SelectionEvent e) {
                onRemoveComponent();
            }

            @Override
            public void widgetDefaultSelected(SelectionEvent e) { }
        });
        gd = new GridData(GridData.VERTICAL_ALIGN_BEGINNING);
        gd.horizontalAlignment = SWT.LEFT;
        removeButton.setAlignment(SWT.CENTER);
        removeButton.setLayoutData(gd);

        Table t = toolkit.createTable(parent, SWT.NULL);
        gd = new GridData(GridData.FILL_BOTH);
        gd.heightHint = 20;
        gd.widthHint = 200;
        gd.horizontalSpan = 3;
        t.setLayoutData(gd);
        toolkit.paintBordersFor(parent);

        componentsViewer = new TableViewer(t);
        componentsViewer.setContentProvider(new IStructuredContentProvider() {

            @Override
            public void inputChanged(Viewer viewer, Object oldInput, Object newInput) {}

            @Override
            public void dispose() {}

            @Override
            public Object[] getElements(Object inputElement) {
                @SuppressWarnings("unchecked")
                List<Object> components = (List<Object>) inputElement;
                Object[] ret = new Object[components.size()];
                return components.toArray(ret);
            }
        });

        componentsViewer.setLabelProvider(new LabelProvider() {
            @Override
            public Image getImage(Object element) {
                if (element instanceof Component) {
                    Component component = (Component) element;
                    return component.getImage();
                }
                return super.getImage(element);
            }

            @Override
            public boolean isLabelProperty(Object element, String property) {
                if (element instanceof Component) {
                    return property.equals(Component.ID_PROPERTY) || property.equals(Component.COMPONENT_PROPERTY);
                } else {
                    return false;
                }
            }
        });

        componentsViewer.setSorter(new ViewerSorter() {
            @Override
            public boolean isSorterProperty(Object element, String property) {
                if (element instanceof Component) {
                    return property.equals(Component.ID_PROPERTY);
                } else {
                    return false;
                }
            }
        });
        componentsViewer.setColumnProperties(new String[] {"column1"});
        componentsViewer.setInput(this.allComponents);
        componentsViewer.addSelectionChangedListener(this);

        TextCellEditor cellEditor = new TextCellEditor(componentsViewer.getTable());
        cellEditor.addListener(this);
        componentsViewer.setCellEditors(new CellEditor[] {cellEditor});
        final CellModifier cellModifier = new CellModifier(this);
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

        componentsViewer.getTable().addMouseListener(new MouseAdapter() {
            @Override
            public void mouseDoubleClick(MouseEvent e) {
                ViewerCell cell = componentsViewer.getCell(new Point(e.x, e.y));
                if (cell == null)
                    return;

                Object element = cell.getElement();

                String resource = null;
                if (element instanceof ResourceComponent) {
                    ResourceComponent component = (ResourceComponent)element;
                    resource = component.getResource();
                }
                if (resource != null) {
                    IFile file = contentRoot.getFile(new Path(resource));
                    IWorkbenchPage page = PlatformUI.getWorkbench().getActiveWorkbenchWindow().getActivePage();
                    try {
                        IDE.openEditor(page, file);
                    } catch (PartInitException e1) {
                        e1.printStackTrace();
                    }
                }
            }
        });
    }

    void createDetailsComposite(Composite parent) {
        protoTreeEditor = new ProtoTreeEditor(parent, this.toolkit, this.contentRoot, this.undoContext);
        protoTreeEditor.addListener(this);
        protoTreeEditor.getTreeViewer().addFilter(new ViewerFilter() {
            @Override
            public boolean select(Viewer viewer, Object parentElement, Object element) {
                if (parentElement != null && parentElement instanceof MessageNode && element instanceof IPath) {
                    IPath path = (IPath)element;
                    if (path.getName().equals(Component.ID_PROPERTY)) {
                        return false;
                    }
                }
                return true;
            }
        });
    }

    @Override
    public void createPartControl(Composite parent) {
        toolkit = new FormToolkit(parent.getDisplay());

        form = toolkit.createForm(parent);

        IFileEditorInput input = (IFileEditorInput) getEditorInput();
        Image image = getImageFromFilename(input.getName());
        form.setImage(image);

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

        IContextService contextService = (IContextService) getSite().getService(IContextService.class);
        this.contextActivation = contextService.activateContext("com.dynamo.cr.ddfeditor.contexts.gameobjecteditor");
    }

    @Override
    public void setFocus() {
        this.form.getBody().setFocus();
    }

    public void updateActions() {
        IActionBars actionBars = getEditorSite().getActionBars();
        actionBars.updateActionBars();

        actionBars.setGlobalActionHandler(ActionFactory.UNDO.getId(), undoAction);
        actionBars.setGlobalActionHandler(ActionFactory.REDO.getId(), redoAction);
    }

    @Override
    public void historyNotification(OperationHistoryEvent event) {
        firePropertyChange(PROP_DIRTY);
    }

    @Override
    public void selectionChanged(SelectionChangedEvent event) {
        ISelection selection = event.getSelection();
        if (!selection.isEmpty()) {
            this.removeButton.setEnabled(true);
            IStructuredSelection structured = (IStructuredSelection) selection;
            Component component = (Component) structured.getFirstElement();
            protoTreeEditor.setInput(component.getEditableMessageNode(), component.resourceType);
        }
        else {
            this.removeButton.setEnabled(false);
            protoTreeEditor.setInput(null, null);
        }
    }

    public void addComponent(Component component) {
        this.allComponents.add(component);
        addId(component.getId());
        this.componentsViewer.refresh();
        this.componentsViewer.setSelection(new StructuredSelection(new Object[] {component}), true);
    }

    public void addComponent(int index, Component component) {
        this.allComponents.add(index, component);
        addId(component.getId());
        this.componentsViewer.refresh();
        this.componentsViewer.setSelection(new StructuredSelection(new Object[] {component}), true);
    }

    public void removeComponent(Component component) {
        this.allComponents.remove(component);
        removeId(component.getId());
        this.componentsViewer.refresh();
    }

    public void addId(String id) {
        this.idSet.add(id);
    }

    public void removeId(String id) {
        this.idSet.remove(id);
    }

    public boolean isIdUsed(String id) {
        return this.idSet.contains(id);
    }

    public String getUniqueId(String baseId) {
        if (isIdUsed(baseId)) {
            int i = 0;
            String id = String.format("%s%d", baseId, i);
            while (isIdUsed(id)) {
                ++i;
                id = String.format("%s%d", baseId, i);
            }
            return id;
        } else {
            return baseId;
        }
    }

    void activateContext() {
        IContextService contextService = (IContextService) getSite().getService(IContextService.class);
        contextActivation = contextService.activateContext("com.dynamo.cr.ddfeditor.contexts.gameobjecteditor");
    }

    void deactivateContext() {
        IContextService contextService = (IContextService) getSite().getService(IContextService.class);
        contextService.deactivateContext(this.contextActivation);
    }

    @Override
    public void applyEditorValue() {
        activateContext();
    }

    @Override
    public void cancelEditor() {
        deactivateContext();
    }

    @Override
    public void editorValueChanged(boolean oldValidState, boolean newValidState) {
    }

    public void componentChanged(Component component, String[] properties) {
        this.componentsViewer.update(component, properties);
        this.protoTreeEditor.getTreeViewer().refresh();
    }

    @Override
    public void fieldChanged(MessageNode message, IPath field) {
        for (Component component : this.allComponents) {
            if (component.getEditableMessageNode() == message) {
                if (field.getName().equals(Component.ID_PROPERTY)) {
                    component.setId((String)message.getField(field));
                } else {
                    this.componentsViewer.update(component, new String[] {field.getName()});
                }
            }
        }
    }
}
