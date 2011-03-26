package com.dynamo.cr.ddfeditor.refactoring;

import org.eclipse.core.resources.IContainer;
import org.eclipse.core.resources.IFile;
import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Path;
import org.eclipse.core.runtime.Status;

import com.dynamo.cr.editor.core.IResourceRefactorParticipant;
import com.dynamo.cr.editor.core.ResourceRefactorContext;
import com.dynamo.sprite.proto.Sprite.SpriteDesc;
import com.dynamo.sprite.proto.Sprite.SpriteDesc.Builder;
import com.google.protobuf.TextFormat;
import com.google.protobuf.TextFormat.ParseException;

public class SpriteRefactorParticipant implements IResourceRefactorParticipant {

    public SpriteRefactorParticipant() {
    }

    @Override
    public String updateReferences(ResourceRefactorContext context,
                                   String resource,
                                   IFile reference,
                                   String newPath) throws CoreException {

        Builder builder = SpriteDesc.newBuilder();

        IContainer contentRoot = context.getContentRoot();
        try {
            TextFormat.merge(resource, builder);
            SpriteDesc spriteDesc = builder.build();
            IFile texture = contentRoot.getFile(new Path(spriteDesc.getTexture()));
            if (reference.equals(texture)) {
                builder = SpriteDesc.newBuilder(spriteDesc);
                builder.setTexture(newPath);
                return builder.build().toString();
            }

        } catch (ParseException e) {
            throw new CoreException(new Status(IStatus.ERROR, "com.dynamo.cr.ddfeditor", e.getMessage()));
        }
        return null;
    }

    @Override
    public String deleteReferences(ResourceRefactorContext context,
            String resource, IFile reference) throws CoreException {
        Builder builder = SpriteDesc.newBuilder();

        IContainer contentRoot = context.getContentRoot();
        try {
            TextFormat.merge(resource, builder);
            SpriteDesc spriteDesc = builder.build();
            IFile texture = contentRoot.getFile(new Path(spriteDesc.getTexture()));
            if (reference.equals(texture)) {
                builder = SpriteDesc.newBuilder(spriteDesc);
                builder.setTexture("");
                return builder.build().toString();
            }

        } catch (ParseException e) {
            throw new CoreException(new Status(IStatus.ERROR, "com.dynamo.cr.ddfeditor", e.getMessage()));
        }
        return null;
    }
}
