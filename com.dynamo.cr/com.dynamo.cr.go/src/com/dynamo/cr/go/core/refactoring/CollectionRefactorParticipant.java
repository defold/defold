package com.dynamo.cr.go.core.refactoring;

import java.util.List;

import org.eclipse.core.resources.IContainer;
import org.eclipse.core.resources.IFile;
import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Status;

import com.dynamo.cr.editor.core.GenericRefactorParticipant;
import com.dynamo.cr.editor.core.ResourceRefactorContext;
import com.dynamo.cr.go.Constants;
import com.dynamo.cr.protobind.MessageNode;
import com.dynamo.gameobject.proto.GameObject.CollectionDesc;
import com.dynamo.gameobject.proto.GameObject.EmbeddedInstanceDesc;
import com.dynamo.gameobject.proto.GameObject.CollectionDesc.Builder;
import com.dynamo.gameobject.proto.GameObject.PrototypeDesc;
import com.google.protobuf.TextFormat;
import com.google.protobuf.TextFormat.ParseException;

public class CollectionRefactorParticipant extends GenericRefactorParticipant {

    public CollectionRefactorParticipant() {
    }


    @Override
    public String updateReferences(ResourceRefactorContext context,
            String resource, IFile reference, String newPath) throws CoreException {
        Builder builder = CollectionDesc.newBuilder();

        IContainer contentRoot = context.getContentRoot();
        try {
            TextFormat.merge(resource, builder);
            CollectionDesc collection = builder.build();

            Builder newBuilder = CollectionDesc.newBuilder(collection);
            newBuilder.clearEmbeddedInstances();

            boolean changed = false;
            List<EmbeddedInstanceDesc> originalList = collection.getEmbeddedInstancesList();
            for (EmbeddedInstanceDesc desc : originalList) {

                PrototypeDesc.Builder protoBuilder = PrototypeDesc.newBuilder();
                TextFormat.merge(desc.getData(), protoBuilder);
                PrototypeDesc tmp = protoBuilder.build();
                MessageNode node = new MessageNode(tmp);
                boolean embeddedChanged = GenericRefactorParticipant.doUpdateReferences(contentRoot, reference, newPath, node, node);
                if (embeddedChanged) {
                    EmbeddedInstanceDesc.Builder b = EmbeddedInstanceDesc.newBuilder();
                    b.mergeFrom(desc);
                    b.setData(TextFormat.printToString(node.build()));
                    newBuilder.addEmbeddedInstances(b.build());
                    changed = true;
                }
                else {
                    newBuilder.addEmbeddedInstances(desc);
                }
            }

            if (changed) {
                return newBuilder.build().toString();
            }

        } catch (ParseException e) {
            throw new CoreException(new Status(IStatus.ERROR, Constants.PLUGIN_ID, e.getMessage()));
        }
        return null;
    }

    @Override
    public Builder getBuilder() {
        return CollectionDesc.newBuilder();
    }

}
