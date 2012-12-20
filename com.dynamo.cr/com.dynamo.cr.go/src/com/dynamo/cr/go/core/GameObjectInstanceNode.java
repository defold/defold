package com.dynamo.cr.go.core;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

import org.eclipse.core.resources.IFile;
import org.eclipse.core.runtime.IPath;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Path;
import org.eclipse.core.runtime.Status;
import org.eclipse.jface.viewers.IStructuredSelection;
import org.eclipse.jface.viewers.StructuredSelection;
import org.eclipse.osgi.util.NLS;
import org.eclipse.swt.graphics.Image;

import com.dynamo.cr.go.Constants;
import com.dynamo.cr.properties.NotEmpty;
import com.dynamo.cr.properties.Property;
import com.dynamo.cr.properties.Property.EditorType;
import com.dynamo.cr.properties.Resource;
import com.dynamo.cr.sceneed.core.ISceneModel;
import com.dynamo.cr.sceneed.core.Node;

@SuppressWarnings("serial")
public class GameObjectInstanceNode extends InstanceNode {

    @Property(editorType=EditorType.RESOURCE, extensions={"go"})
    @Resource
    @NotEmpty
    private String gameObject = "";

    private Map<String, Map<String, String>> componentProperties;

    private transient GameObjectNode gameObjectNode;

    public GameObjectInstanceNode() {
        super();
    }

    public GameObjectInstanceNode(GameObjectNode gameObject) {
        super();
        this.gameObjectNode = gameObject;
        this.componentProperties = new HashMap<String, Map<String, String>>();
        if (this.gameObjectNode != null) {
            this.gameObjectNode.setFlagsRecursively(Flags.LOCKED);
            addChild(this.gameObjectNode);
        }
    }

    public String getGameObject() {
        return this.gameObject;
    }

    public void setGameObject(String gameObject) {
        this.gameObject = gameObject;
        reloadGameObject();
        if (getId() == null) {
            IPath p = new Path(gameObject).removeFileExtension();
            setId(p.lastSegment());
        }
    }

    @Override
    public void setModel(ISceneModel model) {
        super.setModel(model);
        if (model != null) {
            if (this.gameObjectNode == null) {
                reloadGameObject();
            } else {
                reloadComponentPropertyNodes();
            }
        }
    }

    public Map<String, String> getComponentProperties(String id) {
        return this.componentProperties.get(id);
    }

    public void setComponentProperties(String id, Map<String, String> properties) {
        this.componentProperties.put(id, properties);
    }

    public IStatus validateGameObject() {
        if (getModel() != null && !this.gameObject.isEmpty()) {
            if (this.gameObjectNode == null) {
                int index = this.gameObject.lastIndexOf('.');
                String message = null;
                if (index < 0) {
                    message = NLS.bind(Messages.GameObjectInstanceNode_gameObject_UNKNOWN_TYPE, this.gameObject);
                } else {
                    String ext = this.gameObject.substring(index+1);
                    message = NLS.bind(Messages.GameObjectInstanceNode_gameObject_INVALID_TYPE, ext);
                }
                return new Status(IStatus.ERROR, Constants.PLUGIN_ID, message);
            }
        }
        return Status.OK_STATUS;
    }

    @Override
    public String toString() {
        return String.format("%s (%s)", getId(), this.gameObject);
    }

    @Override
    public boolean handleReload(IFile file) {
        IFile gameObjectFile = getModel().getFile(this.gameObject);
        if (gameObjectFile.exists() && gameObjectFile.equals(file)) {
            if (reloadGameObject()) {
                return true;
            }
        }
        if (this.gameObjectNode != null) {
            return this.gameObjectNode.handleReload(file);
        }
        return false;
    }

    private boolean reloadGameObject() {
        ISceneModel model = getModel();
        if (model != null) {
            try {
                removeChild(this.gameObjectNode);
                this.gameObjectNode = (GameObjectNode)model.loadNode(this.gameObject);
                if (this.gameObjectNode != null) {
                    this.gameObjectNode.setFlagsRecursively(Flags.LOCKED);
                    addChild(this.gameObjectNode);
                    reloadComponentPropertyNodes();
                }
            } catch (Throwable e) {
                // no reason to handle exception since having a null type is invalid state, will be caught in validateComponent below
            }
            return true;
        }
        return false;
    }

    @Override
    protected void childRemoved(Node child) {
        // TODO this is a poor way to remove children from the selection
        // It should preferably be done in Node immediately, but seemed like a too complex change at the moment
        if (getModel() != null) {
            IStructuredSelection selection = getModel().getSelection();
            if (selection != null) {
                Object[] objects = selection.toArray();
                List<Object> list = new ArrayList<Object>(objects.length);
                for (Object o : objects) {
                    if (o != child) {
                        list.add(o);
                    }
                }
                if (objects.length != list.size()) {
                    getModel().setSelection(new StructuredSelection(list));
                }
            }
        }
    }

    private void reloadComponentPropertyNodes() {
        Map<String, RefComponentNode> refIds = new HashMap<String, RefComponentNode>();
        for (Node child : this.gameObjectNode.getChildren()) {
            if (child instanceof RefComponentNode) {
                RefComponentNode ref = (RefComponentNode)child;
                refIds.put(ref.getId(), ref);
            }
        }
        List<Node> children = new ArrayList<Node>(getChildren());
        for (Node node : children) {
            if (node instanceof ComponentPropertyNode) {
                ComponentPropertyNode compProp = (ComponentPropertyNode)node;
                RefComponentNode ref = refIds.get(compProp.getId());
                if (ref != null) {
                    compProp.setRefComponentNode(ref);
                    refIds.remove(compProp.getId());
                } else {
                    removeChild(node);
                }
            }
        }
        for (Map.Entry<String, RefComponentNode> entry : refIds.entrySet()) {
            Map<String, String> properties = getComponentProperties(entry.getKey());
            if (properties == null) {
                properties = new HashMap<String, String>();
                setComponentProperties(entry.getKey(), properties);
            }
            ComponentPropertyNode node = new ComponentPropertyNode(entry.getValue(), properties);
            addChild(node);
        }
    }

    @Override
    public Image getIcon() {
        if (this.gameObjectNode != null) {
            return this.gameObjectNode.getIcon();
        }
        return super.getIcon();
    }

}
