package com.dynamo.cr.properties;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

@Validator(validator=NotEmptyValidator.class)
@Retention(RetentionPolicy.RUNTIME)
public @interface NotEmpty {
}
