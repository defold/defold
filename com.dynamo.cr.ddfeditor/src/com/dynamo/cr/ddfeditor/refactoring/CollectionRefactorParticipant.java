package com.dynamo.cr.ddfeditor.refactoring;

import java.util.List;

import org.eclipse.core.resources.IContainer;
import org.eclipse.core.resources.IFile;
import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Path;
import org.eclipse.core.runtime.Status;

import com.dynamo.cr.ddfeditor.Activator;
import com.dynamo.cr.editor.core.IResourceRefactorParticipant;
import com.dynamo.cr.editor.core.ResourceRefactorContext;
import com.dynamo.gameobject.proto.GameObject.CollectionDesc;
import com.dynamo.gameobject.proto.GameObject.CollectionDesc.Builder;
import com.dynamo.gameobject.proto.GameObject.CollectionInstanceDesc;
import com.dynamo.gameobject.proto.GameObject.InstanceDesc;
import com.google.protobuf.TextFormat;
import com.google.protobuf.TextFormat.ParseException;

public class CollectionRefactorParticipant implements IResourceRefactorParticipant {

    @Override
    public String updateReferences(ResourceRefactorContext context,
                                   String resource,
                                   IFile reference,
                                   String newPath) throws CoreException {

        Builder builder = CollectionDesc.newBuilder();
        IContainer contentRoot = context.getContentRoot();
        try {
            TextFormat.merge(resource, builder);
            CollectionDesc collection = builder.build();

            Builder newBuilder = CollectionDesc.newBuilder(collection);
            newBuilder.clearInstances();
            newBuilder.clearCollectionInstances();

            boolean changed = false;
            List<InstanceDesc> instances = collection.getInstancesList();
            for (InstanceDesc instanceDesc : instances) {
                String prototype = instanceDesc.getPrototype();
                IFile prototypeFile = contentRoot.getFile(new Path(prototype));
                if (reference.equals(prototypeFile)) {
                    newBuilder.addInstances((InstanceDesc.newBuilder(instanceDesc).setPrototype(newPath)));
                    changed = true;
                }
                else {
                    newBuilder.addInstances(instanceDesc);
                }
            }

            List<CollectionInstanceDesc> collections = collection.getCollectionInstancesList();
            for (CollectionInstanceDesc collectionDesc : collections) {
                String collectionRef = collectionDesc.getCollection();
                IFile collecitonRefFile = contentRoot.getFile(new Path(collectionRef));
                if (reference.equals(collecitonRefFile)) {
                    newBuilder.addCollectionInstances((CollectionInstanceDesc.newBuilder(collectionDesc).setCollection(newPath)));
                    changed = true;
                }
                else {
                    newBuilder.addCollectionInstances(collectionDesc);
                }
            }

            if (changed) {
                return newBuilder.build().toString();
            }

        } catch (ParseException e) {
            throw new CoreException(new Status(IStatus.ERROR, Activator.PLUGIN_ID, e.getMessage()));
        }
        return null;
    }

    @Override
    public String deleteReferences(ResourceRefactorContext context,
                                   String resource,
                                   IFile reference) throws CoreException {
        return null;
    }
}
