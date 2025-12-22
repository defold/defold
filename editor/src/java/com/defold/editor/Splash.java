// Copyright 2020-2025 The Defold Foundation
// Copyright 2014-2020 King
// Copyright 2009-2014 Ragnar Svensson, Christian Murray
// Licensed under the Defold License version 1.0 (the "License"); you may not use
// this file except in compliance with the License.
//
// You may obtain a copy of the License, together with FAQs at
// https://www.defold.com/license
//
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

package com.defold.editor;

import javafx.beans.property.BooleanProperty;
import javafx.beans.property.SimpleBooleanProperty;
import javafx.beans.property.SimpleStringProperty;
import javafx.beans.property.StringProperty;
import javafx.fxml.FXMLLoader;
import javafx.scene.Parent;
import javafx.scene.Scene;
import javafx.scene.control.Button;
import javafx.scene.control.Label;
import javafx.scene.image.Image;
import javafx.scene.image.ImageView;
import javafx.scene.paint.Color;
import javafx.scene.text.TextFlow;
import javafx.stage.Stage;
import javafx.stage.StageStyle;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.nio.charset.Charset;
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

    private static int randomInt(int max) {
        return new Random().nextInt(max);
    }

    private static List<String> readTips() throws IOException {
        InputStream tipsResource = Thread.currentThread()
                .getContextClassLoader()
                .getResourceAsStream("tips.txt");
        List<String> tips = new ArrayList<>();
        InputStreamReader reader = new InputStreamReader(tipsResource, Charset.forName("UTF-8"));
        try (BufferedReader bufferedReader = new BufferedReader(reader)) {
            String line;
            while ((line = bufferedReader.readLine()) != null) {
                if (line.startsWith("#") || line.isEmpty())
                    continue;
                tips.add(line);
            }
        }
        return tips;
    }

    private static String randomElement(List<String> tips) {
        return tips.get(randomInt(tips.size()));
    }

    private static void randomGame(Scene scene) {
        List<String[]> games = new ArrayList<>();
        
        games.add(new String[]{"Family Island", "/games/familyisland.jpg"});
        games.add(new String[]{"Solitaire Jazz Travel", "/games/solitairejazztravel.jpg"});
        games.add(new String[]{"Duo Zombies", "/games/duozombies.jpg"});
        games.add(new String[]{"Fates of Ort", "/games/fatesofort.jpg"});
        games.add(new String[]{"Plague Lords", "/games/plaguelords.jpg"});
        games.add(new String[]{"Void Scrappers", "/games/voidscrappers.jpg"});
        games.add(new String[]{"Monkey Mart", "/games/monkeymart.jpg"});
        games.add(new String[]{"Look Your Loot", "/games/lookyourloot.jpg"});
        games.add(new String[]{"Warnament", "/games/warnament.jpg"});
        games.add(new String[]{"Craftomation 101", "/games/craftomation101.jpg"});
        games.add(new String[]{"BORE BLASTERS", "/games/boreblasters.jpg"});
        games.add(new String[]{"Skull Horde", "/games/skullhorde.jpg"});
        games.add(new String[]{"SuperWEIRD", "/games/superweird.jpg"});
        games.add(new String[]{"Tiny Racing", "/games/tinyracing.jpeg"});

        String[] game = games.get(randomInt(games.size()));

        ImageView gameScreenshot = (ImageView) scene.lookup("#game-screenshot");
        gameScreenshot.setImage(new Image(game[1]));
        Label gameNameLabel = (Label) scene.lookup("#game-name");
        gameNameLabel.setText(game[0]);
    }

    public void show() throws IOException {
        Object root = FXMLLoader.load(this.getClass().getResource("/splash.fxml"));
        Scene scene = new Scene((Parent) root);
        scene.setFill(Color.TRANSPARENT);
        stage = new Stage();
        stage.initStyle(StageStyle.TRANSPARENT);
        stage.getIcons().add(new Image(Splash.class.getResourceAsStream("/logo_blue.png")));
        stage.setScene(scene);

        randomGame(scene);

        TextFlow startupTipFlow = (TextFlow) scene.lookup("#startup-tip");
        startupTipFlow.visibleProperty().bind(errorShowing.not());
        startupTipFlow.getChildren().setAll(UIUtil.stringToTextFlowNodes(randomElement(readTips())));

        Label launchErrorLabel = (Label) scene.lookup("#launch-error");
        launchErrorLabel.textProperty().bind(launchError);
        launchErrorLabel.visibleProperty().bind(errorShowing);
        launchErrorLabel.managedProperty().bind(launchErrorLabel.visibleProperty());

        Button errorButton = (Button) scene.lookup("#error-button");
        errorButton.visibleProperty().bind(errorShowing);
        errorButton.managedProperty().bind(errorButton.visibleProperty());
        errorButton.setOnAction((event) -> System.exit(1));

        // Footer data; version and copyright
        String versionString = System.getProperty("defold.version");
        String channelName = System.getProperty("defold.channel");
        String sha1 = System.getProperty("defold.editor.sha1");

        boolean hasVersion = true;
        if (versionString == null || versionString.isEmpty()) {
            versionString = "No version";
            hasVersion = false;
        }
        channelName = (channelName == null || channelName.isEmpty()) ? "No channel" : channelName;
        channelName = channelName.equals("editor-alpha") ? "" : "   •   " + channelName;
        sha1 = (sha1 == null || sha1.isEmpty()) ? "no sha1" : sha1;
        if (sha1.length() > 7) {
            sha1 = sha1.substring(0, 7);
        }

        Label channelLabel = (Label) scene.lookup("#version");
        channelLabel.setText(versionString + " (" + sha1 + ")" + channelName);
        channelLabel.setVisible(true);

        String windowTitle = "Defold";
        if (hasVersion) {
            windowTitle += " " + versionString;
        }
        stage.setTitle(windowTitle);
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
