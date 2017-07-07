package com.dynamo.cr.ddfeditor.scene;

import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;

import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.IProgressMonitor;

import com.dynamo.camera.proto.Camera.CameraDesc;
import com.dynamo.cr.sceneed.core.ILoaderContext;
import com.dynamo.cr.sceneed.core.INodeLoader;
import com.google.protobuf.Message;
import com.google.protobuf.TextFormat;

public class CameraLoader implements INodeLoader<CameraNode> {

    @Override
    public CameraNode load(ILoaderContext context, InputStream contents)
            throws IOException, CoreException {
        InputStreamReader reader = new InputStreamReader(contents);
        CameraDesc.Builder builder = CameraDesc.newBuilder();
        TextFormat.merge(reader, builder);
        CameraDesc desc = builder.build();
        CameraNode camera = new  CameraNode();
        camera.setAspectRatio(desc.getAspectRatio());
        camera.setFov(desc.getFov());
        camera.setNearZ(desc.getNearZ());
        camera.setFarZ(desc.getFarZ());
        camera.setautoAspectRatio(desc.getAutoAspectRatio());
        return camera;
    }



    @Override
    public Message buildMessage(ILoaderContext context, CameraNode node,
            IProgressMonitor monitor) throws IOException, CoreException {
        CameraDesc.Builder builder = CameraDesc.newBuilder();
        builder.setAspectRatio(node.getAspectRatio());
        builder.setFov(node.getFov());
        builder.setNearZ(node.getNearZ());
        builder.setFarZ(node.getFarZ());
        builder.setAutoAspectRatio(node.getAutoAspectRatio());
        return builder.build();
    }

}
