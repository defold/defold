package com.dynamo.cr.scene.graph;

import java.io.IOException;
import java.util.HashMap;
import java.util.Map;

import org.eclipse.core.runtime.CoreException;

import com.dynamo.cr.scene.math.MathUtil;
import com.dynamo.cr.scene.resource.CollectionResource;
import com.dynamo.cr.scene.resource.IResourceListener;
import com.dynamo.cr.scene.resource.PrototypeResource;
import com.dynamo.cr.scene.resource.Resource;
import com.dynamo.gameobject.proto.GameObject.CollectionDesc;
import com.dynamo.gameobject.proto.GameObject.CollectionInstanceDesc;
import com.dynamo.gameobject.proto.GameObject.InstanceDesc;

public class CollectionNode extends Node implements IResourceListener {

    private CollectionResource resource;
    private Map<String, Node> collectionInstanceIdToNode;
    private Map<String, Node> instanceIdToNode;
    private INodeFactory factory;

    public static INodeCreator getCreator() {
        return new INodeCreator() {

            @Override
            public Node create(String identifier, Resource resource, Node parent, Scene scene, INodeFactory factory)
                    throws IOException, CreateException, CoreException {
                return new CollectionNode(identifier, (CollectionResource)resource, scene, factory);
            }
        };
    }

    public CollectionNode(String identifier, CollectionResource resource, Scene scene, INodeFactory factory) throws CreateException {
        super(identifier, scene, FLAG_EDITABLE | FLAG_CAN_HAVE_CHILDREN);
        this.resource = resource;
        this.collectionInstanceIdToNode = new HashMap<String, Node>();
        this.instanceIdToNode = new HashMap<String, Node>();
        this.factory = factory;
        setup();
        resource.addListener(this);
    }

    @Override
    protected void finalize() throws Throwable {
        resource.removeListener(this);
        super.finalize();
    }

