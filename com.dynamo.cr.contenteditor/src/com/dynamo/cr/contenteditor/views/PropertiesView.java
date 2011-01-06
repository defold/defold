package com.dynamo.cr.contenteditor.views;

import java.util.ArrayList;
import java.util.Collections;
import java.util.HashSet;
import java.util.Set;
import java.util.concurrent.ArrayBlockingQueue;
import java.util.concurrent.TimeUnit;

import javax.xml.bind.PropertyException;

import org.eclipse.core.commands.operations.UndoContext;
import org.eclipse.jface.viewers.CellEditor;
import org.eclipse.jface.viewers.ColumnLabelProvider;
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
import org.eclipse.jface.viewers.ViewerSorter;
import org.eclipse.swt.SWT;
import org.eclipse.swt.graphics.Image;
import org.eclipse.swt.widgets.Composite;
import org.eclipse.swt.widgets.Display;
import org.eclipse.ui.IEditorPart;
import org.eclipse.ui.ISelectionListener;
import org.eclipse.ui.ISharedImages;
import org.eclipse.ui.IViewSite;
import org.eclipse.ui.IWorkbenchPart;
import org.eclipse.ui.PartInitException;
import org.eclipse.ui.PlatformUI;
import org.eclipse.ui.actions.ActionFactory;
import org.eclipse.ui.contexts.IContextService;
import org.eclipse.ui.operations.RedoActionHandler;
import org.eclipse.ui.operations.UndoActionHandler;
import org.eclipse.ui.part.ViewPart;

import com.dynamo.cr.contenteditor.editors.IEditor;
import com.dynamo.cr.contenteditor.operations.SetPropertyOperation;
import com.dynamo.cr.contenteditor.scene.IProperty;
import com.dynamo.cr.contenteditor.scene.ISceneListener;
import com.dynamo.cr.contenteditor.scene.Node;
import com.dynamo.cr.contenteditor.scene.Scene;
import com.dynamo.cr.contenteditor.scene.SceneEvent;
import com.dynamo.cr.contenteditor.scene.ScenePropertyChangedEvent;

