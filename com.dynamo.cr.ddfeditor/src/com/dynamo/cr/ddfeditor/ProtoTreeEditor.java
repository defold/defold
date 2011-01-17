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
import org.eclipse.jface.viewers.LabelProvider;
import org.eclipse.jface.viewers.TextCellEditor;
import org.eclipse.jface.viewers.TreeViewer;
import org.eclipse.jface.viewers.TreeViewerColumn;
import org.eclipse.swt.SWT;
import org.eclipse.swt.layout.GridData;
import org.eclipse.swt.layout.GridLayout;
import org.eclipse.swt.widgets.Composite;
import org.eclipse.swt.widgets.Control;
import org.eclipse.swt.widgets.Tree;
import org.eclipse.ui.PlatformUI;
import org.eclipse.ui.dialogs.ListDialog;
import org.eclipse.ui.dialogs.ResourceListSelectionDialog;
import org.eclipse.ui.forms.widgets.FormToolkit;

import com.dynamo.cr.ddfeditor.operations.SetFieldOperation;
import com.dynamo.cr.protobind.IPath;
import com.dynamo.cr.protobind.MessageNode;
import com.dynamo.cr.protobind.Node;
import com.dynamo.cr.protobind.PathElement;
import com.google.protobuf.Descriptors.EnumDescriptor;
import com.google.protobuf.Descriptors.EnumValueDescriptor;
import com.google.protobuf.Descriptors.FieldDescriptor;
import com.google.protobuf.Descriptors.FieldDescriptor.Type;

public class ProtoTreeEditor {

    private TreeViewer treeViewer;
    private IContainer contentRoot;
    private UndoContext undoContext;

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
                org.eclipse.core.runtime.IPath fullPath = r.getFullPath();
                return fullPath.makeRelativeTo(contentRoot.getFullPath()).toPortableString();
            }
            return null;
        }
    }

    class EnumCellEditor extends ComboBoxCellEditor {
        public EnumCellEditor(Composite parent) {
            super(parent, new String[] {});
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

        public ProtoFieldEditingSupport(TreeViewer viewer) {
            super(viewer);
            textEditor = new TextCellEditor(viewer.getTree());
            resourceEditor = new ResourceDialogCellEditor(treeViewer.getTree());
            enumEditor = new EnumCellEditor(viewer.getTree());
        }

        @Override
        protected CellEditor getCellEditor(Object element) {
            IPath fieldPath = (IPath) element;
            PathElement lastElement = fieldPath.lastElement();

            FieldDescriptor fieldDescriptor = lastElement.fieldDescriptor;
            if (fieldDescriptor.getType() == Type.ENUM) {
                enumEditor.setEnumDescriptor(fieldDescriptor.getEnumType());
                return enumEditor;
            }
            else if (isResource(fieldDescriptor)) {
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

            SetFieldOperation op = new SetFieldOperation(treeViewer, message, fieldPath, oldValue, value);
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


        //ColumnLabelProvider propertyValueLabelProvider = new ProtoFieldValueLabelProvider();
        TreeViewerColumn valueColumn = new TreeViewerColumn(treeViewer, SWT.NONE);
        valueColumn.setEditingSupport(new ProtoFieldEditingSupport(treeViewer));

        valueColumn.setLabelProvider(new ColumnLabelProvider() {
            @Override
            public String getText(Object element) {
                if (element instanceof IPath) {
                    IPath fieldPath = (IPath) element;

                    MessageNode message = (MessageNode) treeViewer.getInput();
                    Object fieldValue = message.getField(fieldPath);

                    if (fieldValue instanceof Node) {
                        if (fieldValue instanceof MessageNode) {
                            // TODO: Stuff from DdfEditor. If ProtoTreeEditor is to be used in DdfEditor fix this...
                            //MessageNode messageNode = (MessageNode) fieldValue;
                            return "";
                         //   return getMessageNodeLabelValue(messageNode);
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

    public void setInput(MessageNode messageNode) {
        this.treeViewer.setInput(messageNode);
    }

}