    private void setup() throws CreateException {
        try {
            CollectionDesc desc = this.resource.getCollectionDesc();
            if (desc != null) {
                String id = desc.getName();
                setIdentifier(id);
                Node parent = getParent();
                boolean canHaveChildren = (getFlags() & FLAG_CAN_HAVE_CHILDREN) == FLAG_CAN_HAVE_CHILDREN;
                setFlags(getFlags() | FLAG_CAN_HAVE_CHILDREN);
                clearNodes();
                for (int i = 0; i < desc.getCollectionInstancesCount(); ++i) {
                    CollectionInstanceDesc cid = desc.getCollectionInstances(i);
                    CollectionResource subResource = this.resource.getCollectionInstances().get(i);
                    // detect recursion
                    String ancestorCollection = id;
                    Node subNode;
                    if (!id.equals(cid.getCollection()) && parent != null) {
                        Node ancestor = parent;
                        ancestorCollection = ((CollectionNode)parent).getResource();
                        while (!ancestorCollection.equals(cid.getCollection()) && ancestor != null) {
                            ancestor = ancestor.getParent();
                            if (ancestor != null && ancestor instanceof CollectionNode) {
                                ancestorCollection = ((CollectionNode)ancestor).getResource();
                            }
                        }
                    }
                    Node subCollection = null;
                    if (!ancestorCollection.equals(cid.getCollection()) && subResource != null) {
                        subCollection = factory.create(cid.getId(), subResource, parent, getScene());
                        if (subResource.getCollectionDesc() == null) {
                            String error = "Unable to open " + cid.getCollection();
                            subCollection.setError(Node.ERROR_FLAG_RESOURCE_ERROR, error);
                            factory.reportError(error);
                        }
                    }

                    subNode = new CollectionInstanceNode(cid.getId(), getScene(), cid.getCollection(), subCollection);
                    if (subCollection == null) {
                        subNode.setError(Node.ERROR_FLAG_RECURSION, String.format("The resource %s is already used in a collection above this item.", cid.getCollection()));
                    }

                    subNode.setLocalTranslation(MathUtil.toVector4(cid.getPosition()));
                    subNode.setLocalRotation(MathUtil.toQuat4(cid.getRotation()));

                    addNode(subNode);
                }

                // Needs to be map of lists to handle duplicated ids
                Map<String, Node> idToNode = new HashMap<String, Node>();
                for (int i = 0; i < desc.getInstancesCount(); ++i) {
                    InstanceDesc instanceDesc = desc.getInstances(i);
                    PrototypeResource subResource = resource.getPrototypeInstances().get(i);
                    Node prototype;
                    try {
                        prototype = factory.create(subResource.getPath(), subResource, null, getScene());
                    } catch (IOException e) {
                        prototype = new PrototypeNode(subResource.getPath(), subResource, getScene(), factory);
                        prototype.setError(Node.ERROR_FLAG_RESOURCE_ERROR, e.getMessage());
                        factory.reportError(e.getMessage());
                    }

                    InstanceNode in = new InstanceNode(instanceDesc.getId(), getScene(), instanceDesc.getPrototype(), prototype);
                    idToNode.put(instanceDesc.getId(), in);
                    in.setLocalTranslation(MathUtil.toVector4(instanceDesc.getPosition()));
                    in.setLocalRotation(MathUtil.toQuat4(instanceDesc.getRotation()));
                    addNode(in);
                }

                for (InstanceDesc instanceDesc : desc.getInstancesList()) {
                    Node parentInstance = idToNode.get(instanceDesc.getId());
                    for (String childId : instanceDesc.getChildrenList()) {
                        Node child = idToNode.get(childId);
                        if (child == null)
                            throw new CreateException(String.format("Child %s not found", childId));

                        if (parentInstance.acceptsChild(child)) {
                            removeNode(child);
                            parentInstance.addNode(child);
                        } else {
                            Node instanceNode = new InstanceNode(childId, getScene(), null, null);
                            parentInstance.addNode(instanceNode);
                            instanceNode.setError(Node.ERROR_FLAG_RECURSION, String.format("The instance %s can not be a child of %s since it occurs above %s in the hierarchy.", childId, instanceDesc.getId(), instanceDesc.getId()));
                        }
                    }
                }
                if (!canHaveChildren) {
                    setFlags(getFlags() & ~FLAG_CAN_HAVE_CHILDREN);
                }
            }
        } catch (IOException e) {
            throw new CreateException(e.getMessage(), e);
        } catch (CoreException e) {
            throw new CreateException(e.getMessage(), e);
        }
    }

    @Override
    public String getLabel() {
        return String.format("%s (%s)", getIdentifier(), this.resource.getPath());
    }

    private void addId(Node node, Map<String, Node> registry) {
        String id = node.getIdentifier();
        if (registry.containsKey(id)) {
            node.setError(ERROR_FLAG_DUPLICATE_ID);
        } else {
            registry.put(id, node);
        }
    }

    @Override
    public void nodeAdded(Node node) {
        if (node instanceof CollectionInstanceNode) {
            addId(node, collectionInstanceIdToNode);
            node.setFlags(node.getFlags() & ~Node.FLAG_CAN_HAVE_CHILDREN);
        } else if (node instanceof InstanceNode) {
            addId(node, instanceIdToNode);
        }
    }

    private void removeId(Node node, String id, Map<String, Node> registry) {
        if (registry.get(id) == node) {
            registry.remove(id);
            for (Node child : getChildren()) {
                if (node.getClass().isInstance(child)) {
                    if (child.getIdentifier().equals(id)) {
                        registry.put(id, child);
                        child.clearError(Node.ERROR_FLAG_DUPLICATE_ID);
                        break;
                    }
                }
            }
        }
    }

    @Override
    protected void nodeRemoved(Node node) {
        String id = node.getIdentifier();
        if (node instanceof CollectionInstanceNode) {
            removeId(node, id, collectionInstanceIdToNode);
        } else if (node instanceof InstanceNode) {
            removeId(node, id, instanceIdToNode);
        }
    }

    @Override
    public void draw(DrawContext context) {
    }

    public String getResource() {
        return resource.getPath();
    }

    void buildInstance(CollectionDesc.Builder builder, InstanceNode in) {
        builder.addInstances(in.buildDesciptor());

        for (Node child : in.getChildren()) {
            if (child instanceof InstanceNode) {
                buildInstance(builder, (InstanceNode)child);
            }
        }
    }

