package com.dynamo.cr.ddfeditor.scene;

import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;

import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.IProgressMonitor;

import com.dynamo.cr.sceneed.core.ILoaderContext;
import com.dynamo.cr.sceneed.core.INodeLoader;
import com.dynamo.cr.sceneed.core.util.LoaderUtil;
import com.dynamo.label.proto.Label.LabelDesc;
import com.dynamo.label.proto.Label.LabelDesc.Builder;
import com.google.protobuf.Message;
import com.google.protobuf.TextFormat;

public class LabelLoader implements INodeLoader<LabelNode> {

    @Override
    public LabelNode load(ILoaderContext context, InputStream contents)
            throws IOException, CoreException {

        InputStreamReader reader = new InputStreamReader(contents);
        Builder builder = LabelDesc.newBuilder();
        TextFormat.merge(reader, builder);
        LabelDesc ddf = builder.build();

        LabelNode node = new LabelNode();

        node.setText(ddf.getText());

        node.setColor(LoaderUtil.toRGB(ddf.getColor()));
        node.setAlpha(ddf.getColor().getW());
        node.setOutline(LoaderUtil.toRGB(ddf.getOutline()));
        node.setOutlineAlpha(ddf.getOutline().getW());
        node.setShadow(LoaderUtil.toRGB(ddf.getShadow()));
        node.setShadowAlpha(ddf.getShadow().getW());

        node.setScale(LoaderUtil.toVector3(ddf.getScale()));
        node.setSize(LoaderUtil.toVector3(ddf.getSize()));
        node.setLineBreak(ddf.getLineBreak());
        node.setLeading(ddf.getLeading());
        node.setTracking(ddf.getTracking());
        node.setPivot(ddf.getPivot());

        node.setFont(ddf.getFont());
        node.setMaterial(ddf.getMaterial());
        node.setBlendMode(ddf.getBlendMode());

        return node;
    }

    @Override
    public Message buildMessage(ILoaderContext context, LabelNode node,
            IProgressMonitor monitor) throws IOException, CoreException {

        LabelDesc.Builder builder = LabelDesc.newBuilder();
        builder.setText(node.getText());

        builder.setColor(LoaderUtil.toVector4(node.getColor(), (float)node.getAlpha()));
        builder.setOutline(LoaderUtil.toVector4(node.getOutline(), (float)node.getOutlineAlpha()));
        builder.setShadow(LoaderUtil.toVector4(node.getShadow(), (float)node.getShadowAlpha()));

        builder.setScale(LoaderUtil.toVector4(node.getScale()));
        builder.setSize(LoaderUtil.toVector4(node.getSize()));
        builder.setLineBreak(node.getLineBreak());
        builder.setLeading((float)node.getLeading());
        builder.setTracking((float)node.getTracking());
        builder.setPivot(node.getPivot());

        builder.setFont(node.getFont());
        builder.setMaterial(node.getMaterial());
        builder.setBlendMode(node.getBlendMode());

        return builder.build();
    }

}
