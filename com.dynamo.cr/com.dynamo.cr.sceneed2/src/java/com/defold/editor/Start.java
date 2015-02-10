package com.defold.editor;


import javafx.application.Application;
import javafx.stage.Stage;
import clojure.lang.RT;

public class Start extends Application {

    @Override
    public void start(Stage primaryStage) throws Exception {
        RT.load("editor/debug");
        RT.load("editor/boot");
    }

    public static void main(String[] args) {
		Start.launch(args);
	}

}
