package com.dynamo.cr.properties;

import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Status;

public class GreaterEqualThanZeroValidator implements IValidator<Number, GreaterEqualThanZero, IPropertyObjectWorld> {

    @Override
    public IStatus validate(GreaterEqualThanZero validationParameters, Object object, String property, Number value, IPropertyObjectWorld world) {
        double doubleValue = value.doubleValue();
        if (doubleValue >= 0) {
            return Status.OK_STATUS;
        } else {
            return ValidatorUtil.createStatus(object, property, IStatus.ERROR, "OUTSIDE_RANGE", Messages.GreaterEqualThanZero_OUTSIDE_RANGE, null);
        }
    }
}
