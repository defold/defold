package com.dynamo.cr.go.core.test;

import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;

import javax.vecmath.Point3f;

import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.IProgressMonitor;

import com.dynamo.cr.sceneed.core.ILoaderContext;
import com.dynamo.cr.sceneed.core.INodeLoader;
import com.dynamo.proto.DdfMath.Point3;
import com.google.protobuf.Message;
import com.google.protobuf.TextFormat;

public class DummyComponentLoader implements INodeLoader<DummyComponentNode> {

    @Override
    public DummyComponentNode load(ILoaderContext context, InputStream contents)
            throws IOException, CoreException {
        InputStreamReader reader = new InputStreamReader(contents);
        Point3.Builder builder = Point3.newBuilder();
        TextFormat.merge(reader, builder);
        Point3 p = builder.build();
        DummyComponentNode node = new DummyComponentNode();
        node.setPosition(new Point3f(p.getX(), p.getY(), p.getZ()));
        return node;
    }

    @Override
    public Message buildMessage(ILoaderContext context, DummyComponentNode node,
            IProgressMonitor monitor) throws IOException, CoreException {
        Point3.Builder builder = Point3.newBuilder();
        Point3f p = node.getPosition();
        return builder.setX(p.getX()).setY(p.getY()).setZ(p.getZ()).build();
    }

}
