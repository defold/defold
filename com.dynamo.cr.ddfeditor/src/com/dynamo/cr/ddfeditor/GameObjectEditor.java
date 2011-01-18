package com.dynamo.cr.ddfeditor;

import java.io.ByteArrayInputStream;
import java.io.IOException;
import java.io.InputStreamReader;
import java.io.Reader;
import java.io.StringReader;
import java.util.ArrayList;
import java.util.List;

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
import org.eclipse.core.resources.IFolder;
import org.eclipse.core.resources.IResource;
import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.IAdaptable;
import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Status;
import org.eclipse.jface.dialogs.Dialog;
import org.eclipse.jface.dialogs.ErrorDialog;
import org.eclipse.jface.resource.ImageRegistry;
import org.eclipse.jface.viewers.ArrayContentProvider;
import org.eclipse.jface.viewers.ISelection;
import org.eclipse.jface.viewers.ISelectionChangedListener;
import org.eclipse.jface.viewers.IStructuredContentProvider;
import org.eclipse.jface.viewers.IStructuredSelection;
import org.eclipse.jface.viewers.LabelProvider;
import org.eclipse.jface.viewers.SelectionChangedEvent;
import org.eclipse.jface.viewers.TableViewer;
import org.eclipse.jface.viewers.Viewer;
import org.eclipse.swt.SWT;
import org.eclipse.swt.events.SelectionEvent;
import org.eclipse.swt.events.SelectionListener;
import org.eclipse.swt.graphics.Image;
import org.eclipse.swt.layout.FillLayout;
import org.eclipse.swt.layout.GridData;
import org.eclipse.swt.layout.GridLayout;
import org.eclipse.swt.widgets.Button;
import org.eclipse.swt.widgets.Composite;
import org.eclipse.swt.widgets.Display;
import org.eclipse.swt.widgets.Table;
import org.eclipse.ui.IActionBars;
import org.eclipse.ui.IEditorInput;
import org.eclipse.ui.IEditorSite;
import org.eclipse.ui.IFileEditorInput;
import org.eclipse.ui.PartInitException;
import org.eclipse.ui.PlatformUI;
import org.eclipse.ui.actions.ActionFactory;
import org.eclipse.ui.dialogs.ListDialog;
import org.eclipse.ui.dialogs.ResourceListSelectionDialog;
import org.eclipse.ui.forms.widgets.Form;
import org.eclipse.ui.forms.widgets.FormToolkit;
import org.eclipse.ui.forms.widgets.Section;
import org.eclipse.ui.operations.LinearUndoViolationUserApprover;
import org.eclipse.ui.operations.RedoActionHandler;
import org.eclipse.ui.operations.UndoActionHandler;
import org.eclipse.ui.part.EditorPart;

import com.dynamo.cr.editor.core.EditorCorePlugin;
import com.dynamo.cr.editor.core.IResourceType;
import com.dynamo.cr.editor.core.IResourceTypeRegistry;
import com.dynamo.cr.protobind.MessageNode;
import com.dynamo.gameobject.proto.GameObject.ComponentDesc;
import com.dynamo.gameobject.proto.GameObject.EmbeddedComponentDesc;
import com.dynamo.gameobject.proto.GameObject.PrototypeDesc;
import com.google.protobuf.Descriptors.Descriptor;
import com.google.protobuf.DynamicMessage;
import com.google.protobuf.Message;
import com.google.protobuf.TextFormat;

public class GameObjectEditor extends EditorPart implements IOperationHistoryListener, ISelectionChangedListener {

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

    class AddComponentOperation extends AbstractOperation {
        private ArrayList<Component> prevAllComponents;
        private ArrayList<Component> newAllCompononents;

        public AddComponentOperation(String label, Component component) {
            super(label);
            this.prevAllComponents = allComponents;
            this.newAllCompononents = new ArrayList<Component>(allComponents);
            this.newAllCompononents.add(component);
        }

        @Override
        public IStatus execute(IProgressMonitor monitor, IAdaptable info)
                throws ExecutionException {
            allComponents = newAllCompononents;
            componentsViewer.setInput(allComponents);
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
            allComponents = this.prevAllComponents;
            componentsViewer.setInput(allComponents);
            return Status.OK_STATUS;
        }
    }

