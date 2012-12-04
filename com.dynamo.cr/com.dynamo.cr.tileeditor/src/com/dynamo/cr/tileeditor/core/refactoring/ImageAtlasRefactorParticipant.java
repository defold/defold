package com.dynamo.cr.tileeditor.core.refactoring;

import org.eclipse.core.resources.IContainer;
import org.eclipse.core.resources.IFile;
import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Path;
import org.eclipse.core.runtime.Status;

import com.dynamo.atlas.proto.Atlas.AtlasAnimation;
import com.dynamo.atlas.proto.Atlas.ImageAtlas;
import com.dynamo.cr.editor.core.GenericRefactorParticipant;
import com.dynamo.cr.editor.core.ResourceRefactorContext;
import com.dynamo.cr.tileeditor.Activator;
import com.google.protobuf.Message.Builder;
import com.google.protobuf.TextFormat;
import com.google.protobuf.TextFormat.ParseException;

public class ImageAtlasRefactorParticipant extends GenericRefactorParticipant {

    private ImageAtlas imageAtlas;

    public ImageAtlasRefactorParticipant() {
    }

    @Override
    public Builder getBuilder() {
        return ImageAtlas.newBuilder();
    }

    @Override
    public String deleteReferences(ResourceRefactorContext context,
            String resource, IFile reference) throws CoreException {
        return modify(context, resource, reference, null);
    }

    @Override
    public String updateReferences(ResourceRefactorContext context,
            String resource, IFile reference, String newPath)
            throws CoreException {
        return modify(context, resource, reference, newPath);
    }

    private String modify(ResourceRefactorContext context, String resource,
            IFile reference, String newPath) throws CoreException {
        IContainer contentRoot = context.getContentRoot();
        boolean changed = false;

        ImageAtlas.Builder builder = ImageAtlas.newBuilder();
        try {
            TextFormat.merge(resource, builder);
            imageAtlas = builder.build();

            ImageAtlas.Builder newBuilder = ImageAtlas.newBuilder(imageAtlas);
            newBuilder.clearImages();
            newBuilder.clearAnimations();

            for (String image : imageAtlas.getImagesList()) {
                IFile imageFile = contentRoot.getFile(new Path(image));

                if (reference.equals(imageFile)) {
                    if (newPath == null) {
                    } else {
                        newBuilder.addImages(newPath);
                    }
                    changed = true;
                } else {
                    newBuilder.addImages(image);
                }
            }

            for (AtlasAnimation animation : imageAtlas.getAnimationsList()) {
                AtlasAnimation.Builder animBuilder = AtlasAnimation.newBuilder(animation);
                animBuilder.clearImages();
                for (String image : animation.getImagesList()) {
                    IFile imageFile = contentRoot.getFile(new Path(image));

                    if (reference.equals(imageFile)) {
                        if (newPath == null) {
                        } else {
                            animBuilder.addImages(newPath);
                        }
                        changed = true;
                    } else {
                        animBuilder.addImages(image);
                    }
                }
                newBuilder.addAnimations(animBuilder);
            }

            if (changed) {
                return newBuilder.build().toString();
            } else {
                return null;
            }
        } catch (ParseException e) {
            throw new CoreException(new Status(IStatus.ERROR, Activator.PLUGIN_ID, e.getMessage()));
        }
    }

}
