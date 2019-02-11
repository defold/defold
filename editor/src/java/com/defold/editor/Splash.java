package com.defold.editor;

import javafx.beans.property.BooleanProperty;
import javafx.beans.property.SimpleBooleanProperty;
import javafx.beans.property.SimpleStringProperty;
import javafx.beans.property.StringProperty;
import javafx.fxml.FXMLLoader;
import javafx.scene.Node;
import javafx.scene.Parent;
import javafx.scene.Scene;
import javafx.scene.control.Button;
import javafx.scene.control.Hyperlink;
import javafx.scene.control.Label;
import javafx.scene.image.Image;
import javafx.scene.paint.Color;
import javafx.scene.text.Text;
import javafx.scene.text.TextFlow;
import javafx.stage.Stage;
import javafx.stage.StageStyle;

import java.awt.*;
import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.net.URI;
import java.util.ArrayList;
import java.util.List;
import java.util.Random;

public class Splash {

    private Stage stage;
    private StringProperty launchError = new SimpleStringProperty();
    private BooleanProperty errorShowing = new SimpleBooleanProperty(false);
    private BooleanProperty shown = new SimpleBooleanProperty(false);

    public Splash() {
    }

    private static Hyperlink hyperlink(String text, String url) {
        Hyperlink hyperlink = new Hyperlink(text);
        hyperlink.setOnAction(event -> new Thread(() -> {
            try {
                Desktop.getDesktop().browse(URI.create(url));
            } catch (IOException e) {
                e.printStackTrace();
            }
        }).start());
        return hyperlink;
    }

    private static Text text(String str) {
        Text text = new Text(str);
        text.getStyleClass().add("text");
        return text;
    }

    private static String unescape(String str) {
        return str.replace("\\n", "\n");
    }

    private static List<Node> stringToTextFlowNodes(String str) {
        List<Node> ret = new ArrayList<>();
        int i = 0;
        while (i < str.length()) {
            if (str.charAt(i) == '[') {
                // parse link
                int textStartIndex = i + 1; // skip '['
                int textEndIndex = str.indexOf(']', textStartIndex);
                String text = unescape(str.substring(textStartIndex, textEndIndex));
                int urlStartIndex = textEndIndex + 2; // skip ']' and then '('
                int urlEndIndex = str.indexOf(')', urlStartIndex);
                String url = str.substring(urlStartIndex, urlEndIndex);

                ret.add(hyperlink(text, url));

                i = urlEndIndex + 1;
            } else {
                // parse text
                int nextUrlIndex = str.indexOf('[', i);
                int textEndIndex = nextUrlIndex == -1 ? str.length() : nextUrlIndex;

                ret.add(text(unescape(str.substring(i, textEndIndex))));

                i = textEndIndex;
            }
        }
        return ret;
    }

    private static List<String> readTips() throws IOException {
        InputStream tipsResource = Thread.currentThread()
                .getContextClassLoader()
                .getResourceAsStream("tips.txt");
        List<String> tips = new ArrayList<>();
        try (BufferedReader reader = new BufferedReader(new InputStreamReader(tipsResource))) {
            String line;
            while ((line = reader.readLine()) != null) {
                if (line.startsWith("#") || line.isEmpty())
                    continue;
                tips.add(line);
            }
        }
        return tips;
    }

    private static String randomElement(List<String> tips) {
        return tips.get(new Random().nextInt(tips.size()));
    }

    public void show() throws IOException {
        Object root = FXMLLoader.load(this.getClass().getResource("/splash.fxml"));
        Scene scene = new Scene((Parent) root);
        scene.setFill(Color.TRANSPARENT);
        stage = new Stage();
        stage.initStyle(StageStyle.TRANSPARENT);
        stage.getIcons().add(new Image(Splash.class.getResourceAsStream("/logo_blue.png")));
        stage.setScene(scene);

        TextFlow startupTipFlow = (TextFlow) scene.lookup("#startup-tip");
        try {
            startupTipFlow.getChildren().setAll(stringToTextFlowNodes(randomElement(readTips())));
            startupTipFlow.visibleProperty().bind(errorShowing.not());
        } catch (Exception e) {
            startupTipFlow.setVisible(false);
            e.printStackTrace();
        }

        Label launchErrorLabel = (Label) scene.lookup("#launch-error");
        launchErrorLabel.textProperty().bind(launchError);
        launchErrorLabel.visibleProperty().bind(errorShowing);

        Button errorButton = (Button) scene.lookup("#error-button");
        errorButton.visibleProperty().bind(errorShowing);
        errorButton.setOnAction((event) -> System.exit(1));

        stage.setOnShown(event -> shown.set(true));
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

    public BooleanProperty shownProperty() {
        return shown;
    }
}
