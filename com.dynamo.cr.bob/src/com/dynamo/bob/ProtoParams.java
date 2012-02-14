package com.dynamo.bob;

import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;

import com.google.protobuf.GeneratedMessage;

/**
 * Proto builder parameters
 * @author Christian Murray
 *
 */
@Retention(RetentionPolicy.RUNTIME)
@Target(ElementType.TYPE)
public @interface ProtoParams {
    Class<? extends GeneratedMessage> messageClass();
}
