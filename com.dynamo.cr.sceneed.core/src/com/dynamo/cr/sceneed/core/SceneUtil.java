package com.dynamo.cr.sceneed.core;

import java.io.IOException;
import java.io.OutputStream;
import java.io.OutputStreamWriter;

import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.IProgressMonitor;

import com.google.protobuf.Message;
import com.google.protobuf.TextFormat;

public class SceneUtil {
    public static void saveMessage(Message message, OutputStream contents, IProgressMonitor monitor) throws IOException, CoreException {
        OutputStreamWriter writer = new OutputStreamWriter(contents);
        try {
            TextFormat.print(message, writer);
            writer.flush();
        } finally {
            writer.close();
        }
    }
}
