package com.dynamo.cr.ddfeditor.scene;

import java.io.IOException;
import java.io.InputStream;

import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.IProgressMonitor;

import com.dynamo.cr.sceneed.core.ILoaderContext;
import com.dynamo.cr.sceneed.core.INodeLoader;
import com.dynamo.cr.sceneed.core.util.LoaderUtil;
import com.dynamo.sound.proto.Sound.SoundDesc;
import com.dynamo.sound.proto.Sound.SoundDesc.Builder;
import com.google.protobuf.Message;

public class SoundLoader implements INodeLoader<SoundNode>  {

    public SoundLoader() {
    }

    @Override
    public SoundNode load(ILoaderContext context, InputStream contents)
            throws IOException, CoreException {


        Builder b = SoundDesc.newBuilder();
        LoaderUtil.loadBuilder(b, contents);
        SoundDesc soundDesc = b.build();

        SoundNode node = new SoundNode(soundDesc);
        return node;
    }

    @Override
    public Message buildMessage(ILoaderContext context, SoundNode node,
            IProgressMonitor monitor) throws IOException, CoreException {
        return node.buildMessage();
    }

}
