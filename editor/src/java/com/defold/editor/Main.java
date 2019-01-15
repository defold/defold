package com.defold.editor;

/**
 * Using separate Main class is needed to avoid error "JavaFX runtime components
 * are missing, and are required to run this application."
 *
 * If class with main function extends {@link javafx.application.Application}
 * class, Java 11 checks if the module javafx.graphics is available and refuses
 * to start otherwise. Using other class as container for main is a workaround
 *
 * @see <a href="https://github.com/javafxports/openjdk-jfx/issues/236">Issue
 * disscussion on github</a>
 */
public class Main {
    public static void main(String[] args) {
        Start.main(args);
    }
}
