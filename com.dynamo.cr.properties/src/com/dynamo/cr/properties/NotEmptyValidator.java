package com.dynamo.cr.properties;

import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Status;
import org.eclipse.osgi.util.NLS;

public class NotEmptyValidator implements IValidator<String, NotEmpty, IPropertyObjectWorld> {

    @Override
    public IStatus validate(NotEmpty validationParameters, String property, String value, IPropertyObjectWorld world, Class<? extends NLS> nls) {
        if (value != null && value.length() > 0) {
            return Status.OK_STATUS;
        } else {
            String messageKey = String.format("%s_%s_%s", world.getClass().getSimpleName(), this.getClass().getSimpleName(), property);
            return new Status(IStatus.ERROR, "com.dynamo", ValidateUtil.bind(nls, messageKey, "Empty string"));
        }
    }
}
