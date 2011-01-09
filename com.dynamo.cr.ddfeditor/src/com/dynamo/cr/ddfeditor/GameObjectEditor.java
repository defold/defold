package com.dynamo.cr.ddfeditor;

import org.eclipse.core.resources.IContainer;
import org.eclipse.core.resources.IResource;
import org.eclipse.core.runtime.IPath;
import org.eclipse.ui.IFileEditorInput;
import org.eclipse.ui.PlatformUI;
import org.eclipse.ui.dialogs.ListDialog;
import org.eclipse.ui.dialogs.ResourceListSelectionDialog;

import com.dynamo.cr.protobind.MessageNode;
import com.dynamo.gameobject.proto.GameObject.ComponentDesc;
import com.dynamo.gameobject.proto.GameObject.PrototypeDesc;
import com.google.protobuf.Descriptors.Descriptor;

public class GameObjectEditor extends DdfEditor {

    public GameObjectEditor() {
        super(PrototypeDesc.newBuilder().buildPartial());
    }

    @Override
    public boolean hasNewValueForPath(String path) {
        return path.equalsIgnoreCase("/components");
    }

    @Override
    public Object getNewValueForPath(String path) {
        if (path.equalsIgnoreCase("/components")) {

            IFileEditorInput fi = (IFileEditorInput) getEditorInput();
            IContainer contentRoot = findContentRoot(fi.getFile());
            if (contentRoot == null) {
                return null;
            }

            ResourceListSelectionDialog dialog = new ResourceListSelectionDialog(PlatformUI.getWorkbench().getActiveWorkbenchWindow().getShell(), fi.getFile().getProject(), IResource.FILE | IResource.DEPTH_INFINITE);
            int ret = dialog.open();

            if (ret == ListDialog.OK)
            {
                IResource r = (IResource) dialog.getResult()[0];
                IPath fullPath = r.getFullPath();
                String relPath = fullPath.makeRelativeTo(contentRoot.getFullPath()).toPortableString();
                return ComponentDesc.newBuilder().setResource(relPath).build();
            }

            return null;
        }
        return null;
    }

    @Override
    public String getMessageNodeLabelValue(MessageNode messageNode) {
        Descriptor descriptor = messageNode.getDescriptor();
        String name = descriptor.getName();
        if (name.equals("ComponentDesc")) {
            ComponentDesc componentDesc = (ComponentDesc) messageNode.build();
            return componentDesc.getResource();
        }
        return super.getMessageNodeLabelValue(messageNode);
    }
}

