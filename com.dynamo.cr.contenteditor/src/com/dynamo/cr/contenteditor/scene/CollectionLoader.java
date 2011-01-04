package com.dynamo.cr.contenteditor.scene;

import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.OutputStream;
import java.io.OutputStreamWriter;
import java.util.HashMap;
import java.util.Map;

import org.eclipse.core.runtime.IProgressMonitor;

import com.dynamo.cr.contenteditor.math.MathUtil;
import com.dynamo.ddf.DDF;
import com.dynamo.gameobject.ddf.GameObject;
import com.dynamo.gameobject.ddf.GameObject.CollectionDesc;
import com.dynamo.gameobject.ddf.GameObject.CollectionInstanceDesc;
import com.dynamo.gameobject.ddf.GameObject.InstanceDesc;

public class CollectionLoader implements ILoader {

    @Override
    public Node load(IProgressMonitor monitor, Scene scene, String name, InputStream stream, LoaderFactory factory) throws IOException,
            LoaderException {

        InputStreamReader reader = new InputStreamReader(stream);

        GameObject.CollectionDesc desc = DDF.loadTextFormat(reader, GameObject.CollectionDesc.class);
        monitor.beginTask(name, desc.m_Instances.size() + desc.m_CollectionInstances.size());

        Map<String, Node> idToNode = new HashMap<String, Node>();
        CollectionNode node = new CollectionNode(scene, desc.m_Name, name);
        for (InstanceDesc id : desc.m_Instances) {
            Node prototype = factory.load(monitor, scene, id.m_Prototype);
            monitor.worked(1);

            InstanceNode in = new InstanceNode(scene, id.m_Id, id.m_Prototype, prototype);
            idToNode.put(id.m_Id, in);
            in.setLocalTranslation(MathUtil.toVector4(id.m_Position));
            in.setLocalRotation(MathUtil.toQuat4(id.m_Rotation));
            node.addNode(in);
        }

        for (InstanceDesc id : desc.m_Instances) {
            Node parent = idToNode.get(id.m_Id);
            for (String child_id : id.m_Children) {
                Node child = idToNode.get(child_id);
                if (child == null)
                    throw new LoaderException(String.format("Child %s not found", child_id));

                node.removeNode(child);
                parent.addNode(child);
            }
        }

        for (CollectionInstanceDesc cid : desc.m_CollectionInstances) {
            Node sub_collection = factory.load(monitor, scene, cid.m_Collection);
            monitor.worked(1);

            CollectionInstanceNode cin = new CollectionInstanceNode(scene, cid.m_Id, cid.m_Collection, sub_collection);

            cin.setLocalTranslation(MathUtil.toVector4(cid.m_Position));
            cin.setLocalRotation(MathUtil.toQuat4(cid.m_Rotation));
            node.addNode(cin);
        }

        return node;
    }

    @Override
    public void save(IProgressMonitor monitor, String name, Node node, OutputStream stream,
            LoaderFactory loaderFactory) throws IOException, LoaderException {
        CollectionNode coll_node = (CollectionNode) node;
        CollectionDesc desc = coll_node.getDescriptor();

        OutputStreamWriter writer = new OutputStreamWriter(stream);
        DDF.saveTextFormat(desc, writer);
        writer.close();
    }
}

