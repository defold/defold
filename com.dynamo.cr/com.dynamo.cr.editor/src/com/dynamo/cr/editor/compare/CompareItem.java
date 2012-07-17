package com.dynamo.cr.editor.compare;

import java.io.ByteArrayInputStream;
import java.io.InputStream;

import org.eclipse.compare.IModificationDate;
import org.eclipse.compare.IStreamContentAccessor;
import org.eclipse.compare.ITypedElement;
import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Status;
import org.eclipse.swt.graphics.Image;

import com.dynamo.cr.client.IBranchClient;
import com.dynamo.cr.client.RepositoryException;
import com.dynamo.cr.editor.Activator;

public class CompareItem implements IStreamContentAccessor, ITypedElement,
        IModificationDate {

    IBranchClient branchClient;
    String path;
    String revision;

    public CompareItem(IBranchClient branchClient, String path, String revision) {
        this.branchClient = branchClient;
        this.path = path;
        this.revision = revision;
    }

    @Override
    public long getModificationDate() {
        // TODO Auto-generated method stub
        return 0;
    }

    @Override
    public String getName() {
        return this.path;
    }

    @Override
    public Image getImage() {
        return Activator.getDefault().getImage(path);
    }

    @Override
    public String getType() {
        return TEXT_TYPE;
    }

    @Override
    public InputStream getContents() throws CoreException {
        try {
            return new ByteArrayInputStream(this.branchClient.getResourceData(this.path, this.revision));
        } catch (RepositoryException e) {
            throw new CoreException(new Status(IStatus.ERROR, Activator.PLUGIN_ID, e.getMessage(), e));
        }
    }

}
