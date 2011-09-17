package com.dynamo.cr.properties;

import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Status;
import org.eclipse.osgi.util.NLS;

public class RangeValidator implements IValidator<Number, Range, IPropertyObjectWorld> {

    @Override
    public IStatus validate(Range validationParameters, String property, Number value, IPropertyObjectWorld world, Class<? extends NLS> nls) {
        double doubleValue = value.doubleValue();
        if (doubleValue >= validationParameters.min() && doubleValue <= validationParameters.max()) {
            return Status.OK_STATUS;
        } else {
            String messageKey = String.format("%s_%s_%s", world.getClass().getSimpleName(), this.getClass().getSimpleName(), property);
            return new Status(IStatus.ERROR, "com.dynamo", ValidateUtil.bind(nls, messageKey, "Value out of range"));
        }
    }
}
