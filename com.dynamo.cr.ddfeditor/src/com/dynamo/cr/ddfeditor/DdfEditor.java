package com.dynamo.cr.ddfeditor;

import java.io.ByteArrayInputStream;
import java.io.IOException;
import java.io.InputStreamReader;
import java.io.Reader;
import java.util.List;
import java.util.Map;

import org.eclipse.core.commands.ExecutionException;
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
import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Status;
import org.eclipse.jface.dialogs.ErrorDialog;
import org.eclipse.jface.viewers.CellEditor;
import org.eclipse.jface.viewers.ColumnLabelProvider;
import org.eclipse.jface.viewers.DialogCellEditor;
import org.eclipse.jface.viewers.EditingSupport;
import org.eclipse.jface.viewers.ISelection;
import org.eclipse.jface.viewers.IStructuredSelection;
import org.eclipse.jface.viewers.ITableLabelProvider;
import org.eclipse.jface.viewers.ITreeContentProvider;
import org.eclipse.jface.viewers.LabelProvider;
import org.eclipse.jface.viewers.TextCellEditor;
import org.eclipse.jface.viewers.TreeViewer;
import org.eclipse.jface.viewers.TreeViewerColumn;
import org.eclipse.jface.viewers.Viewer;
import org.eclipse.swt.SWT;
import org.eclipse.swt.graphics.Image;
import org.eclipse.swt.widgets.Composite;
import org.eclipse.swt.widgets.Control;
import org.eclipse.swt.widgets.Display;
import org.eclipse.swt.widgets.Event;
import org.eclipse.swt.widgets.Listener;
import org.eclipse.swt.widgets.Menu;
import org.eclipse.swt.widgets.MenuItem;
import org.eclipse.ui.IActionBars;
import org.eclipse.ui.IEditorInput;
import org.eclipse.ui.IEditorSite;
import org.eclipse.ui.IFileEditorInput;
import org.eclipse.ui.ISharedImages;
import org.eclipse.ui.PartInitException;
import org.eclipse.ui.PlatformUI;
import org.eclipse.ui.actions.ActionFactory;
import org.eclipse.ui.dialogs.ListDialog;
import org.eclipse.ui.dialogs.ResourceListSelectionDialog;
import org.eclipse.ui.operations.LinearUndoViolationUserApprover;
import org.eclipse.ui.operations.RedoActionHandler;
import org.eclipse.ui.operations.UndoActionHandler;
import org.eclipse.ui.part.EditorPart;

import com.dynamo.cr.ddfeditor.operations.AddRepeatedOperation;
import com.dynamo.cr.ddfeditor.operations.RemoveRepeatedOperation;
import com.dynamo.cr.ddfeditor.operations.SetFieldOperation;
import com.dynamo.cr.protobind.IPath;
import com.dynamo.cr.protobind.MessageNode;
import com.dynamo.cr.protobind.Node;
import com.dynamo.cr.protobind.PathElement;
import com.dynamo.cr.protobind.RepeatedNode;
import com.google.protobuf.Descriptors.FieldDescriptor;
import com.google.protobuf.Message;
import com.google.protobuf.TextFormat;

public abstract class DdfEditor extends EditorPart implements IOperationHistoryListener, Listener {

    class ProtoContentProvider implements ITreeContentProvider {

        @Override
        public void dispose() {}

        @Override
        public void inputChanged(Viewer viewer, Object oldInput, Object newInput) {}

        @Override
        public Object[] getElements(Object inputElement) {
            if (inputElement instanceof MessageNode) {
                MessageNode message = (MessageNode) inputElement;
                return message.getAllPaths();
            }
            return null;
        }

        @Override
        public Object[] getChildren(Object parentElement) {
            if (parentElement instanceof IPath) {
                IPath fieldPath = (IPath) parentElement;
                Object value = message.getField(fieldPath);
                if (value instanceof Node) {
                    Node node = (Node) value;
                    return node.getAllPaths();
                }
            }
            return null;
        }

        @Override
        public Object getParent(Object element) {
            return null;
        }

        @Override
        public boolean hasChildren(Object element) {
            if (element instanceof IPath) {
                IPath fieldPath = (IPath) element;
                Object value = message.getField(fieldPath);
                if (value instanceof Node) {
                    Node node = (Node) value;
                    return node.getAllPaths().length > 0;
                }
            }
            return false;
        }
    }

    class ProtoLabelProvider extends LabelProvider implements ITableLabelProvider {
        public String getColumnText(Object obj, int index) {
            return getText(obj);
        }

        public Image getColumnImage(Object obj, int index) {
            return getImage(obj);
        }

        public Image getImage(Object obj) {
            return PlatformUI.getWorkbench().getSharedImages()
                    .getImage(ISharedImages.IMG_OBJ_ELEMENT);
        }
    }

