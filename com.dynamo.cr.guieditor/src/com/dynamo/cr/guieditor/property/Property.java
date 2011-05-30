package com.dynamo.cr.guieditor.property;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

@Retention(RetentionPolicy.RUNTIME)
public @interface Property {
    Class<? extends IPropertyAccessor<?, ?>> setter() default BeanPropertyAccessor.class;
    Class<? extends ICommandFactory<?, ?>> commandFactory() default DummyCommandFactory.class;
    Class<? extends IEmbeddedPropertySource<?>> embeddedSource() default DEFAULT_EMBEDDED_SOURCE.class;
    boolean isResource() default false;
    static abstract class DEFAULT_EMBEDDED_SOURCE implements IEmbeddedPropertySource<Object> {}
}