    class RemoveComponentOperation extends AbstractOperation {
        private ArrayList<Component> prevAllComponents;
        private ArrayList<Component> newAllCompononents;

        public RemoveComponentOperation(String label, int index) {
            super(label);
            this.prevAllComponents = allComponents;
            this.newAllCompononents = new ArrayList<Component>(allComponents);
            this.newAllCompononents.remove(index);
        }

        @Override
        public IStatus execute(IProgressMonitor monitor, IAdaptable info)
                throws ExecutionException {
            allComponents = newAllCompononents;
            componentsViewer.setInput(allComponents);
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
            allComponents = this.prevAllComponents;
            componentsViewer.setInput(allComponents);
            return Status.OK_STATUS;
        }
    }

    static abstract class Component {
        public abstract String getExtension();
        public abstract MessageNode getEditableMessageNode();
        public abstract Message getSavableMessage();
    }

    static class ResourceComponent extends Component {

        private ComponentDesc desc;
        private MessageNode messageNode;

        public ResourceComponent(ComponentDesc desc) {
            this.desc = desc;
            this.messageNode = new MessageNode(desc);
        }

        @Override
        public String toString() {
            return desc.getResource();
        }

        @Override
        public String getExtension() {
            int index = desc.getResource().lastIndexOf(".");
            return desc.getResource().substring(index);
        }

        @Override
        public MessageNode getEditableMessageNode() {
            return messageNode;
        }

        @Override
        public Message getSavableMessage() {
            return messageNode.build();
        }
    }

    static class EmbeddedComponent extends Component {

        private EmbeddedComponentDesc descriptor;
        private MessageNode messageNode;

        public EmbeddedComponent(EmbeddedComponentDesc desc) {
            this.descriptor = desc;


            IResourceTypeRegistry registry = EditorCorePlugin.getDefault().getResourceTypeRegistry();
            IResourceType resourceType = registry.getResourceTypeFromExtension(descriptor.getType());
            Descriptor protoDescriptor = resourceType.getMessageDescriptor();

            //Descriptor protoDescriptor = ProtoFactory.getDescriptorForExtension(descriptor.getType());
            if (protoDescriptor == null) {
                throw new RuntimeException(String.format("Unknown type: %s", descriptor.getType()));
            }
            DynamicMessage.Builder builder = DynamicMessage.newBuilder(protoDescriptor);

            try {
                TextFormat.merge(new StringReader(descriptor.getData()), builder);
                this.messageNode = new MessageNode(builder.build());
            } catch (IOException e) {
                throw new RuntimeException(e);
            }
        }

        @Override
        public String toString() {
            return descriptor.getType() + " (embedded)";
        }

        @Override
        public String getExtension() {
            return "." + descriptor.getType();
        }

        @Override
        public MessageNode getEditableMessageNode() {
            return this.messageNode;
        }

        @Override
        public Message getSavableMessage() {
            String data = TextFormat.printToString(this.messageNode.build());
            return EmbeddedComponentDesc.newBuilder().setType(descriptor.getType()).setData(data).build();
        }

    }

    public void executeOperation(IUndoableOperation operation) {
        IOperationHistory history = PlatformUI.getWorkbench().getOperationSupport().getOperationHistory();
        operation.addContext(undoContext);
        try
        {
            history.execute(operation, null, null);
        } catch (ExecutionException e)
        {
            e.printStackTrace();
        }
    }

    public GameObjectEditor() {
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

        this.contentRoot = findContentRoot(file);

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
                        this.allComponents.add(new ResourceComponent(componentDesc));
                    }
                    for (EmbeddedComponentDesc embeddedComponentDesc : embeddedComponents) {
                        this.allComponents.add(new EmbeddedComponent(embeddedComponentDesc));
                    }
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

                ResourceListSelectionDialog dialog = new ResourceListSelectionDialog(getSite().getShell(), contentRoot, IResource.FILE | IResource.DEPTH_INFINITE);
                dialog.setTitle("Add Resource Component");

