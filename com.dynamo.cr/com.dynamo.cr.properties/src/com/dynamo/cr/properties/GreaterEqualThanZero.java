package com.dynamo.cr.properties;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

@Validator(validator=GreaterEqualThanZeroValidator.class)
@Retention(RetentionPolicy.RUNTIME)
public @interface GreaterEqualThanZero {
}
