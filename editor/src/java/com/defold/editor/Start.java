package com.defold.editor;

import javafx.application.Application;
import javafx.stage.Stage;
import clojure.lang.RT;
import clojure.lang.Var;

public class Start extends Application {
    public static final int REPL_PORT = 4001;

    @Override
    public void start(Stage primaryStage) throws Exception {
        RT.load("editor/debug");
        Var startServer = RT.var("editor.debug", "start-server");
        startServer.invoke(REPL_PORT);
        RT.load("dynamo/messages");
        RT.load("editor/boot");
    }

    public static void main(String[] args) {
        Start.launch(args);
    }
}
