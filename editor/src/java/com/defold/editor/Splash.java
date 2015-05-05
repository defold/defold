package com.defold.editor;

import java.io.IOException;

import javafx.fxml.FXMLLoader;
import javafx.scene.Parent;
import javafx.scene.Scene;
import javafx.stage.Stage;
import javafx.stage.StageStyle;

public class Splash {

    private Stage stage;

    public Splash() throws IOException {
        Object root = FXMLLoader.load(this.getClass().getResource("/splash.fxml"));
        Scene scene = new Scene((Parent) root);
        stage = new Stage();
        stage.initStyle(StageStyle.UNDECORATED);
        stage.setScene(scene);
        stage.show();
    }

    public void close() {
        stage.close();
    }

}
