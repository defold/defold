package com.dynamo.cr.properties;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

@Validator(validator=NotZeroValidator.class)
@Retention(RetentionPolicy.RUNTIME)
public @interface NotZero {
}