                int ret = dialog.open();
                if (ret == Dialog.OK) {
                    IResource r = (IResource) dialog.getResult()[0];

                    org.eclipse.core.runtime.IPath fullPath = r.getFullPath();
                    String relPath = fullPath.makeRelativeTo(contentRoot.getFullPath()).toPortableString();

                    ComponentDesc componentDesc = ComponentDesc.newBuilder().setResource(relPath).build();
                    ResourceComponent resourceComponent = new ResourceComponent(componentDesc);
                    AddComponentOperation op = new AddComponentOperation("Add " + r.getName(), resourceComponent);
                    executeOperation(op);
                }
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

                IResourceTypeRegistry registry = EditorCorePlugin.getDefault().getResourceTypeRegistry();
                IResourceType[] resourceTypes = registry.getResourceTypes();
                List<IResourceType> embedabbleTypes = new ArrayList<IResourceType>();
                for (IResourceType t : resourceTypes) {
                    if (t.isEmbeddable()) {
                        embedabbleTypes.add(t);
                    }
                }

                ListDialog dialog = new ListDialog(getSite().getShell());
                dialog.setTitle("Add Embeded Resource");
                dialog.setMessage("Select resource type from list to embed:");
                dialog.setContentProvider(new ArrayContentProvider());
                dialog.setInput(embedabbleTypes.toArray());
                dialog.setLabelProvider(new LabelProvider() {
                    @Override
                    public Image getImage(Object element) {
                        IResourceType resourceType = (IResourceType) element;
                        return PlatformUI.getWorkbench().getEditorRegistry().getImageDescriptor("dummy." + resourceType.getFileExtension()).createImage();
                    }
                });

                int ret = dialog.open();
                if (ret == Dialog.OK) {
                    Object[] result = dialog.getResult();
                    IResourceType resourceType = (IResourceType) result[0];
                    Message templateMessage = resourceType.createTemplateMessage();
                    EmbeddedComponentDesc embeddedComponentDesc = EmbeddedComponentDesc.newBuilder().setData(TextFormat.printToString(templateMessage)).setType(resourceType.getFileExtension()).build();
                    EmbeddedComponent embeddedComponent = new EmbeddedComponent(embeddedComponentDesc);
                    AddComponentOperation op = new AddComponentOperation("Add " + resourceType.getName(), embeddedComponent);
                    executeOperation(op);
                }
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
        removeButton.setToolTipText("Remove Resource");
        removeButton.setImage(imageRegistry.get("delete"));
        removeButton.addSelectionListener(new SelectionListener() {

            @Override
            public void widgetSelected(SelectionEvent e) {
                ISelection selection = componentsViewer.getSelection();
                if (!selection.isEmpty()) {
                    Component component = (Component) ((IStructuredSelection) selection).getFirstElement();
                    int index = allComponents.indexOf(component);
                    assert index != -1;
                    // TODO: Better description for undo/redo action? (ie Remove X)
                    RemoveComponentOperation op = new RemoveComponentOperation("Remove Component", index);
                    executeOperation(op);
                }
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
                    return PlatformUI.getWorkbench().getEditorRegistry().getImageDescriptor(component.getExtension()).createImage();
                }
                return super.getImage(element);
            }
        });

        componentsViewer.setInput(this.allComponents);
        componentsViewer.addSelectionChangedListener(this);
    }

    // TODO: MOVE?
    // Similar function in LoaderFactory!
    private IContainer findContentRoot(IFile file) {
        IContainer c = file.getParent();
        while (c != null) {
            if (c instanceof IFolder) {
                IFolder folder = (IFolder) c;
                IFile f = folder.getFile("game.project");
                if (f.exists()) {
                    return c;
                }
            }
            c = c.getParent();
        }
        return null;
    }

    void createDetailsComposite(Composite parent) {
        protoTreeEditor = new ProtoTreeEditor(parent, this.toolkit, this.contentRoot, this.undoContext);
    }

    @Override
    public void createPartControl(Composite parent) {
        toolkit = new FormToolkit(parent.getDisplay());

        form = toolkit.createForm(parent);

        IFileEditorInput input = (IFileEditorInput) getEditorInput();
        Image image = PlatformUI.getWorkbench().getEditorRegistry().getImageDescriptor(input.getName()).createImage();
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
    }

    @Override
    public void setFocus() {
        this.form.getBody().setFocus();
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
            protoTreeEditor.setInput(component.getEditableMessageNode());
        }
        else {
            this.removeButton.setEnabled(false);
            protoTreeEditor.setInput(null);
        }
    }
}

