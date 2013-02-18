package com.dynamo.cr.tileeditor.core.refactoring;

import java.util.List;

import org.eclipse.core.resources.IContainer;
import org.eclipse.core.resources.IFile;
import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Path;
import org.eclipse.core.runtime.Status;

import com.dynamo.atlas.proto.AtlasProto.Atlas;
import com.dynamo.atlas.proto.AtlasProto.AtlasAnimation;
import com.dynamo.atlas.proto.AtlasProto.AtlasImage;
import com.dynamo.cr.editor.core.GenericRefactorParticipant;
import com.dynamo.cr.editor.core.ResourceRefactorContext;
import com.google.protobuf.Message.Builder;
import com.google.protobuf.TextFormat;
import com.google.protobuf.TextFormat.ParseException;

public class AtlasRefactorParticipant extends GenericRefactorParticipant {
    @Override
    public Builder getBuilder() {
        return Atlas.newBuilder();
    }

    @Override
    public String deleteReferences(ResourceRefactorContext context,
                                   String resource,
                                   IFile reference) throws CoreException {
        Atlas.Builder builder = Atlas.newBuilder();

        IContainer contentRoot = context.getContentRoot();
        try {
            boolean changed = false;
            TextFormat.merge(resource, builder);
            List<AtlasImage> images = builder.getImagesList();
            builder.clearImages();
            for (AtlasImage image : images) {
                if (image.getImage().length() > 1) {
                    String tmp = image.getImage();
                    if (tmp.startsWith("/")) {
                        tmp = tmp.substring(1);
                    }
                    IFile file = contentRoot.getFile(new Path(tmp));
                    if (!reference.equals(file)) {
                        builder.addImages(image);
                    }
                }
            }
            changed = images.size() != builder.getImagesCount();
            int animCount = builder.getAnimationsCount();
            for (int i = 0; i < animCount; ++i) {
                AtlasAnimation.Builder animBuilder = AtlasAnimation.newBuilder(builder.getAnimations(i));
                List<AtlasImage> animImages = animBuilder.getImagesList();
                animBuilder.clearImages();
                for (AtlasImage image : animImages) {
                    if (image.getImage().length() > 1) {
                        String tmp = image.getImage();
                        if (tmp.startsWith("/")) {
                            tmp = tmp.substring(1);
                        }
                        IFile file = contentRoot.getFile(new Path(tmp));
                        if (!reference.equals(file)) {
                            animBuilder.addImages(image);
                        }
                    }
                }
                if (!changed) {
                    changed = animImages.size() != animBuilder.getImagesCount();
                }
                builder.setAnimations(i, animBuilder.build());
            }
            if (changed) {
                return builder.build().toString();
            }
        } catch (ParseException e) {
            throw new CoreException(new Status(IStatus.ERROR, "com.dynamo.cr.tileeditor", e.getMessage()));
        }
        return null;
    }
}
