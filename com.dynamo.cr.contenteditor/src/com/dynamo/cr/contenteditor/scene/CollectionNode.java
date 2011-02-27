package com.dynamo.cr.contenteditor.scene;

import java.util.HashMap;
import java.util.Map;

import com.dynamo.cr.contenteditor.editors.DrawContext;
import com.dynamo.gameobject.ddf.GameObject.CollectionDesc;
import com.dynamo.gameobject.ddf.GameObject.CollectionInstanceDesc;
import com.dynamo.gameobject.ddf.GameObject.InstanceDesc;

public class CollectionNode extends Node {

    private String name;
    private String resource;
    private Map<String, Node> collectionInstanceIdToNode = new HashMap<String, Node>();
    private Map<String, Node> instanceIdToNode = new HashMap<String, Node>();

    public CollectionNode(Scene scene, String name, String resource) {
        super(scene, FLAG_EDITABLE | FLAG_CAN_HAVE_CHILDREN);
        this.name = name;
        this.resource = resource;
    }

    @Override
    public String getName() {
        return resource;
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
        return resource;
    }

    void doGetDescriptor(CollectionDesc desc, InstanceNode in) {
        InstanceDesc id = in.getDesciptor();
        for (Node n2 : in.getChildren()) {
            if (n2 instanceof InstanceNode) {
                InstanceNode in2 = (InstanceNode) n2;
                id.m_Children.add(in2.getName());
                doGetDescriptor(desc, in2);
            }
        }
        desc.m_Instances.add(id);
    }

    public CollectionDesc getDescriptor() {
        CollectionDesc desc = new CollectionDesc();
        desc.m_Name = this.name;

        for (Node n : getChildren()) {

            if (n instanceof CollectionInstanceNode) {
                CollectionInstanceNode cin = (CollectionInstanceNode) n;
                CollectionInstanceDesc cid = cin.getDesciptor();
                desc.m_CollectionInstances.add(cid);
            }
            else if (n instanceof InstanceNode) {
                InstanceNode in = (InstanceNode) n;
                doGetDescriptor(desc, in);
            }
        }
        return desc;
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
        return (child instanceof CollectionInstanceNode) || (child instanceof InstanceNode) || (child instanceof BrokenNode);
    }
}

