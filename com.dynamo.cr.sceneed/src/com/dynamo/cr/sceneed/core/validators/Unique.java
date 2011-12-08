package com.dynamo.cr.sceneed.core.validators;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

import com.dynamo.cr.properties.Validator;

@Validator(validator=UniqueValidator.class)
@Retention(RetentionPolicy.RUNTIME)
public @interface Unique {
}
