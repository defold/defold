package com.dynamo.cr.contenteditor.editors;

import java.util.Collections;
import java.util.Comparator;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;

import org.eclipse.core.resources.IContainer;
import org.eclipse.core.resources.IFile;
import org.eclipse.core.runtime.Path;
import org.eclipse.jface.resource.ImageRegistry;
import org.eclipse.jface.viewers.CellEditor;
import org.eclipse.jface.viewers.ColumnLabelProvider;
import org.eclipse.jface.viewers.ColumnViewerToolTipSupport;
import org.eclipse.jface.viewers.ICellEditorListener;
import org.eclipse.jface.viewers.ICellModifier;
import org.eclipse.jface.viewers.ISelection;
import org.eclipse.jface.viewers.IStructuredSelection;
import org.eclipse.jface.viewers.ITreeContentProvider;
import org.eclipse.jface.viewers.LabelProvider;
import org.eclipse.jface.viewers.StructuredSelection;
import org.eclipse.jface.viewers.TextCellEditor;
import org.eclipse.jface.viewers.TreeViewer;
import org.eclipse.jface.viewers.Viewer;
import org.eclipse.jface.viewers.ViewerCell;
import org.eclipse.jface.viewers.ViewerSorter;
import org.eclipse.swt.SWT;
import org.eclipse.swt.events.KeyEvent;
import org.eclipse.swt.events.KeyListener;
import org.eclipse.swt.events.MouseAdapter;
import org.eclipse.swt.events.MouseEvent;
import org.eclipse.swt.graphics.Color;
import org.eclipse.swt.graphics.Image;
import org.eclipse.swt.graphics.Point;
import org.eclipse.swt.widgets.Composite;
import org.eclipse.swt.widgets.Display;
import org.eclipse.swt.widgets.TreeItem;
import org.eclipse.ui.IActionBars;
import org.eclipse.ui.ISelectionListener;
import org.eclipse.ui.IWorkbenchPage;
import org.eclipse.ui.IWorkbenchPart;
import org.eclipse.ui.PartInitException;
import org.eclipse.ui.PlatformUI;
import org.eclipse.ui.actions.ActionFactory;
import org.eclipse.ui.contexts.IContextActivation;
import org.eclipse.ui.contexts.IContextService;
import org.eclipse.ui.ide.IDE;
import org.eclipse.ui.part.IPageSite;
import org.eclipse.ui.views.contentoutline.ContentOutline;
import org.eclipse.ui.views.contentoutline.ContentOutlinePage;

import com.dynamo.cr.contenteditor.Activator;
import com.dynamo.cr.scene.graph.CollectionInstanceNode;
import com.dynamo.cr.scene.graph.CollectionNode;
import com.dynamo.cr.scene.graph.CollectionRootNode;
import com.dynamo.cr.scene.graph.ComponentNode;
import com.dynamo.cr.scene.graph.INodeFactory;
import com.dynamo.cr.scene.graph.InstanceNode;
import com.dynamo.cr.scene.graph.Node;
import com.dynamo.cr.scene.graph.NodeFactory;
import com.dynamo.cr.scene.graph.PrototypeNode;
import com.dynamo.cr.scene.operations.SetIdentifierOperation;

class EditorOutlinePageContentProvider implements ITreeContentProvider
{
    @Override
    public Object[] getChildren(Object parentElement)
    {
        Node n = (Node) parentElement;
        return n.getChildren();
    }

    @Override
    public Object getParent(Object element)
    {
        Node n = (Node) element;
        return n.getParent();
    }

    @Override
    public boolean hasChildren(Object element)
    {
        Node n = (Node) element;
        return n.getChildren().length > 0;
    }

    @Override
    public Object[] getElements(Object inputElement)
    {
        Node n = (Node) inputElement;
        return n.getChildren();
    }

    @Override
    public void dispose()
    {
    }

