package com.dynamo.cr.properties;

import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;

import org.eclipse.ui.views.properties.IPropertyDescriptor;

/**
 * Annotation for dynamic properties. The annotated method
 * must return an array of {@link IPropertyDescriptor} with no arguments.
 * @author chmu
 *
 */
@Retention(RetentionPolicy.RUNTIME)
@Target(value = ElementType.METHOD)
public @interface DynamicProperties {

}
