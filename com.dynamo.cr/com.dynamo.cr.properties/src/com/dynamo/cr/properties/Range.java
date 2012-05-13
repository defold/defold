package com.dynamo.cr.properties;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

@Validator(validator=RangeValidator.class)
@Retention(RetentionPolicy.RUNTIME)
public @interface Range {
    int min() default Integer.MIN_VALUE;
    int max() default Integer.MAX_VALUE;
}