    public CollectionDesc buildDescriptor() {
        CollectionDesc.Builder builder = CollectionDesc.newBuilder();
        builder.setName(getIdentifier());

        for (Node child : getChildren()) {
            if (child instanceof CollectionInstanceNode) {
                builder.addCollectionInstances(((CollectionInstanceNode)child).buildDescriptor());
            } else if (child instanceof InstanceNode) {
                buildInstance(builder, (InstanceNode)child);
            }
        }

        return builder.build();
    }

    @Override
    protected void childIdentifierChanged(Node node, String oldId) {
        if (node instanceof CollectionInstanceNode) {
            removeId(node, oldId, collectionInstanceIdToNode);
            node.clearError(ERROR_FLAG_DUPLICATE_ID);
            addId(node, collectionInstanceIdToNode);
        } else if (node instanceof InstanceNode) {
            removeId(node, oldId, instanceIdToNode);
            node.clearError(ERROR_FLAG_DUPLICATE_ID);
            addId(node, instanceIdToNode);
        }
    }

    @Override
    public boolean isChildIdentifierUsed(Node node, String id) {
        if (node instanceof CollectionInstanceNode) {
            return collectionInstanceIdToNode.containsKey(id);
        } else if (node instanceof InstanceNode) {
            return instanceIdToNode.containsKey(id);
        } else {
            assert false : "Child of CollectionNode instance of illegal class.";
            return false;
        }
    }

    @Override
    public String getUniqueChildIdentifier(Node child) {
        if (child instanceof CollectionInstanceNode) {
            return getUniqueCollectionInstanceId(child.getIdentifier());
        } else if (child instanceof InstanceNode) {
            return getUniqueInstanceId(child.getIdentifier());
        } else {
            return super.getUniqueChildIdentifier(child);
        }
    }

    public Node getCollectionInstanceNodeFromId(String ident) {
        return collectionInstanceIdToNode.get(ident);
    }

    public Node getInstanceNodeFromId(String ident) {
        return instanceIdToNode.get(ident);
    }

    private String getUniqueCollectionInstanceId(String base) {
        int id = 0;

        int i = base.length()-1;
        while (i >= 0) {
            char c = base.charAt(i);
            if (!(c >= '0' && c <= '9'))
                break;
            --i;
        }
        int leading_zeros = 0;
        if (i < base.length()-1) {
            id = Integer.parseInt(base.substring(i+1));
            leading_zeros = base.substring(i+1).length();
            base = base.substring(0, i+1);
        }
        String format_string;
        if (leading_zeros > 0)
            format_string = String.format("%%s%%0%dd", leading_zeros);
        else
            format_string = "%s%d";

        while (true) {
            String s = String.format(format_string, base, id);
            if (!collectionInstanceIdToNode.containsKey(s))
                return s;
            ++id;
        }
    }

    private String getUniqueInstanceId(String base) {
        int id = 0;

        int i = base.length()-1;
        while (i >= 0) {
            char c = base.charAt(i);
            if (!(c >= '0' && c <= '9'))
                break;
            --i;
        }
        int leading_zeros = 0;
        if (i < base.length()-1) {
            id = Integer.parseInt(base.substring(i+1));
            leading_zeros = base.substring(i+1).length();
            base = base.substring(0, i+1);
        }
        String format_string;
        if (leading_zeros > 0)
            format_string = String.format("%%s%%0%dd", leading_zeros);
        else
            format_string = "%s%d";

        while (true) {
            String s = String.format(format_string, base, id);
            if (!instanceIdToNode.containsKey(s))
                return s;
            ++id;
        }
    }

    @Override
    protected boolean verifyChild(Node child) {
        return (child instanceof CollectionInstanceNode) || (child instanceof InstanceNode);
    }

    @Override
    public void resourceChanged(Resource resource) {
        try {
            setup();
            getScene().fireSceneEvent(new SceneEvent(SceneEvent.NODE_CHANGED, this));
        } catch (CreateException e) {
            e.printStackTrace();
        }
    }
}

