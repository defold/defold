package com.dynamo.cr.go.core;

import java.util.Comparator;

import com.dynamo.cr.properties.NotEmpty;
import com.dynamo.cr.properties.Property;
import com.dynamo.cr.sceneed.core.Node;

@SuppressWarnings("serial")
public class CollectionNode extends Node {

    @Property
    @NotEmpty
    private String name = "";

    @Property
    private boolean scaleAlongZ = false;

    private String path;

    public CollectionNode() {
        setFlags(Flags.NO_SCALE_ALONG_Z);
    }

    public String getName() {
        return this.name;
    }

    public void setName(String name) {
        this.name = name;
    }

    public boolean isScaleAlongZ() {
        return this.scaleAlongZ;
    }

    private void syncScaleAlongZ(Node node) {
        if (this.scaleAlongZ) {
            node.clearFlagsRecursively(Flags.NO_SCALE_ALONG_Z);
        } else {
            node.setFlagsRecursively(Flags.NO_SCALE_ALONG_Z);
        }
    }

    public void setScaleAlongZ(boolean scaleAlongZ) {
        this.scaleAlongZ = scaleAlongZ;
        syncScaleAlongZ(this);
    }

    public String getPath() {
        return this.path;
    }

    public void setPath(String path) {
        this.path = path;
    }

    @Override
    protected void childAdded(Node child) {
        sortInstances();
        syncScaleAlongZ(child);
    }

    public void sortInstances() {
        sortChildren(new Comparator<Node>() {
            @Override
            public int compare(Node o1, Node o2) {
                if ((o1 instanceof GameObjectInstanceNode && o2 instanceof CollectionInstanceNode)
                        || (o1 instanceof CollectionInstanceNode && o2 instanceof GameObjectInstanceNode)) {
                    if (o1 instanceof GameObjectInstanceNode) {
                        return 1;
                    } else {
                        return -1;
                    }
                } else {
                    String id1 = ((InstanceNode)o1).getId();
                    String id2 = ((InstanceNode)o2).getId();
                    return id1.compareTo(id2);
                }

            }
        });
    }
}
