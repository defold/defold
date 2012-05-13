package com.dynamo.cr.properties;

import java.lang.annotation.Annotation;

import org.eclipse.core.runtime.IStatus;

public interface IValidator<T, A extends Annotation, U extends IPropertyObjectWorld> {

    IStatus validate(A validationParameters, Object object, String property, T value, U world);

}
