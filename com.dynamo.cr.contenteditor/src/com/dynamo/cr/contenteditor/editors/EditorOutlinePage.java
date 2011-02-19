package com.dynamo.cr.contenteditor.editors;

import java.util.HashMap;
import java.util.Map;

import org.eclipse.core.resources.IContainer;
import org.eclipse.core.resources.IFile;
import org.eclipse.core.runtime.Path;
import org.eclipse.jface.dialogs.MessageDialog;
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
import org.eclipse.jface.viewers.TextCellEditor;
import org.eclipse.jface.viewers.TreeViewer;
import org.eclipse.jface.viewers.TreeViewerColumn;
import org.eclipse.jface.viewers.Viewer;
import org.eclipse.jface.viewers.ViewerCell;
import org.eclipse.jface.viewers.ViewerSorter;
import org.eclipse.swt.SWT;
import org.eclipse.swt.events.KeyEvent;
import org.eclipse.swt.events.KeyListener;
import org.eclipse.swt.events.MouseAdapter;
import org.eclipse.swt.events.MouseEvent;
import org.eclipse.swt.graphics.Image;
import org.eclipse.swt.graphics.Point;
import org.eclipse.swt.widgets.Composite;
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
import com.dynamo.cr.contenteditor.operations.SetIdentifierOperation;
import com.dynamo.cr.contenteditor.scene.CollectionInstanceNode;
import com.dynamo.cr.contenteditor.scene.CollectionNode;
import com.dynamo.cr.contenteditor.scene.ComponentNode;
import com.dynamo.cr.contenteditor.scene.BrokenNode;
import com.dynamo.cr.contenteditor.scene.InstanceNode;
import com.dynamo.cr.contenteditor.scene.MeshNode;
import com.dynamo.cr.contenteditor.scene.Node;
import com.dynamo.cr.contenteditor.scene.PrototypeNode;
import com.dynamo.cr.contenteditor.scene.Scene;

class EditorOutlinePageContentProvider implements ITreeContentProvider
{
    @Override
    public Object[] getChildren(Object parentElement)
    {
        Node n = (Node) parentElement;
        return n.getChilden();
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
        return n.getChilden().length > 0;
    }

    @Override
    public Object[] getElements(Object inputElement)
    {
        Node n = (Node) inputElement;
        return n.getChilden();
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
            return n.getName();
        }
        return super.getText(element);
    }

    @Override
    public Image getImage(Object element)
    {
        ImageRegistry regist = Activator.getDefault().getImageRegistry();
        if (element instanceof PrototypeNode)
        {
            return regist.get(Activator.PROTOTYPE_IMAGE_ID);
        }
        else if (element instanceof InstanceNode)
        {
            return regist.get(Activator.INSTANCE_IMAGE_ID);
        }
        else if (element instanceof CollectionNode)
        {
            return regist.get(Activator.COLLECTION_IMAGE_ID);
        }
        else if (element instanceof CollectionInstanceNode)
        {
            return regist.get(Activator.COLLECTION_IMAGE_ID);
        }
        else if (element instanceof MeshNode)
        {
            return regist.get(Activator.MESH_IMAGE_ID);
        }
        else if (element instanceof BrokenNode)
        {
            return regist.get(Activator.BROKEN_IMAGE_ID);
        }
        else if (element instanceof ComponentNode)
        {
            ComponentNode componentNode = (ComponentNode) element;
            String resource = componentNode.getResource();
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
        if (element instanceof BrokenNode) {
            BrokenNode brokenNode = (BrokenNode) element;
            return brokenNode.getErrorMessage();
        }
        return "X: " + element.toString();
    }
}

public class EditorOutlinePage extends ContentOutlinePage implements ISelectionListener, ICellEditorListener
{
    private CollectionEditor m_Editor;
    private Node m_Root;
    private boolean m_Mac;
    private IContextActivation contextActivation;

    public EditorOutlinePage(CollectionEditor editor)
    {
        m_Editor = editor;
        m_Root = editor.getRoot();
        m_Mac = System.getProperty("os.name").toLowerCase().indexOf( "mac" ) >= 0;
    }

    class CellModifier implements ICellModifier {
        public boolean enabled = false;

        public void setEnabled(boolean enabled) {
            this.enabled = enabled;
        }

        @Override
        public boolean canModify(Object element, String property) {
            return enabled && (element instanceof InstanceNode);
        }

        @Override
        public Object getValue(Object element, String property) {
            InstanceNode node = (InstanceNode) element;
            return node.getIdentifier();
        }

        @Override
        public void modify(Object element, String property, Object value) {
            TreeViewer viewer = getTreeViewer();
            TreeItem item = (TreeItem) element;
            InstanceNode node = (InstanceNode) item.getData();
            Scene scene = m_Editor.getScene();
            Node tmp = scene.getNodeFromIdentifer((String) value);
            if (tmp != null && tmp != node) {
                // Duplicate
                MessageDialog.openWarning(viewer.getTree().getShell(),
                                          "Duplicate identifier",
                                          String.format("Identifier %s already used.", value));
            }
            else {
                String stringValue = (String) value;
                // Check if new value equals old
                if (!node.getIdentifier().equals(stringValue)) {
                    SetIdentifierOperation op = new SetIdentifierOperation(node, (String) value);
                    m_Editor.executeOperation(op);
                    viewer.update(node, new String[] {});
                    viewer.refresh(node);
                }
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
        TreeViewerColumn column = new TreeViewerColumn(viewer, SWT.NONE);
        column.getColumn().setText("Game Object");

        column.setLabelProvider(new EditorOutlineLabelProvider());
        column.getColumn().setWidth(240);

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
                    ComponentNode component_node = (ComponentNode) element;
                    resource_name = component_node.getResource();
                }
                else if (element instanceof InstanceNode) {
                    InstanceNode instance_node = (InstanceNode) element;
                    resource_name = instance_node.getPrototype();
                }
                else if (element instanceof PrototypeNode) {
                    PrototypeNode proto_node = (PrototypeNode) element;
                    resource_name = proto_node.getName();
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
                    NodeLoaderFactory factory = m_Editor.getLoaderFactory();
                    IContainer content_root = factory.getContentRoot();
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
            m_Editor.setSelection(selection);
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
