package com.dynamo.cr.ddfeditor.refactoring;

import java.util.List;

import org.eclipse.core.resources.IContainer;
import org.eclipse.core.resources.IFile;
import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Path;
import org.eclipse.core.runtime.Status;

import com.dynamo.cr.editor.core.IResourceRefactorParticipant;
import com.dynamo.cr.editor.core.ResourceRefactorContext;
import com.dynamo.gui.proto.Gui.SceneDesc;
import com.dynamo.gui.proto.Gui.SceneDesc.Builder;
import com.dynamo.gui.proto.Gui.SceneDesc.FontDesc;
import com.dynamo.gui.proto.Gui.SceneDesc.TextureDesc;
import com.google.protobuf.TextFormat;
import com.google.protobuf.TextFormat.ParseException;

public class GuiRefactorParticipant implements IResourceRefactorParticipant {

    public GuiRefactorParticipant() {
    }

    @Override
    public String updateReferences(ResourceRefactorContext context,
            String resource, IFile reference, String newPath)
            throws CoreException {

        Builder builder = SceneDesc.newBuilder();

        IContainer contentRoot = context.getContentRoot();
        try {
            TextFormat.merge(resource, builder);
            SceneDesc sceneDesc = builder.build();
            Builder newBuilder = SceneDesc.newBuilder(sceneDesc);
            newBuilder.clearTextures();
            newBuilder.clearFonts();

            boolean changed = false;
            List<TextureDesc> textures = sceneDesc.getTexturesList();
            for (TextureDesc textureDesc : textures) {
                IFile texture = contentRoot.getFile(new Path(textureDesc.getTexture()));
                if (reference.equals(texture)) {
                    TextureDesc.Builder newTextureDesc = TextureDesc.newBuilder(textureDesc).setTexture(newPath);
                    newBuilder.addTextures(newTextureDesc);
                    changed = true;
                }
                else {
                    newBuilder.addTextures(textureDesc);
                }
            }

            List<FontDesc> fonts = sceneDesc.getFontsList();
            for (FontDesc fontDesc : fonts) {
                IFile font = contentRoot.getFile(new Path(fontDesc.getFont()));
                if (reference.equals(font)) {
                    FontDesc.Builder newFontDesc = FontDesc.newBuilder(fontDesc).setFont(newPath);
                    newBuilder.addFonts(newFontDesc);
                    changed = true;
                }
                else {
                    newBuilder.addFonts(fontDesc);
                }
            }

            if (changed)
                return newBuilder.build().toString();
            else
                return null;

        } catch (ParseException e) {
            throw new CoreException(new Status(IStatus.ERROR, "com.dynamo.cr.ddfeditor", e.getMessage()));
        }
    }

    @Override
    public String deleteReferences(ResourceRefactorContext context,
            String resource, IFile reference) throws CoreException {
        return null;
    }

}
