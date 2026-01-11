// Copyright 2020-2026 The Defold Foundation
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

import java.io.File;
import java.io.IOException;

import javafx.scene.control.TreeCell;
import javafx.scene.control.TreeView;
import javafx.util.Callback;
import javafx.scene.Node;
import javafx.scene.control.Hyperlink;
import javafx.scene.text.Text;
import javafx.scene.text.TextFlow;

import java.net.URI;

import java.util.ArrayList;
import java.util.List;

import java.awt.Desktop;


public class UIUtil {

    public static Callback<TreeView<File>, TreeCell<File>> newFileCellFactory() {

        return new Callback<TreeView<File>, TreeCell<File>>() {
            @Override
            public TreeCell<File> call(TreeView<File> v) {
                return new TreeCell<File>() {
                    @Override
                    protected void updateItem(File item, boolean empty) {
                        super.updateItem(item, empty);
                        if (!empty) {
                            if (item != null) {
                                String name = item.getName();
                                if (name != null) {
                                    setText(name);
                                }
                            }
                        } else {
                        	setGraphic(null);
                        	setText(null);
                        }
                    }
                };
            }
        };
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

    public static void stringToTextFlowNodes(TextFlow text, String str) {
        text.getChildren().setAll(UIUtil.stringToTextFlowNodes(str));
    }
    public static List<Node> stringToTextFlowNodes(String str) {
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

}