    class ProtoFieldValueLabelProvider extends ColumnLabelProvider {
        public String getText(Object element) {
            if (element instanceof IPath) {
                IPath fieldPath = (IPath) element;

                Object fieldValue = message.getField(fieldPath);
                if (fieldValue instanceof Node) {
                    return "";
                }
                else {
                    return fieldValue.toString();
                }
            }
            return element.toString();
        }
    }

    class ResourceDialogCellEditor extends DialogCellEditor {

        public ResourceDialogCellEditor(Composite parent) {
            super(parent, SWT.NONE);
        }

        @Override
        protected Object openDialogBox(Control cellEditorWindow) {

            IFileEditorInput fi = (IFileEditorInput) getEditorInput();
            IContainer contentRoot = findContentRoot(fi.getFile());
            if (contentRoot == null) {
                return null;
            }

            ResourceListSelectionDialog dialog = new ResourceListSelectionDialog(getSite().getShell(), contentRoot, IResource.FILE | IResource.DEPTH_INFINITE);

            int ret = dialog.open();

            if (ret == ListDialog.OK)
            {
                IResource r = (IResource) dialog.getResult()[0];
                org.eclipse.core.runtime.IPath fullPath = r.getFullPath();
                return fullPath.makeRelativeTo(contentRoot.getFullPath()).toPortableString();
            }
            return null;
        }
    }

    class ProtoFieldEditingSupport extends EditingSupport {

        private TextCellEditor textEditor;
        private ResourceDialogCellEditor resourceEditor;

        public ProtoFieldEditingSupport(TreeViewer viewer) {
            super(viewer);
            textEditor = new TextCellEditor(viewer.getTree());
            resourceEditor = new ResourceDialogCellEditor(viewer.getTree());
        }

        @Override
        protected CellEditor getCellEditor(Object element) {
            IPath fieldPath = (IPath) element;
            PathElement lastElement = fieldPath.lastElement();

            FieldDescriptor fieldDescriptor = lastElement.fieldDescriptor;
            if (isResource(fieldDescriptor)) {
                return resourceEditor;
            }
            else {
                return textEditor;
            }
        }

        @Override
        protected boolean canEdit(Object element) {
            if (element instanceof IPath) {
                IPath fieldPath = (IPath) element;
                Object value = message.getField(fieldPath);
                // Currently only support for string and number editing
                if (value instanceof String) {
                    return true;
                }
                else if (value instanceof Number) {
                    return true;
                }
            }
            return false;
        }

        boolean isResource(FieldDescriptor fieldDescriptor) {
            Map<FieldDescriptor, Object> allFields = fieldDescriptor.getOptions().getAllFields();
            for (FieldDescriptor fd : allFields.keySet()) {
                if (fd.getName().equals("resource")) {
                    Boolean b = (Boolean) allFields.get(fd);
                    return b.booleanValue();
                }
            }
            return false;
        }

        @Override
        protected Object getValue(Object element) {
            IPath fieldPath = (IPath) element;
            return message.getField(fieldPath).toString();
        }

        Object coerceNumber(Object template, Object value) {
            if (template instanceof Integer) {
                return new Integer(value.toString());
            }
            else if (template instanceof Float) {
                return new Float(value.toString());
            }
            else if (template instanceof Double) {
                return new Double(value.toString());
            }

            throw new RuntimeException("Unknown number type: " + template);
        }

        @Override
        protected void setValue(Object element, Object value) {
            IPath fieldPath = (IPath) element;
            Object oldValue = message.getField(fieldPath);

            if (oldValue instanceof Number) {
                try {
                    value = coerceNumber(oldValue, value);
                }
                catch (NumberFormatException e) {
                    // Ignore format errors.
                    return;
                }
            }

            if (oldValue.equals(value))
                return;

            SetFieldOperation op = new SetFieldOperation(viewer, message, fieldPath, oldValue, value, element);
            executeOperation(op);
            getViewer().update(element, null);
        }
    }

    private TreeViewer viewer;
    private MessageNode message;
    private Message messageType;
    private UndoContext undoContext;
    private UndoActionHandler undoAction;
    private RedoActionHandler redoAction;
    private int cleanUndoStackDepth = 0;
    private Menu menu;

