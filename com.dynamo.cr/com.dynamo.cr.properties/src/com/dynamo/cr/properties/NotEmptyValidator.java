package com.dynamo.cr.properties;

import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Status;

public class NotEmptyValidator implements IValidator<String, NotEmpty, IPropertyObjectWorld> {

    @Override
    public IStatus validate(NotEmpty validationParameters, Object object, String property, String value, IPropertyObjectWorld world) {
        if (value != null && value.length() > 0) {
            return Status.OK_STATUS;
        } else {
            return ValidatorUtil.createStatus(object, property, validationParameters.severity(), "EMPTY", Messages.NotEmptyValidator_EMPTY);
        }
    }
}
