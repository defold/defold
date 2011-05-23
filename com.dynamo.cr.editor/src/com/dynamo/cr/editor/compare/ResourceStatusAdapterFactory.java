package com.dynamo.cr.editor.compare;

import org.eclipse.core.resources.IResource;
import org.eclipse.core.runtime.IAdapterFactory;

public class ResourceStatusAdapterFactory implements IAdapterFactory {

    @Override
    public Object getAdapter(Object adaptableObject, @SuppressWarnings("rawtypes") Class adapterType) {
        if (adaptableObject instanceof ResourceStatus && IResource.class.equals(adapterType)) {
            return ((ResourceStatus)adaptableObject).getResource();
        }
        return null;
    }

    @SuppressWarnings("rawtypes")
    @Override
    public Class[] getAdapterList() {
        return new Class[] { IResource.class };
    }

}
