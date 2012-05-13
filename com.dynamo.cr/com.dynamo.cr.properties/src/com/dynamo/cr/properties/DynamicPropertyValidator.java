package com.dynamo.cr.properties;

import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;

/**
 * Annotation for property validator. The annotated method
 * must return {@link IValidator} given a {@link IPropertyObjectWorld}
 * @author chmu
 *
 */
@Retention(RetentionPolicy.RUNTIME)
@Target(value = ElementType.METHOD)
public @interface DynamicPropertyValidator {

}
