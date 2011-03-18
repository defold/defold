package com.dynamo.cr.contenteditor.scene;

import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.OutputStream;
import java.io.OutputStreamWriter;
import java.util.HashMap;
import java.util.Map;

import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.IProgressMonitor;

import com.dynamo.cr.contenteditor.math.MathUtil;
import com.dynamo.cr.contenteditor.resource.IResourceLoaderFactory;
import com.dynamo.gameobject.proto.GameObject.CollectionDesc;
import com.dynamo.gameobject.proto.GameObject.CollectionInstanceDesc;
import com.dynamo.gameobject.proto.GameObject.InstanceDesc;
import com.google.protobuf.TextFormat;

public class CollectionNodeLoader implements INodeLoader {

    @Override
    public Node load(IProgressMonitor monitor, Scene scene, String name, InputStream stream, INodeLoaderFactory factory, IResourceLoaderFactory resourceFactory, Node parent) throws IOException,
            LoaderException, CoreException {

        InputStreamReader reader = new InputStreamReader(stream);

        CollectionDesc.Builder b = CollectionDesc.newBuilder();
        TextFormat.merge(reader, b);
        CollectionDesc desc = b.build();
        monitor.beginTask(name, desc.getInstancesCount() + desc.getCollectionInstancesCount());

        CollectionNode node = new CollectionNode(desc.getName(), scene, name);

        for (CollectionInstanceDesc cid : desc.getCollectionInstancesList()) {
            // detect recursion
            String ancestorCollection = name;
            Node subNode;
            if (!name.equals(cid.getCollection()) && parent != null) {
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
            if (!ancestorCollection.equals(cid.getCollection())) {
                try {
                    subCollection = factory.load(monitor, scene, cid.getCollection(), node);
                    monitor.worked(1);
                } catch (IOException e) {
                    subCollection = new CollectionNode(cid.getCollection(), scene, cid.getCollection());
                    subCollection.setError(Node.ERROR_FLAG_RESOURCE_ERROR, e.getMessage());
                    factory.reportError(e.getMessage());
                }
            }

            subNode = new CollectionInstanceNode(cid.getId(), scene, cid.getCollection(), subCollection);
            if (subCollection == null) {
                subNode.setError(Node.ERROR_FLAG_RECURSION, String.format("The resource %s is already used in a collection above this item.", cid.getCollection()));
            }

            subNode.setLocalTranslation(MathUtil.toVector4(cid.getPosition()));
            subNode.setLocalRotation(MathUtil.toQuat4(cid.getRotation()));

            node.addNode(subNode);
        }

        // Needs to be map of lists to handle duplicated ids
        Map<String, Node> idToNode = new HashMap<String, Node>();
        for (InstanceDesc id : desc.getInstancesList()) {
            Node prototype;
            try {
                prototype = factory.load(monitor, scene, id.getPrototype(), null);
            }
            catch (IOException e) {
                prototype = new PrototypeNode(id.getPrototype(), scene);
                prototype.setError(Node.ERROR_FLAG_RESOURCE_ERROR, e.getMessage());
                factory.reportError(e.getMessage());
            }
            monitor.worked(1);

            InstanceNode in = new InstanceNode(id.getId(), scene, id.getPrototype(), prototype);
            idToNode.put(id.getId(), in);
            in.setLocalTranslation(MathUtil.toVector4(id.getPosition()));
            in.setLocalRotation(MathUtil.toQuat4(id.getRotation()));
            node.addNode(in);
        }

        for (InstanceDesc id : desc.getInstancesList()) {
            Node parentInstance = idToNode.get(id.getId());
            for (String childId : id.getChildrenList()) {
                Node child = idToNode.get(childId);
                if (child == null)
                    throw new LoaderException(String.format("Child %s not found", childId));

                if (parentInstance.acceptsChild(child)) {
                    node.removeNode(child);
                    parentInstance.addNode(child);
                } else {
                    Node instanceNode = new InstanceNode(childId, scene, null, null);
                    parentInstance.addNode(instanceNode);
                    instanceNode.setError(Node.ERROR_FLAG_RECURSION, String.format("The instance %s can not be a child of %s since it occurs above %s in the hierarchy.", childId, id.getId(), id.getId()));
                }
            }
        }

        return node;
    }

    @Override
    public void save(IProgressMonitor monitor, String name, Node node, OutputStream stream,
            INodeLoaderFactory loaderFactory) throws IOException, LoaderException {
        CollectionNode coll_node = (CollectionNode) node;
        CollectionDesc desc = coll_node.buildDescriptor();

        OutputStreamWriter writer = new OutputStreamWriter(stream);
        TextFormat.print(desc, writer);
        writer.close();
    }
}

