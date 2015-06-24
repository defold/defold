package com.defold.editor;

import java.io.IOException;

import javafx.beans.property.BooleanProperty;
import javafx.beans.property.StringProperty;
import javafx.beans.property.SimpleBooleanProperty;
import javafx.beans.property.SimpleStringProperty;
import javafx.fxml.FXMLLoader;
import javafx.scene.Parent;
import javafx.scene.Scene;
import javafx.stage.Stage;
import javafx.stage.StageStyle;
import javafx.scene.control.Label;
import javafx.scene.control.Button;

public class Splash {

    private Stage stage;
    private Scene scene;
    private StringProperty launchError = new SimpleStringProperty();
    private BooleanProperty errorShowing = new SimpleBooleanProperty(false);

    public Splash() throws IOException {
        Object root = FXMLLoader.load(this.getClass().getResource("/splash.fxml"));
        scene = new Scene((Parent) root);
        stage = new Stage();
        stage.initStyle(StageStyle.UNDECORATED);
        stage.setScene(scene);

        Label launchErrorLabel = (Label) scene.lookup("#launchError");
        launchErrorLabel.textProperty().bind(launchError);
        launchErrorLabel.visibleProperty().bind(errorShowing);

        Button errorButton = (Button) scene.lookup("#errorButton");
        errorButton.visibleProperty().bind(errorShowing);
        errorButton.setOnAction((event) -> {
                System.exit(1);
            });

        stage.show();
    }

    public void close() {
        stage.close();
    }

    public void setLaunchError(String value) {
        launchError.setValue(value);
    }

    public String getLaunchError() {
        return launchError.getValue();
    }

    public StringProperty launchErrorProperty() {
        return launchError;
    }

     public void setErrorShowing(boolean value) {
        errorShowing.setValue(value);
    }

    public boolean getErrorShowing() {
        return errorShowing.getValue();
    }

    public BooleanProperty errorShowingProperty() {
        return errorShowing;
    }

}
