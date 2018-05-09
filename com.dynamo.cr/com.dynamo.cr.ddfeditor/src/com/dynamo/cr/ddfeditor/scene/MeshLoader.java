package com.dynamo.cr.ddfeditor.scene;

import java.io.IOException;
import java.io.InputStream;

import javax.xml.stream.XMLStreamException;

import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.IProgressMonitor;
import com.dynamo.bob.pipeline.ColladaUtil;
import com.dynamo.bob.pipeline.LoaderException;
import com.dynamo.cr.sceneed.core.ILoaderContext;
import com.dynamo.cr.sceneed.core.INodeLoader;
import com.google.protobuf.Message;
import com.dynamo.rig.proto.Rig.Mesh;
import com.dynamo.rig.proto.Rig.MeshEntry;
import com.dynamo.rig.proto.Rig.MeshSet;
import com.dynamo.rig.proto.Rig.Skeleton;

public class MeshLoader implements INodeLoader<MeshNode> {

    @Override
    public MeshNode load(ILoaderContext context, InputStream contents)
            throws IOException, CoreException {

        try {
            MeshSet.Builder meshSetBuilder = MeshSet.newBuilder();
            ColladaUtil.loadMesh(contents, meshSetBuilder);
            MeshSet meshSet = meshSetBuilder.build();
            if(meshSet.getMeshEntriesCount() == 0) {
                return new MeshNode();
            }
            return new MeshNode(meshSet.getMeshAttachments(0));
        } catch (XMLStreamException e) {
            return invalidMeshNode(e);
        } catch (LoaderException e) {
            return invalidMeshNode(e);
        }
    }

    private MeshNode invalidMeshNode(Exception e) {
        MeshNode node = new MeshNode();
        node.setLoadError(e);
        return node;
    }

    @Override
    public Message buildMessage(ILoaderContext context, MeshNode node,
            IProgressMonitor monitor) throws IOException, CoreException {
        return null;
    }

}