    @Override
    public void inputChanged(Viewer viewer, Object oldInput, Object newInput)
    {
    }
}

class EditorOutlineLabelProvider extends ColumnLabelProvider {
    Map<String, Image> extensionToImage = new HashMap<String, Image>();

    @Override
    public String getText(Object element)
    {
        if (element instanceof Node)
        {
            Node n = (Node) element;
            return n.getLabel();
        }
        return super.getText(element);
    }

    @Override
    public Image getImage(Object element)
    {
        ImageRegistry regist = Activator.getDefault().getImageRegistry();
        Node node = (Node)element;
        if (node instanceof PrototypeNode)
        {
            if (node.isOk()) {
                return regist.get(Activator.PROTOTYPE_IMAGE_ID);
            } else {
                return regist.get(Activator.BROKEN_PROTOTYPE_IMAGE_ID);
            }
        }
        else if (node instanceof InstanceNode)
        {
            if (node.isOk()) {
                return regist.get(Activator.INSTANCE_IMAGE_ID);
            } else {
                return regist.get(Activator.BROKEN_INSTANCE_IMAGE_ID);
            }
        }
        else if (element instanceof CollectionNode || element instanceof CollectionInstanceNode)
        {
            if (node.isOk()) {
                return regist.get(Activator.COLLECTION_IMAGE_ID);
            } else {
                return regist.get(Activator.BROKEN_COLLECTION_IMAGE_ID);
            }
        }
        else if (element instanceof ComponentNode)
        {
            ComponentNode<?> componentNode = (ComponentNode<?>) element;
            String resource = componentNode.getResourceIdentifier();
            String ext = resource.substring(resource.lastIndexOf('.')+1);
            if (!extensionToImage.containsKey(ext)) {
                Image image = PlatformUI.getWorkbench().getEditorRegistry().getImageDescriptor("dummy." + ext).createImage();
                extensionToImage.put(ext, image);
            }

            return extensionToImage.get(ext);
        }

        return super.getImage(element);
    }

    @Override
    public String getToolTipText(Object element) {
        Node node = (Node)element;
        return node.getToolTip();
    }

    @Override
    public boolean isLabelProperty(Object element, String property) {
        if (property.equals("status")) {
            return true;
        } else {
            return false;
        }
    }

    @Override
    public Color getForeground(Object element) {
        Node node = (Node)element;
        if ((node.getFlags() & Node.FLAG_EDITABLE) != 0) {
            return Display.getCurrent().getSystemColor(SWT.COLOR_BLACK);
        } else {
            return Display.getCurrent().getSystemColor(SWT.COLOR_DARK_GRAY);
        }
    }
}

public class EditorOutlinePage extends ContentOutlinePage implements ISelectionListener, ICellEditorListener
{
    private CollectionEditor m_Editor;
    private Node m_Root;
    private boolean m_Mac;
    private IContextActivation contextActivation;
    private int nextSelectionOrder = 0;
    private Map<Object, Integer> selectionOrder = new HashMap<Object, Integer>();

    public EditorOutlinePage(CollectionEditor editor)
    {
        m_Editor = editor;
        m_Root = new CollectionRootNode(editor.getRoot().getScene());
        editor.getRoot().setParent(m_Root);
        m_Mac = System.getProperty("os.name").toLowerCase().indexOf( "mac" ) >= 0;
    }

    class CellModifier implements ICellModifier {
        public boolean enabled = false;

        public void setEnabled(boolean enabled) {
            this.enabled = enabled;
        }

        @Override
        public boolean canModify(Object element, String property) {
            Node node = (Node)element;
            return enabled && ((node.getFlags() & Node.FLAG_EDITABLE) != 0);
        }

        @Override
        public Object getValue(Object element, String property) {
            Node node = (Node) element;
            if (this.enabled) {
                return node.getIdentifier();
            } else {
                return node.getLabel();
            }
        }

