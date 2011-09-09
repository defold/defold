package com.dynamo.cr.properties;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

@Retention(RetentionPolicy.RUNTIME)
public @interface Entity {
    Class<? extends ICommandFactory<?, ?>> commandFactory() default DummyCommandFactory.class;
}