public class PropertiesView extends ViewPart implements ISelectionListener,
        ISceneListener {

    /**
     * The ID of the view as specified by the extension.
     */
    public static final String ID = "com.dynamo.cr.contenteditor.views.PropertiesView";

    private TreeViewer viewer;
    Node node;
    private Set<String> previouslyExpanded = new HashSet<String>();
    private Scene currentScene;
    private PropertyUpdateThread updateThread;

    class PropertyUpdateThread extends Thread {

        ArrayBlockingQueue<IProperty> queue = new ArrayBlockingQueue<IProperty>(128);
        private boolean quit = false;
        private boolean setPropertiesRequested = false;
        private long lastAdded;

        void setPropertiesAsync(final IProperty property) {

            Display.getDefault().asyncExec(new Runnable() {
                @Override
                public void run() {
                    viewer.update(property, null);
                }
            });
        }

        @Override
        public void run() {
            ArrayList<IProperty> lst = new ArrayList<IProperty>();

            while (!quit) {
                IProperty property = null;
                try {
                    property = queue.poll(5, TimeUnit.SECONDS);
                } catch (InterruptedException e) {
                    if (quit)
                        return;
                }

                if (property != null) {
                    if (setPropertiesRequested) {
                        try {
                            Thread.sleep(200);
                        } catch (InterruptedException e) {
                            if (quit)
                                return;
                        }
                    }

                    lst.clear();
                    lst.add(property);
                    queue.drainTo(lst);
                    Collections.reverse(lst);
                    while (lst.size() > 0) {
                        property = lst.remove(0);

                        for (int i = 1; i < lst.size(); ++i) {
                            IProperty tmp = lst.get(i);
                            if (tmp == property) {
                                // Remove duplicates
                                lst.remove(i);
                                --i;
                            }
                        }
                        setPropertiesRequested = true;
                        setPropertiesAsync(property);
                    }
                }
            }
        }

        public void dispose() {
            quit = true;
            this.interrupt();
            try {
                this.join();
            } catch (InterruptedException e) {
            }
        }

        public void addProperty(IProperty property) {
            queue.offer(property);
            long time = System.currentTimeMillis();

            if (time - lastAdded > 200) {
                this.interrupt();
            }

            lastAdded = time;
        }
    }

    class ViewLabelProvider extends LabelProvider implements ITableLabelProvider {
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

    class NameSorter extends ViewerSorter {
    }

    class PropertyValueLabelProvider extends ColumnLabelProvider {
        public String getText(Object element) {
            if (element instanceof IProperty) {
                IProperty p = (IProperty) element;
                try {
                    // NOTE: We return editable value. Less decimals.
                    // The "real" value is properly handled for undo/redo actions.
                    // See ValueEditingSupport.setValue
                    return p.getEditableValue(node).toString();
                } catch (PropertyException e) {
                    e.printStackTrace();
                }
            }
            return element.toString();
        }
    }

    class PropertyContentProvider implements ITreeContentProvider {

        @Override
        public void dispose() {
        }

        @Override
        public void inputChanged(Viewer viewer, Object oldInput, Object newInput) {
        }

        @Override
        public Object[] getElements(Object inputElement) {
            if (inputElement instanceof Node) {
                Node node = (Node) inputElement;
                return node.getProperties();
            }
            return null;
        }

        @Override
        public Object[] getChildren(Object parentElement) {
            if (parentElement instanceof IProperty) {
                IProperty p = (IProperty) parentElement;
                return p.getSubProperties();
            }
            return null;
        }

        @Override
        public Object getParent(Object element) {
            if (element instanceof IProperty) {
                IProperty p = (IProperty) element;
                return p.getParentProperty();
            }
            return null;
        }

        @Override
        public boolean hasChildren(Object element) {
            if (element instanceof IProperty) {
                IProperty p = (IProperty) element;
                IProperty[] sub = p.getSubProperties();
                return sub != null && sub.length > 0;
            }
            return false;
        }

    }

    class ValueEditingSupport extends EditingSupport {

        private TextCellEditor textEditor;

        public ValueEditingSupport(TreeViewer viewer) {
            super(viewer);
            textEditor = new TextCellEditor(viewer.getTree());
        }

        @Override
        protected CellEditor getCellEditor(Object element) {
            return textEditor;
        }

        @Override
        protected boolean canEdit(Object element) {
            if (element instanceof IProperty) {
                IProperty p = (IProperty) element;
                IProperty[] subProps = p.getSubProperties();
                return subProps == null || subProps.length == 0;
            }
            return false;
        }

        @Override
        protected Object getValue(Object element) {
            IProperty p = (IProperty) element;
            try {
                return p.getEditableValue(node).toString();
            } catch (PropertyException e) {
                e.printStackTrace();
            }
            return null;
        }

        @Override
        protected void setValue(Object element, Object value) {

            IEditorPart activeEditor = getSite().getPage().getActiveEditor();
            if (activeEditor instanceof IEditor) {
                IEditor editor = (IEditor) activeEditor;

                IProperty p = (IProperty) element;
                try {

                    Object original_edit_value = p.getEditableValue(node)
                            .toString();
                    // Object current_edit_value = t.getText();

                    if (original_edit_value.equals(value))
                        return;

                    IProperty undo_property = p;
                    while (!undo_property.isIndependent()) {
                        undo_property = undo_property.getParentProperty();
                    }

                    Object undo_original_value = undo_property.getValue(node);

                    Object new_value = value.toString();
                    String parent_name = "";
                    if (p.getParentProperty() != null) {
                        parent_name = p.getParentProperty().getName() + ".";
                    }

                    // TODO: Always irreversible? quat.x, normalize(), ...
                    // Store parent property for undo-state instead?
                    // Kanske for det mesta men hur ar det for euler vs quat och
                    // skuggade tillstand?
                    SetPropertyOperation op = new SetPropertyOperation("set "
                            + parent_name + p.getName(), node, p,
                            undo_property, undo_original_value, new_value);
                    editor.executeOperation(op);

                    getViewer().update(element, null);

                    // p.setValue(node, value);
                } catch (PropertyException e) {
                    e.printStackTrace();
                }
            }
        }
    }

    /**
     * The constructor.
     */
    public PropertiesView() {
        updateThread = new PropertyUpdateThread();
        updateThread.start();

    }

    @Override
    public void dispose() {
        super.dispose();
        getViewSite().getPage().removeSelectionListener(this);
        updateThread.dispose();

        if (currentScene != null) {
            currentScene.removeSceneListener(this);
        }
    }

    /**
     * This is a callback that will allow us to create the viewer and initialize
     * it.
     */
    public void createPartControl(Composite parent) {
        viewer = new TreeViewer(parent, SWT.BORDER | SWT.FULL_SELECTION);

        ViewLabelProvider labelProvider = new ViewLabelProvider();
        viewer.setContentProvider(new PropertyContentProvider());
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
                if (element instanceof IProperty) {
                    IProperty p = (IProperty) element;
                    return p.getName();
                }
                return super.getText(element);
            }
        });

        ColumnLabelProvider propertyValueLabelProvider = new PropertyValueLabelProvider();
        TreeViewerColumn valueColumn = new TreeViewerColumn(viewer, SWT.NONE);
        valueColumn.getColumn().setWidth(140);
        valueColumn.getColumn().setMoveable(true);
        valueColumn.getColumn().setText("Value");
        valueColumn.setLabelProvider(propertyValueLabelProvider);

        valueColumn.setEditingSupport(new ValueEditingSupport(viewer));

        getViewSite().getPage().addSelectionListener(this);

        IContextService context_service = (IContextService) getSite().getService(IContextService.class);
        context_service.activateContext("com.dynamo.cr.contenteditor.contexts.collectioneditor");
    }

    @Override
    public void init(IViewSite site) throws PartInitException {
        super.init(site);
    }

    public void setFocus() {
        viewer.getControl().setFocus();
    }

    @Override
    public void selectionChanged(IWorkbenchPart part, ISelection selection) {

        IEditorPart activeEditor = getSite().getPage().getActiveEditor();
        if (activeEditor instanceof IEditor) {
            IEditor editor = (IEditor) activeEditor;

            // Avoid updating the UI when selecting.
            if (editor.isSelecting()) {
                return;
            }

            UndoContext undoContext = editor.getUndoContext();
            getViewSite().getActionBars().setGlobalActionHandler(
                    ActionFactory.UNDO.getId(),
                    new UndoActionHandler(getSite(), undoContext));
            getViewSite().getActionBars().setGlobalActionHandler(
                    ActionFactory.REDO.getId(),
                    new RedoActionHandler(getSite(), undoContext));

            Scene scene = editor.getScene();
            if (currentScene != null) {
                if (scene != currentScene) {
                    currentScene.removeSceneListener(this);
                    scene.addSceneListener(this);
                }
            } else {
                scene.addSceneListener(this);
            }
            currentScene = scene;
        }

        if (selection instanceof IStructuredSelection) {
            IStructuredSelection structSelection = (IStructuredSelection) selection;
            if (!structSelection.isEmpty()) {
                Object first = structSelection.getFirstElement();
                if (first instanceof Node) {
                    Node node = (Node) first;

                    // Only "selectable" nodes have editable properties
                    if ((node.getFlags() & Node.FLAG_SELECTABLE) != 0) {
                        this.node = node;
                        if (viewer.getInput() != null) {
                            Object[] expanded = viewer.getExpandedElements();
                            Set<String> expandedNames = new HashSet<String>();
                            for (Object o : expanded) {
                                if (o instanceof IProperty) {
                                    IProperty p = (IProperty) o;
                                    expandedNames.add(p.getName());
                                }
                            }
                            previouslyExpanded = expandedNames;
                        }

                        viewer.setInput(node);

                        IProperty[] properties = node.getProperties();
                        ArrayList<IProperty> toExpand = new ArrayList<IProperty>();
                        for (IProperty p : properties) {
                            if (previouslyExpanded.contains(p.getName())) {
                                toExpand.add(p);
                            }
                        }

                        viewer.setExpandedElements(toExpand.toArray());
                        return;
                    }
                }
            }
        }
        viewer.setInput(null);
    }

    @Override
    public void sceneChanged(SceneEvent event) {
    }

    void updatePropertyRecursive(IProperty property) {
        updateThread.addProperty(property);

        IProperty[] subProps = property.getSubProperties();
        if (subProps != null) {
            for (IProperty p : subProps) {
                updatePropertyRecursive(p);
            }
        }
    }

    @Override
    public void propertyChanged(ScenePropertyChangedEvent event) {
        updatePropertyRecursive(event.m_Property);
    }
}
