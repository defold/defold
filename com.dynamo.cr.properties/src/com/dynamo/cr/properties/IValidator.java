package com.dynamo.cr.properties;

import java.lang.annotation.Annotation;
import java.util.ResourceBundle;

import org.eclipse.core.runtime.IStatus;
import org.eclipse.osgi.util.NLS;

public interface IValidator<T, A extends Annotation, U extends IPropertyObjectWorld> {

    IStatus validate(A validationParameters, String property, T value, U world, Class<? extends NLS> nls);

}
