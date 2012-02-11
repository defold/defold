package com.dynamo.bob;

import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;

/**
 * Builder parameters
 * @author Christian Murray
 *
 */
@Retention(RetentionPolicy.RUNTIME)
@Target(ElementType.TYPE)
public @interface BuilderParams {
    /**
     * Get builder name
     * @return builder name
     */
    String name();

    /**
     * Get output extension
     * @return output extension
     */
    String outExt();

    /**
     * Get input extension
     * @return input extension
     */
    String inExt();
}
