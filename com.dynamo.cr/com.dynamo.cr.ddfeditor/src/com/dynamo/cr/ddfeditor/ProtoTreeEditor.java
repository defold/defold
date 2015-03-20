package com.dynamo.cr.ddfeditor;

import java.util.ArrayList;
import java.util.List;
import java.util.Map;

import org.eclipse.core.commands.ExecutionException;
import org.eclipse.core.commands.operations.IOperationHistory;
import org.eclipse.core.commands.operations.IUndoableOperation;
import org.eclipse.core.commands.operations.UndoContext;
import org.eclipse.core.resources.IContainer;
import org.eclipse.core.resources.IResource;
import org.eclipse.jface.viewers.CellEditor;
import org.eclipse.jface.viewers.ColumnLabelProvider;
import org.eclipse.jface.viewers.ComboBoxCellEditor;
import org.eclipse.jface.viewers.DialogCellEditor;
import org.eclipse.jface.viewers.EditingSupport;
import org.eclipse.jface.viewers.ISelection;
import org.eclipse.jface.viewers.IStructuredSelection;
import org.eclipse.jface.viewers.LabelProvider;
import org.eclipse.jface.viewers.TextCellEditor;
import org.eclipse.jface.viewers.TreeViewer;
import org.eclipse.jface.viewers.TreeViewerColumn;
import org.eclipse.swt.SWT;
import org.eclipse.swt.custom.CCombo;
import org.eclipse.swt.layout.GridData;
import org.eclipse.swt.layout.GridLayout;
import org.eclipse.swt.widgets.Composite;
import org.eclipse.swt.widgets.Control;
import org.eclipse.swt.widgets.Event;
import org.eclipse.swt.widgets.Listener;
import org.eclipse.swt.widgets.Menu;
import org.eclipse.swt.widgets.MenuItem;
import org.eclipse.swt.widgets.Tree;
import org.eclipse.ui.PlatformUI;
import org.eclipse.ui.dialogs.ListDialog;
import org.eclipse.ui.dialogs.ResourceListSelectionDialog;
import org.eclipse.ui.forms.widgets.FormToolkit;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import com.dynamo.cr.ddfeditor.operations.AddRepeatedOperation;
import com.dynamo.cr.ddfeditor.operations.RemoveRepeatedOperation;
import com.dynamo.cr.ddfeditor.operations.SetFieldOperation;
import com.dynamo.cr.editor.core.EditorUtil;
import com.dynamo.cr.editor.core.IResourceType;
import com.dynamo.cr.editor.core.IResourceTypeEditSupport;
import com.dynamo.cr.protobind.IPath;
import com.dynamo.cr.protobind.MessageNode;
import com.dynamo.cr.protobind.Node;
import com.dynamo.cr.protobind.PathElement;
import com.dynamo.cr.protobind.RepeatedNode;
import com.google.protobuf.Descriptors.EnumDescriptor;
import com.google.protobuf.Descriptors.EnumValueDescriptor;
import com.google.protobuf.Descriptors.FieldDescriptor;
import com.google.protobuf.Descriptors.FieldDescriptor.JavaType;
import com.google.protobuf.Descriptors.FieldDescriptor.Type;

public class ProtoTreeEditor implements Listener {

    private static Logger logger = LoggerFactory.getLogger(ProtoTreeEditor.class);
    private TreeViewer treeViewer;
    private IContainer contentRoot;
    private UndoContext undoContext;
    private IResourceType resourceType;
    private Menu menu;
    private ArrayList<IProtoListener> listeners;

    class ResourceDialogCellEditor extends DialogCellEditor {

        public ResourceDialogCellEditor(Composite parent) {
            super(parent, SWT.NONE);
        }

        @Override
        protected Object openDialogBox(Control cellEditorWindow) {

            ResourceListSelectionDialog dialog = new ResourceListSelectionDialog(getControl().getShell(), contentRoot, IResource.FILE | IResource.DEPTH_INFINITE);

            int ret = dialog.open();

            if (ret == ListDialog.OK)
            {
                IResource r = (IResource) dialog.getResult()[0];
                return EditorUtil.makeResourcePath(r);
            }
            return null;
        }
    }

    class EnumCellEditor extends ComboBoxCellEditor {
        public EnumCellEditor(Composite parent) {
            super(parent, new String[] {}, SWT.READ_ONLY);
        }

        @Override
        protected Control createControl(Composite parent) {
            // TODO Auto-generated method stub
            Control control = super.createControl(parent);
            CCombo combo = (CCombo)control;
            combo.setVisibleItemCount(10);
            return control;
        }

        public void setEnumDescriptor(EnumDescriptor enumDescriptor) {
            List<EnumValueDescriptor> values = enumDescriptor.getValues();
            List<String> itemList = new ArrayList<String>(values.size());
            for (EnumValueDescriptor enumValue : values) {
                itemList.add(enumValue.getName());
            }
            String[] items = new String[itemList.size()];
            setItems(itemList.toArray(items));
        }
    }