        @Override
        public void modify(Object element, String property, Object value) {
            TreeItem item = (TreeItem) element;
            // Check if new value equals old
            String id = (String)value;
            Node node = (Node)item.getData();
            if (!node.getIdentifier().equals(id)) {
                m_Editor.executeOperation(new SetIdentifierOperation(node, id));
            }
        }
    }

    @Override
    public void init(IPageSite pageSite) {
        super.init(pageSite);

        IActionBars action_bars = pageSite.getActionBars();
        String undo_id = ActionFactory.UNDO.getId();
        String redo_id = ActionFactory.REDO.getId();
        action_bars.setGlobalActionHandler(undo_id, m_Editor.getUndoAction());
        action_bars.setGlobalActionHandler(redo_id, m_Editor.getRedoAction());
    }

    @Override
    public void createControl(Composite parent)
    {
        super.createControl(parent);
        final TreeViewer viewer= getTreeViewer();
        ColumnViewerToolTipSupport.enableFor(viewer);

        viewer.getTree().setHeaderVisible(true);
        viewer.setContentProvider(new EditorOutlinePageContentProvider());
        viewer.setLabelProvider(new LabelProvider());
        viewer.setLabelProvider(new EditorOutlineLabelProvider());

        viewer.setSorter(new ViewerSorter() {
            public int compare(Viewer viewer, Object e1, Object e2)
            {
                if (e1 instanceof Node && e2 instanceof Node) {

                    if (e1 instanceof CollectionInstanceNode && !(e2 instanceof CollectionInstanceNode)) {
                        return -1;
                    }
                    else if (e2 instanceof CollectionInstanceNode && !(e1 instanceof CollectionInstanceNode)) {
                        return 1;
                    }

                }
                return super.compare(viewer, e1, e2);
            }
        });
        viewer.addSelectionChangedListener(this);

        TextCellEditor cell_editor = new TextCellEditor(viewer.getTree());
        cell_editor.addListener(this);

        viewer.setCellEditors(new CellEditor[] {cell_editor});
        final CellModifier cell_modifier = new CellModifier();
        viewer.setCellModifier(cell_modifier);

        viewer.getTree().addKeyListener(new KeyListener() {

            @Override
            public void keyReleased(KeyEvent e) { }

            @Override
            public void keyPressed(KeyEvent e) {
                if (e.keyCode == SWT.F2 || (e.keyCode == SWT.CR && m_Mac)) {
                    deactiveContext();

                    cell_modifier.setEnabled(true);
                    ISelection selection = viewer.getSelection();
                    if (!selection.isEmpty()) {
                        Object first = ((IStructuredSelection) selection).getFirstElement();
                        viewer.editElement(first, 0);
                    }
                }
                cell_modifier.setEnabled(false);
            }
        });

        viewer.getTree().addMouseListener(new MouseAdapter() {
            @Override
            public void mouseDoubleClick(MouseEvent e) {
                ViewerCell cell = viewer.getCell(new Point(e.x, e.y));
                if (cell == null)
                    return;

                Object element = cell.getElement();

                String resource_name = null;
                if (element instanceof ComponentNode) {
                    @SuppressWarnings("rawtypes")
                    ComponentNode component_node = (ComponentNode) element;
                    if (!component_node.getResource().isEmbedded()) {
                        resource_name = component_node.getResourceIdentifier();
                    }
                }
                else if (element instanceof InstanceNode) {
                    InstanceNode instance_node = (InstanceNode) element;
                    resource_name = instance_node.getPrototype();
                }
                else if (element instanceof PrototypeNode) {
                    PrototypeNode proto_node = (PrototypeNode) element;
                    resource_name = proto_node.getIdentifier();
                }
                else if (element instanceof CollectionNode) {
                    CollectionNode cn = (CollectionNode) element;
                    resource_name = cn.getResource();
                }
                else if (element instanceof CollectionInstanceNode) {
                    CollectionInstanceNode cin = (CollectionInstanceNode) element;
                    resource_name = cin.getCollection();
                }

                if (resource_name != null) {
                    INodeFactory factory = m_Editor.getNodeFactory();
                    IContainer content_root = ((NodeFactory)factory).getContentRoot();
                    IFile file = content_root.getFile(new Path(resource_name));
                    IWorkbenchPage page = PlatformUI.getWorkbench().getActiveWorkbenchWindow().getActivePage();
                    try {
                        IDE.openEditor(page, file);
                    } catch (PartInitException e1) {
                        e1.printStackTrace();
                    }
                }
            }
        });

        viewer.setColumnProperties(new String[] {"column1"});
        viewer.setInput(m_Root);
        viewer.expandToLevel(2);

        getSite().getPage().addSelectionListener(this);

        IContextService context_service = (IContextService) getSite().getService(IContextService.class);
        this.contextActivation = context_service.activateContext("com.dynamo.cr.contenteditor.contexts.collectioneditor");
    }

