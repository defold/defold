package com.defold.editor;

import com.defold.libs.NativeArtifacts;
import javafx.application.Application;
import javafx.stage.Stage;
import clojure.lang.RT;
import clojure.lang.Var;

public class DevStart extends Application {

    @Override
    public void start(Stage primaryStage) throws Exception {
        NativeArtifacts.extractNatives();
        Var boot = RT.var("editor.boot", "main");
        boot.invoke(new String[0]);
    }

    @Override
    public void stop() throws Exception {
        System.out.println("App stopped");
    }

    public static void main(String[] args) throws Exception {
        DevStart.launch(args);
    }
}
