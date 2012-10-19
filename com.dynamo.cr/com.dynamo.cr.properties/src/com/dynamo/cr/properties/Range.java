package com.dynamo.cr.properties;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

@Validator(validator=RangeValidator.class)
@Retention(RetentionPolicy.RUNTIME)
public @interface Range {
    double min() default -Double.MAX_VALUE;
    double max() default Double.MAX_VALUE;
}