    @Override
    public void selectionChanged(IWorkbenchPart part, ISelection selection) {
        if (! (part instanceof ContentOutline))
        {
            TreeViewer viewer = getTreeViewer();

            if (selection instanceof IStructuredSelection) {
                IStructuredSelection structured_selection = (IStructuredSelection) selection;
                Object o = structured_selection.getFirstElement();
                if (o instanceof Node) {
                    viewer.setSelection(selection);
                    return;
                }
            }
        }
        else
        {
            if (selection.isEmpty()) {
                nextSelectionOrder = 0;
                selectionOrder.clear();
            }
            else {
                /*
                 * SWT Tree does't preserve selection order. We need to track it ourself...
                 */

                IStructuredSelection structuredSelection = (IStructuredSelection) selection;
                @SuppressWarnings("unchecked")
                List<Object> list = structuredSelection.toList();
                for (Object object : list) {
                    if (!selectionOrder.containsKey(object))
                    {
                        selectionOrder.put(object, nextSelectionOrder++);
                    }
                }

                // Remove not selected object from selectionOrder
                Set<Object> toRemove = new HashSet<Object>();
                for (Object object : selectionOrder.keySet()) {
                    if (!list.contains(object)) {
                        toRemove.add(object);
                    }
                }
                for (Object object : toRemove) {
                    selectionOrder.remove(object);
                }

                // Sort selected
                Collections.sort(list, new Comparator<Object>() {
                    public int compare(Object o1, Object o2) {
                        if (selectionOrder.containsKey(o1) && selectionOrder.containsKey(o2)) {
                            int order1 = selectionOrder.get(o1);
                            int order2 = selectionOrder.get(o2);
                            return order1 - order2;
                        }
                        // Some random order. Should not happen...
                        return o1.hashCode() - o2.hashCode();
                    }
                });

                m_Editor.setSelection(new StructuredSelection(list));
            }
        }
    }

    public void setInput(Node root)
    {
        m_Root = root;
        getTreeViewer().setInput(root);
    }

    public void refresh() {
        getTreeViewer().refresh();
    }

    public void update(Node node, String[] properties) {
        getTreeViewer().update(node, properties);
    }

    public void refresh(Node node) {
        getTreeViewer().refresh(node);
    }

    void activateContext() {
        IContextService context_service = (IContextService) getSite().getService(IContextService.class);
        contextActivation = context_service.activateContext("com.dynamo.cr.contenteditor.contexts.collectioneditor");
    }

    void deactiveContext() {
        IContextService context_service = (IContextService) getSite().getService(IContextService.class);
        context_service.deactivateContext(contextActivation);
    }

    @Override
    public void applyEditorValue() {
        activateContext();
    }

    @Override
    public void cancelEditor() {
        deactiveContext();
    }

    @Override
    public void editorValueChanged(boolean oldValidState, boolean newValidState) {
    }

}
