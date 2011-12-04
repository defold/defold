package com.dynamo.cr.properties;

import org.eclipse.core.resources.IContainer;
import org.eclipse.core.resources.IFile;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Path;
import org.eclipse.core.runtime.Status;

public class ResourceValidator implements IValidator<String, Resource, IPropertyObjectWorld> {

    @Override
    public IStatus validate(Resource validationParameters, Object object, String property, String value, IPropertyObjectWorld world) {
        if (value != null && value.length() > 0) {
            IContainer contentRoot = world.getContentRoot();
            IFile file = contentRoot.getFile(new Path(value));
            if (!file.exists()) {
                return ValidatorUtil.createStatus(object, property, IStatus.ERROR, "NOT_FOUND", Messages.ResourceValidator_NOT_FOUND, new Object[] { value });
            }
        }
        return Status.OK_STATUS;
    }

}
