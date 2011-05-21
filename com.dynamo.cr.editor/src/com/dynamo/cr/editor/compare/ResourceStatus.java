package com.dynamo.cr.editor.compare;

import org.eclipse.core.resources.IResource;
import org.eclipse.core.runtime.IAdaptable;
import org.eclipse.core.runtime.Platform;

import com.dynamo.cr.editor.Activator;
import com.dynamo.cr.editor.core.EditorUtil;
import com.dynamo.cr.protocol.proto.Protocol.BranchStatus.Status;

public class ResourceStatus implements IAdaptable {
    private Status status;
    private IResource resource;

    public ResourceStatus(Status status) {
        this.status = status;
        this.resource = EditorUtil.getContentRoot(Activator.getDefault().getProject()).findMember(status.getName());
    }

    public Status getStatus() {
        return status;
    }

    public IResource getResource() {
        return resource;
    }

    @Override
    public Object getAdapter(@SuppressWarnings("rawtypes") Class adapter) {
        return Platform.getAdapterManager().getAdapter(this, adapter);
    }
}
