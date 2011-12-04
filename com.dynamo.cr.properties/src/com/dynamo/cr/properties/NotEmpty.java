package com.dynamo.cr.properties;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

import org.eclipse.core.runtime.IStatus;

@Validator(validator=NotEmptyValidator.class)
@Retention(RetentionPolicy.RUNTIME)
public @interface NotEmpty {
    int severity() default IStatus.INFO;
}
