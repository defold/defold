package com.dynamo.cr.go.core;

import java.util.ArrayList;
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
public class CollectionInstanceNode extends InstanceNode {

    @Property(editorType=EditorType.RESOURCE, extensions={"collection"})
    @Resource
    @NotEmpty
    private String collection = "";

    private Map<String, Map<String, Map<String, Object>>> instanceProperties;

    private CollectionNode collectionNode;

    public CollectionInstanceNode(CollectionNode collection) {
        super();
        this.collectionNode = collection;
        if (this.collectionNode != null) {
            this.collectionNode.setFlagsRecursively(Flags.LOCKED);
            addChild(this.collectionNode);
        }
        this.instanceProperties = new HashMap<String, Map<String, Map<String, Object>>>();
    }

    @Override
    public void setModel(ISceneModel model) {
        super.setModel(model);
        if (model != null) {
            reloadCollection();
        }
    }

    public String getCollection() {
        return this.collection;
    }

    public void setCollection(String collection) {
        this.collection = collection;
        reloadCollection();
    }

    public IStatus validateCollection() {
        if (getModel() != null && !this.collection.isEmpty()) {
            if (this.collectionNode == null) {
                int index = this.collection.lastIndexOf('.');
                String message = null;
                if (index < 0) {
                    message = NLS.bind(Messages.CollectionInstanceNode_collection_UNKNOWN_TYPE, this.collection);
                } else {
                    String ext = this.collection.substring(index+1);
                    message = NLS.bind(Messages.CollectionInstanceNode_collection_INVALID_TYPE, ext);
                }
                return new Status(IStatus.ERROR, Constants.PLUGIN_ID, message);
            }
        }
        return Status.OK_STATUS;
    }

    public Map<String, Object> getInstanceProperties(String path, String id) {
        Map<String, Map<String, Object>> componentProperties = this.instanceProperties.get(path);
        if (componentProperties != null) {
            return componentProperties.get(id);
        }
        return null;
    }

    public void setInstanceProperties(String path, String id, Map<String, Object> properties) {
        Map<String, Map<String, Object>> compProps = this.instanceProperties.get(path);
        if (compProps == null) {
            compProps = new HashMap<String, Map<String, Object>>();
            this.instanceProperties.put(path, compProps);
        }
        compProps.put(id, properties);
    }

    @Override
    public String toString() {
        return String.format("%s (%s)", getId(), this.collection);
    }

    @Override
    public boolean handleReload(IFile file, boolean childWasReloaded) {
        IFile collectionFile = getModel().getFile(this.collection);
        if (collectionFile.exists() && collectionFile.equals(file)) {
            if (reloadCollection()) {
                return true;
            }
        } else if (childWasReloaded) {
            reloadInstancePropertyNodes();
        }
        return false;
    }

    private boolean reloadCollection() {
        ISceneModel model = getModel();
        if (model != null) {
            try {
                clearChildren();
                this.collectionNode = (CollectionNode)model.loadNode(this.collection);
                if (this.collectionNode != null) {
                    this.collectionNode.setModel(model);
                    this.collectionNode.setPath(this.collection);
                    this.collectionNode.setFlagsRecursively(Flags.LOCKED);
                    addChild(this.collectionNode);
                    reloadInstancePropertyNodes();
                }
            } catch (Throwable e) {
                // no reason to handle exception since having a null type is invalid state, will be caught in validateComponent below
            }
            return true;
        } else if (this.collectionNode != null) {
            this.collectionNode.setPath(this.collection);
        }
        return false;
    }

    protected void reloadInstancePropertyNodes() {
        Map<String, InstanceNode> refIds = new HashMap<String, InstanceNode>();
        List<Node> children = new ArrayList<Node>(getChildren());
        for (Node node : children) {
            if (node instanceof CollectionNode) {
                for (Node child : node.getChildren()) {
                    if (child instanceof InstanceNode) {
                        InstanceNode ref = (InstanceNode)child;
                        refIds.put(ref.getId(), ref);
                    }
                }
                break;
            }
        }
        for (Node node : children) {
            if (node instanceof GameObjectPropertyNode || node instanceof CollectionPropertyNode) {
                removeChild(node);
            }
        }
        for (Map.Entry<String, InstanceNode> entry : refIds.entrySet()) {
            Node node = buildPropertyNodes("", null, entry.getValue());
            addChild(node);
        }
    }

    private Node buildPropertyNodes(String path, String id, Node node) {
        Node propertyNode = null;
        if (node instanceof GameObjectInstanceNode) {
            GameObjectInstanceNode goi = (GameObjectInstanceNode)node;
            propertyNode = new GameObjectPropertyNode(goi);
            for (Node child : goi.getChildren()) {
                // Side step the implicit GameObjectNode of a RefGameObjectInstanceNode
                if (child instanceof GameObjectNode && ((GameObjectNode)child).getId() == null) {
                    for (Node grandChild : child.getChildren()) {
                        propertyNode.addChild(buildPropertyNodes(path, goi.getId(), grandChild));
                    }
                } else {
                    propertyNode.addChild(buildPropertyNodes(path, goi.getId(), child));
                }
            }
        } else if (node instanceof CollectionInstanceNode) {
            CollectionInstanceNode ci = (CollectionInstanceNode)node;
            propertyNode = new CollectionPropertyNode(ci);
            for (Node child : ci.getChildren()) {
                if (child instanceof InstanceNode) {
                    propertyNode.addChild(buildPropertyNodes(path, id, (InstanceNode)child));
                } else if (child instanceof CollectionNode) {
                    for (Node grandChild : child.getChildren()) {
                        if (grandChild instanceof InstanceNode) {
                            propertyNode.addChild(buildPropertyNodes(path + ci.getId() + "/", id, (InstanceNode)grandChild));
                        }
                    }
                }
            }
        } else if (node instanceof RefComponentNode) {
            String fullPath = path + id;
            Map<String, Map<String, Object>> compProps = instanceProperties.get(fullPath);
            if (compProps == null) {
                compProps = new HashMap<String, Map<String, Object>>();
                instanceProperties.put(fullPath, compProps);
            }
            RefComponentNode comp = (RefComponentNode)node;
            Map<String, Object> props = compProps.get(comp.getId());
            if (props == null) {
                props = new HashMap<String, Object>();
                compProps.put(comp.getId(), props);
            }
            propertyNode = new ComponentPropertyNode(comp, props);
        }
        return propertyNode;
    }

    @Override
    public Image getIcon() {
        if (this.collectionNode != null) {
            return this.collectionNode.getIcon();
        }
        return super.getIcon();
    }

}
