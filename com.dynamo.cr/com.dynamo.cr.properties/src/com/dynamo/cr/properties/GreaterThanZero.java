package com.dynamo.cr.properties;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

@Validator(validator=GreaterThanZeroValidator.class)
@Retention(RetentionPolicy.RUNTIME)
public @interface GreaterThanZero {
}