    public DdfEditor(Message messageType) {
        this.messageType = messageType;
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

    @Override
    public void doSave(IProgressMonitor monitor) {
        IFileEditorInput input = (IFileEditorInput) getEditorInput();

        String messageString = TextFormat.printToString(message.build());
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

        try {
            Reader reader = new InputStreamReader(file.getContents());

            Message.Builder builder = messageType.newBuilderForType();
            try {
                try {
                    TextFormat.merge(reader, builder);

                    MessageNode message = new MessageNode(builder.build());
                    this.message = message;
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

    @Override
    public void createPartControl(Composite parent) {
        viewer = new TreeViewer(parent, SWT.BORDER | SWT.FULL_SELECTION);

        ProtoLabelProvider labelProvider = new ProtoLabelProvider();
        viewer.setContentProvider(new ProtoContentProvider());
        viewer.setLabelProvider(labelProvider);
        viewer.getTree().setHeaderVisible(true);
        viewer.getTree().setLinesVisible(true);

        TreeViewerColumn nameColumn = new TreeViewerColumn(viewer, SWT.NONE);
        nameColumn.getColumn().setWidth(140);
        nameColumn.getColumn().setMoveable(true);
        nameColumn.getColumn().setText("Property");
        nameColumn.setLabelProvider(new ColumnLabelProvider() {
            @Override
            public String getText(Object element) {
                if (element instanceof IPath) {
                    IPath fieldPath = (IPath) element;
                    return fieldPath.getName();
                }
                return super.getText(element);
            }
        });

        ColumnLabelProvider propertyValueLabelProvider = new ProtoFieldValueLabelProvider();
        TreeViewerColumn valueColumn = new TreeViewerColumn(viewer, SWT.NONE);
        valueColumn.getColumn().setWidth(240);
        valueColumn.getColumn().setMoveable(true);
        valueColumn.getColumn().setText("Value");
        valueColumn.setLabelProvider(propertyValueLabelProvider);

        valueColumn.setEditingSupport(new ProtoFieldEditingSupport(viewer));

        viewer.setInput(message);

        this.menu = new Menu(parent.getShell(), SWT.POP_UP);
        this.menu.addListener(SWT.Show, this);
        viewer.getTree().setMenu(menu);
    }

    @Override
    public void setFocus() {
        viewer.getTree().setFocus();

        IActionBars action_bars = getEditorSite().getActionBars();
        action_bars.updateActionBars();

        action_bars.setGlobalActionHandler(ActionFactory.UNDO.getId(), undoAction);
        action_bars.setGlobalActionHandler(ActionFactory.REDO.getId(), redoAction);
    }

    @Override
    public void historyNotification(OperationHistoryEvent event) {
        firePropertyChange(PROP_DIRTY);
    }

    public boolean hasNewValueForPath(String path) {
        return false;
    }

    public Object getNewValueForPath(String path) {
        return null;
    }

    // TODO: MOVE?
    // Similar function in LoaderFactory!
    public IContainer findContentRoot(IFile file) {
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

    @Override
    public void handleEvent(Event event) {
        if (event.type == SWT.Show) {
            MenuItem [] menuItems = this.menu.getItems ();
            for (int i=0; i<menuItems.length; i++) {
                menuItems[i].removeListener(SWT.Selection, this);
                menuItems[i].dispose();
            }

            /*
            TreeItem treeItem = viewer.getTree().getItem(new Point(event.x, event.y));
            Object data = treeItem.getData();
            System.out.println("!!!!!!!!!!: " + data + ", " + treeItem);
            */
            ISelection selection = viewer.getSelection();

            if (!selection.isEmpty()) {
                IStructuredSelection structSelection = (IStructuredSelection) selection;
                Object selected = structSelection.getFirstElement();

                if (selected instanceof IPath) {
                    IPath path = (IPath) selected;
                    Object value = message.getField(path);

                    if (value instanceof RepeatedNode) {
                        RepeatedNode repeatedNode = (RepeatedNode) value;
                        FieldDescriptor fieldDescriptor = repeatedNode.getFieldDescriptor();

                        if (hasNewValueForPath(path.toString())) {
                            MenuItem menuItem = new MenuItem (this.menu, SWT.PUSH);
                            menuItem.addListener(SWT.Selection, this);
                            String name = fieldDescriptor.getName();
                            if (name.endsWith("s")) {
                                name = name.substring(0, name.length()-1);
                            }
                            menuItem.setText("Add " + name + "...");
                            menuItem.setData("operation", "add");
                            menuItem.setData("path", path);
                        }
                    }

                    if (path.elementCount() > 0 && path.lastElement().isIndex()) {
                        MenuItem menuItem = new MenuItem (this.menu, SWT.PUSH);
                        menuItem.addListener(SWT.Selection, this);
                        menuItem.setText("Remove");
                        menuItem.setData("operation", "remove");
                        menuItem.setData("path", path);
                    }

                }
            }
        }
        else if (event.type == SWT.Selection) {
            String operation = (String) event.widget.getData("operation");
            IPath path = (IPath) event.widget.getData("path");

            if (operation.equals("add")) {
                Object newValue = getNewValueForPath(path.toString());
                if (newValue == null)
                    return;

                RepeatedNode oldRepeated = (RepeatedNode) message.getField(path);
                List<Object> oldList = oldRepeated.getValueList();
                AddRepeatedOperation op = new AddRepeatedOperation(viewer, message, path, oldList, newValue);
                executeOperation(op);
            }
            else if (operation.equals("remove")) {
                RepeatedNode oldRepeated = (RepeatedNode) message.getField(path.getParent());
                List<Object> oldList = oldRepeated.getValueList();
                RemoveRepeatedOperation op = new RemoveRepeatedOperation(viewer, message, path, oldList);
                executeOperation(op);
            }
        }
    }
}
