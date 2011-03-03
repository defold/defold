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
import com.dynamo.ddf.DDF;
import com.dynamo.gameobject.ddf.GameObject;
import com.dynamo.gameobject.ddf.GameObject.CollectionDesc;
import com.dynamo.gameobject.ddf.GameObject.CollectionInstanceDesc;
import com.dynamo.gameobject.ddf.GameObject.InstanceDesc;

public class CollectionNodeLoader implements INodeLoader {

    @Override
    public Node load(IProgressMonitor monitor, Scene scene, String name, InputStream stream, INodeLoaderFactory factory, IResourceLoaderFactory resourceFactory, Node parent) throws IOException,
            LoaderException, CoreException {

        InputStreamReader reader = new InputStreamReader(stream);

        GameObject.CollectionDesc desc = DDF.loadTextFormat(reader, GameObject.CollectionDesc.class);
        monitor.beginTask(name, desc.m_Instances.size() + desc.m_CollectionInstances.size());

        CollectionNode node = new CollectionNode(desc.m_Name, scene, name);

        for (CollectionInstanceDesc cid : desc.m_CollectionInstances) {
            // detect recursion
            String ancestorCollection = name;
            Node subNode;
            if (!name.equals(cid.m_Collection) && parent != null) {
                Node ancestor = parent;
                ancestorCollection = ((CollectionNode)parent).getResource();
                while (!ancestorCollection.equals(cid.m_Collection) && ancestor != null) {
                    ancestor = ancestor.getParent();
                    if (ancestor != null && ancestor instanceof CollectionNode) {
                        ancestorCollection = ((CollectionNode)ancestor).getResource();
                    }
                }
            }
            Node subCollection = null;
            if (!ancestorCollection.equals(cid.m_Collection)) {
                try {
                    subCollection = factory.load(monitor, scene, cid.m_Collection, node);
                    monitor.worked(1);
                } catch (IOException e) {
                    subCollection = new CollectionNode(cid.m_Collection, scene, cid.m_Collection);
                    subCollection.setError(Node.ERROR_FLAG_RESOURCE_ERROR, e.getMessage());
                    factory.reportError(e.getMessage());
                }
            }

            subNode = new CollectionInstanceNode(cid.m_Id, scene, cid.m_Collection, subCollection);
            if (subCollection == null) {
                subNode.setError(Node.ERROR_FLAG_RECURSION, String.format("The resource %s is already used in a collection above this item.", cid.m_Collection));
            }

            subNode.setLocalTranslation(MathUtil.toVector4(cid.m_Position));
            subNode.setLocalRotation(MathUtil.toQuat4(cid.m_Rotation));

            node.addNode(subNode);
        }

        // Needs to be map of lists to handle duplicated ids
        Map<String, Node> idToNode = new HashMap<String, Node>();
        for (InstanceDesc id : desc.m_Instances) {
            Node prototype;
            try {
                prototype = factory.load(monitor, scene, id.m_Prototype, null);
            }
            catch (IOException e) {
                prototype = new PrototypeNode(id.m_Prototype, scene);
                prototype.setError(Node.ERROR_FLAG_RESOURCE_ERROR, e.getMessage());
                factory.reportError(e.getMessage());
            }
            monitor.worked(1);

            InstanceNode in = new InstanceNode(id.m_Id, scene, id.m_Prototype, prototype);
            idToNode.put(id.m_Id, in);
            in.setLocalTranslation(MathUtil.toVector4(id.m_Position));
            in.setLocalRotation(MathUtil.toQuat4(id.m_Rotation));
            node.addNode(in);
        }

        for (InstanceDesc id : desc.m_Instances) {
            Node parentInstance = idToNode.get(id.m_Id);
            for (String childId : id.m_Children) {
                Node child = idToNode.get(childId);
                if (child == null)
                    throw new LoaderException(String.format("Child %s not found", childId));

                if (parentInstance.acceptsChild(child)) {
                    node.removeNode(child);
                    parentInstance.addNode(child);
                } else {
                    Node instanceNode = new InstanceNode(childId, scene, null, null);
                    parentInstance.addNode(instanceNode);
                    instanceNode.setError(Node.ERROR_FLAG_RECURSION, String.format("The instance %s can not be a child of %s since it occurs above %s in the hierarchy.", childId, id.m_Id, id.m_Id));
                }
            }
        }

        return node;
    }

    @Override
    public void save(IProgressMonitor monitor, String name, Node node, OutputStream stream,
            INodeLoaderFactory loaderFactory) throws IOException, LoaderException {
        CollectionNode coll_node = (CollectionNode) node;
        CollectionDesc desc = coll_node.getDescriptor();

        OutputStreamWriter writer = new OutputStreamWriter(stream);
        DDF.saveTextFormat(desc, writer);
        writer.close();
    }
}

