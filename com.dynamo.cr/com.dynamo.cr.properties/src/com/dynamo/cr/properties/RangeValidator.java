package com.dynamo.cr.properties;

import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Status;

public class RangeValidator implements IValidator<Number, Range, IPropertyObjectWorld> {

    @Override
    public IStatus validate(Range validationParameters, Object object, String property, Number value, IPropertyObjectWorld world) {
        double doubleValue = value.doubleValue();
        double min = validationParameters.min();
        double max = validationParameters.max();
        if (doubleValue >= min && doubleValue <= max) {
            return Status.OK_STATUS;
        } else {
            return ValidatorUtil.createStatus(object, property, IStatus.ERROR, "OUTSIDE_RANGE", Messages.RangeValidator_OUTSIDE_RANGE, new Object[] { min, max });
        }
    }
}
