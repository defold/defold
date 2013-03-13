package com.dynamo.cr.go.core;

import java.util.ArrayList;
import java.util.Comparator;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

import org.eclipse.core.resources.IFile;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Status;
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
public class RefGameObjectInstanceNode extends GameObjectInstanceNode {

    @Property(editorType=EditorType.RESOURCE, extensions={"go"})
    @Resource
    @NotEmpty
    private String gameObject = "";

    private Map<String, Map<String, Object>> componentProperties;

    private transient GameObjectNode gameObjectNode;

    public RefGameObjectInstanceNode() {
        super();
        this.componentProperties = new HashMap<String, Map<String, Object>>();
    }

    public RefGameObjectInstanceNode(GameObjectNode gameObject) {
        super();
        this.gameObjectNode = gameObject;
        if (this.gameObjectNode != null) {
            this.gameObjectNode.setFlagsRecursively(Flags.LOCKED);
            addChild(this.gameObjectNode);
        }
        this.componentProperties = new HashMap<String, Map<String, Object>>();
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

    public String getGameObject() {
        return this.gameObject;
    }

    public void setGameObject(String gameObject) {
        this.gameObject = gameObject;
        reloadGameObject();
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

    public Map<String, Object> getComponentProperties(String id) {
        return this.componentProperties.get(id);
    }

    public void setComponentProperties(String id, Map<String, Object> properties) {
        this.componentProperties.put(id, properties);
    }

    @Override
    public void sortChildren() {
        sortChildren(new Comparator<Node>() {
            @Override
            public int compare(Node o1, Node o2) {
                if (!o1.isEditable()) {
                    return -1;
                } else if (!o2.isEditable()) {
                    return 1;
                } else {
                    if (o1 instanceof GameObjectInstanceNode && o2 instanceof GameObjectInstanceNode) {
                        String id1 = ((GameObjectInstanceNode)o1).getId();
                        String id2 = ((GameObjectInstanceNode)o2).getId();
                        return id1.compareTo(id2);
                    } else if (o1 instanceof ComponentPropertyNode && o2 instanceof ComponentPropertyNode) {
                        String id1 = ((ComponentPropertyNode)o1).getId();
                        String id2 = ((ComponentPropertyNode)o2).getId();
                        return id1.compareTo(id2);
                    } else {
                        if (o1 instanceof ComponentPropertyNode) {
                            return 1;
                        } else {
                            return -1;
                        }
                    }
                }
            }
        });
    }

    @Override
    public String toString() {
        return String.format("%s (%s)", getId(), this.gameObject);
    }

    @Override
    public boolean handleReload(IFile file, boolean childWasReloaded) {
        IFile gameObjectFile = getModel().getFile(this.gameObject);
        if (gameObjectFile.exists() && gameObjectFile.equals(file)) {
            if (reloadGameObject()) {
                return true;
            }
        }
        if (this.gameObjectNode != null) {
            return this.gameObjectNode.handleReload(file, childWasReloaded);
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

    protected void reloadComponentPropertyNodes() {
        Map<String, RefComponentNode> refIds = new HashMap<String, RefComponentNode>();
        List<Node> children = new ArrayList<Node>(getChildren());
        for (Node node : children) {
            if (node instanceof ComponentPropertyNode) {
                ComponentPropertyNode compProp = (ComponentPropertyNode)node;
                RefComponentNode ref = refIds.get(compProp.getId());
                if (ref != null) {
                    compProp.setNode(ref);
                    refIds.remove(compProp.getId());
                } else {
                    removeChild(node);
                }
            }
            else if (node instanceof GameObjectNode) {
                for (Node child : node.getChildren()) {
                    if (child instanceof RefComponentNode) {
                        RefComponentNode ref = (RefComponentNode)child;
                        refIds.put(ref.getId(), ref);
                    }
                }
            }
        }
        for (Map.Entry<String, RefComponentNode> entry : refIds.entrySet()) {
            Map<String, Object> properties = getComponentProperties(entry.getKey());
            if (properties == null) {
                properties = new HashMap<String, Object>();
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
