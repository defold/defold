package com.dynamo.cr.sceneed.core;

import org.eclipse.core.resources.IContainer;
import org.eclipse.core.resources.IFile;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Path;
import org.eclipse.core.runtime.Status;
import org.eclipse.osgi.util.NLS;

import com.dynamo.cr.properties.IValidator;
import com.dynamo.cr.properties.ValidateUtil;

public class ResourceValidator implements IValidator<String, Resource, INodeWorld> {

    @Override
    public IStatus validate(Resource validationParameters, String property, String value, INodeWorld world, Class<? extends NLS> nls) {
        if (value != null && value.length() > 0) {
            IContainer contentRoot = world.getContentRoot();
            IFile file = contentRoot.getFile(new Path(value));
            if (file.exists()) {
                return Status.OK_STATUS;
            } else {
                String messageKey = String.format("%s_%s_%s_NOT_FOUND", world.getClass().getSimpleName(), this.getClass().getSimpleName(), property);
                return new Status(IStatus.ERROR, "com.dynamo.cr.tileeditor", ValidateUtil.bind(nls, messageKey, value, "Resource not found"));
            }
        } else {
            String messageKey = String.format("%s_%s_%s_NOT_SPECIFIED", world.getClass().getSimpleName(), this.getClass().getSimpleName(), property);
            return new Status(IStatus.INFO, "com.dynamo.cr.tileeditor", ValidateUtil.bind(nls, messageKey, value, "No resource specified"));
        }
    }

}