    class ProtoFieldEditingSupport extends EditingSupport {

        private TextCellEditor textEditor;
        private ResourceDialogCellEditor resourceEditor;
        private EnumCellEditor enumEditor;
        private ProtoTreeEditor editor;

        public ProtoFieldEditingSupport(ProtoTreeEditor editor, TreeViewer viewer) {
            super(viewer);
            textEditor = new TextCellEditor(viewer.getTree());
            resourceEditor = new ResourceDialogCellEditor(treeViewer.getTree());
            enumEditor = new EnumCellEditor(viewer.getTree());
            this.editor = editor;
        }

        @Override
        protected CellEditor getCellEditor(Object element) {
            IPath fieldPath = (IPath) element;
            PathElement lastElement = fieldPath.lastElement();

            FieldDescriptor fieldDescriptor = lastElement.fieldDescriptor;
            if (fieldDescriptor.getType() == Type.ENUM) {
                enumEditor.setEnumDescriptor(fieldDescriptor.getEnumType());
                return enumEditor;
            } else if (fieldDescriptor.getType() == Type.BOOL) {
                return textEditor;
            } else if (isResource(fieldDescriptor)) {
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
                MessageNode message = (MessageNode) treeViewer.getInput();
                Object value = message.getField(fieldPath);
                // Currently only support for string and number editing
                if (value instanceof String) {
                    return true;
                }
                else if (value instanceof EnumValueDescriptor) {
                    return true;
                }
                else if (value instanceof Number) {
                    return true;
                }
                else if (value instanceof Boolean) {
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

            MessageNode message = (MessageNode) treeViewer.getInput();
            Object value = message.getField(fieldPath);
            PathElement lastElement = fieldPath.lastElement();
            if (lastElement.fieldDescriptor.getType() == Type.ENUM) {
                if (value instanceof EnumValueDescriptor) {
                    EnumValueDescriptor enumValueDesc = (EnumValueDescriptor) value;
                    return enumValueDesc.getNumber();
                }
                else {
                    return value;
                }
            }
            else {
                return value.toString();
            }
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

            MessageNode message = (MessageNode) treeViewer.getInput();
            Object oldValue = message.getField(fieldPath);

            if (value instanceof Number && fieldPath.lastElement().fieldDescriptor.getType() == Type.ENUM) {
                EnumDescriptor enumDesc = fieldPath.lastElement().fieldDescriptor.getEnumType();
                value = enumDesc.findValueByNumber((Integer) value);
            }
            else if (fieldPath.lastElement().fieldDescriptor.getType() == Type.BOOL) {
                String s = (String)value;
                if (s.toLowerCase().equals("true")) {
                    value = true;
                } else {
                    value = false;
                }
            }
            else if (oldValue instanceof Number) {
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

            SetFieldOperation op = new SetFieldOperation(this.editor, treeViewer, message, fieldPath, oldValue, value);
            executeOperation(op);
            getViewer().update(element, null);
        }
    }

    public ProtoTreeEditor(Composite parent, FormToolkit toolkit, IContainer contentRoot, UndoContext undoContext) {
        this.contentRoot = contentRoot;
        this.undoContext = undoContext;
        GridLayout layout = new GridLayout();
        layout.numColumns = 2;
        layout.marginWidth = 2;
        layout.marginHeight = 2;
        parent.setLayout(layout);
        Tree t = toolkit.createTree(parent, SWT.BORDER | SWT.FULL_SELECTION);

        treeViewer = new TreeViewer(t);
        treeViewer.setContentProvider(new ProtoContentProvider());
        treeViewer.setLabelProvider(new LabelProvider());

        treeViewer.getTree().setHeaderVisible(true);
        treeViewer.getTree().setLinesVisible(true);

        TreeViewerColumn nameColumn = new TreeViewerColumn(treeViewer, SWT.NONE);
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

        TreeViewerColumn valueColumn = new TreeViewerColumn(treeViewer, SWT.NONE);
        valueColumn.setEditingSupport(new ProtoFieldEditingSupport(this, treeViewer));

        valueColumn.setLabelProvider(new ColumnLabelProvider() {
            @Override
            public String getText(Object element) {
                if (element instanceof IPath) {
                    IPath fieldPath = (IPath) element;

                    MessageNode message = (MessageNode) treeViewer.getInput();
                    Object fieldValue = message.getField(fieldPath);

                    if (fieldValue instanceof Node) {
                        if (fieldValue instanceof MessageNode) {
                            MessageNode messageNode = (MessageNode) fieldValue;
                            IResourceTypeEditSupport editSupport = resourceType.getEditSupport();
                            if (editSupport != null)
                                return editSupport.getLabelText(messageNode.build());
                        }
                        return "";
                    }
                    else if (fieldValue instanceof EnumValueDescriptor) {
                        EnumValueDescriptor enumValueDescriptor = (EnumValueDescriptor) fieldValue;
                        return enumValueDescriptor.getName();
                    }
                    else {
                        return fieldValue.toString();
                    }
                }
                return element.toString();
            }
        });

        valueColumn.getColumn().setWidth(200);
        valueColumn.getColumn().setMoveable(true);
        valueColumn.getColumn().setText("Value");
        //valueColumn.setLabelProvider(propertyValueLabelProvider);
        GridData gd = new GridData(GridData.FILL_BOTH);
        gd.heightHint = 20;
        gd.widthHint = 100;
        gd.verticalSpan = 2;
        t.setLayoutData(gd);
        toolkit.paintBordersFor(parent);

        this.menu = new Menu(parent.getShell(), SWT.POP_UP);
        this.menu.addListener(SWT.Show, this);

        this.treeViewer.getTree().setMenu(menu);

        this.listeners = new ArrayList<IProtoListener>();

        /* Workaround for DEF-939
         * Call pack() on the table for it to recalc it's size,
         * otherwise the header bar will cover the first row on OS X Yosemite.
         * See: https://bugs.eclipse.org/bugs/show_bug.cgi?id=446534
         */
        treeViewer.getTree().pack();
    }

    public TreeViewer getTreeViewer() {
        return treeViewer;
    }

    public void executeOperation(IUndoableOperation operation) {
        IOperationHistory history = PlatformUI.getWorkbench().getOperationSupport().getOperationHistory();
        operation.addContext(undoContext);
        try {
            history.execute(operation, null, null);
        } catch (ExecutionException e) {
            logger.error("Failed to execute operation", e);
        }
    }

    public void setInput(MessageNode messageNode, IResourceType resourceType) {
        this.resourceType = resourceType;
        this.treeViewer.setInput(messageNode);
    }

    @Override
    public void handleEvent(Event event) {
        if (event.type == SWT.Show) {
            MenuItem [] menuItems = this.menu.getItems ();
            for (int i=0; i<menuItems.length; i++) {
                menuItems[i].removeListener(SWT.Selection, this);
                menuItems[i].dispose();
            }

            ISelection selection = treeViewer.getSelection();

            if (!selection.isEmpty()) {
                IStructuredSelection structSelection = (IStructuredSelection) selection;
                Object selected = structSelection.getFirstElement();

                if (selected instanceof IPath) {
                    IPath path = (IPath) selected;
                    MessageNode message = (MessageNode) treeViewer.getInput();
                    Object value = message.getField(path);

                    if (value instanceof RepeatedNode) {
                        RepeatedNode repeatedNode = (RepeatedNode) value;
                        FieldDescriptor fieldDescriptor = repeatedNode.getFieldDescriptor();

                        if (createDefaultValue(fieldDescriptor) != null) {
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
            MessageNode message = (MessageNode) treeViewer.getInput();

            if (operation.equals("add")) {
                RepeatedNode oldRepeated = (RepeatedNode) message.getField(path);
                Object newValue = createDefaultValue(oldRepeated.getFieldDescriptor());
                if (newValue == null)
                    return;

                List<Object> oldList = oldRepeated.getValueList();
                AddRepeatedOperation op = new AddRepeatedOperation(treeViewer, message, path, oldList, newValue);
                executeOperation(op);
            }
            else if (operation.equals("remove")) {
                RepeatedNode oldRepeated = (RepeatedNode) message.getField(path.getParent());
                List<Object> oldList = oldRepeated.getValueList();
                RemoveRepeatedOperation op = new RemoveRepeatedOperation(treeViewer, message, path, oldList);
                executeOperation(op);
            }
        }
    }

    public void addListener(IProtoListener listener) {
        listeners.add(listener);
    }

    public void removeListener(IProtoListener listener) {
        listeners.remove(listener);
    }

    public void fireFieldChanged(MessageNode messageNode, IPath field) {
        for (IProtoListener listener : listeners) {
            listener.fieldChanged(messageNode, field);
        }
    }

    private Object createDefaultValue(FieldDescriptor fieldDescriptor) {
        JavaType javaType = fieldDescriptor.getJavaType();

        if (javaType == JavaType.MESSAGE) {
            IResourceTypeEditSupport editSupport = this.resourceType.getEditSupport();
            if (editSupport != null) {
                return editSupport.getTemplateMessageFor(fieldDescriptor.getMessageType());
            }
        }

        switch (javaType) {
            case BOOLEAN:
                return false;
            case DOUBLE:
                return 0.0;
            case FLOAT:
                return 0.0f;
            case INT:
            case LONG:
                return 0;
            case STRING:
                return "";
        }
        return null;
    }

}
