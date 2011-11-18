package com.dynamo.cr.sceneed.ui;

import java.io.IOException;
import java.io.OutputStream;
import java.io.OutputStreamWriter;

import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.IProgressMonitor;

import com.dynamo.cr.sceneed.core.ISceneView;
import com.dynamo.cr.sceneed.core.ISceneView.Context;
import com.google.protobuf.Message;
import com.google.protobuf.TextFormat;

public abstract class NodePresenter implements ISceneView.NodePresenter {

    @Override
    public final void onSaveMessage(Context context, Message message, OutputStream contents, IProgressMonitor monitor) throws IOException, CoreException {
        try {
            OutputStreamWriter writer = new OutputStreamWriter(contents);
            try {
                TextFormat.print(message, writer);
                writer.flush();
            } finally {
                writer.close();
            }
        } catch (IOException e) {
            throw e;
        }
    }

}
