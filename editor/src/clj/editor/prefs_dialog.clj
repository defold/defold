;; Copyright 2020-2023 The Defold Foundation
;; Copyright 2014-2020 King
;; Copyright 2009-2014 Ragnar Svensson, Christian Murray
;; Licensed under the Defold License version 1.0 (the "License"); you may not use
;; this file except in compliance with the License.
;; 
;; You may obtain a copy of the License, together with FAQs at
;; https://www.defold.com/license
;; 
;; Unless required by applicable law or agreed to in writing, software distributed
;; under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
;; CONDITIONS OF ANY KIND, either express or implied. See the License for the
;; specific language governing permissions and limitations under the License.

(ns editor.prefs-dialog
  (:require [clojure.java.io :as io]
            [editor.engine :as engine]
            [editor.engine.native-extensions :as native-extensions]
            [editor.prefs :as prefs]
            [editor.system :as system]
            [editor.ui :as ui])
  (:import [com.defold.control LongField]
           [com.sun.javafx PlatformUtil]
           [javafx.geometry Side VPos]
           [javafx.scene Parent Scene]
           [javafx.scene.control ColorPicker CheckBox ChoiceBox Label TextArea TextField Tab TabPane ContextMenu MenuItem]
           [javafx.scene.layout ColumnConstraints GridPane Priority]
           [javafx.scene.input KeyCode KeyEvent]
           [javafx.util StringConverter]))

(set! *warn-on-reflection* true)

(defmulti create-control! (fn [prefs grid desc] (:type desc)))

(defn- create-generic [^Class class prefs grid desc]
  (let [control (.newInstance class)
        commit (fn [] (prefs/set-prefs prefs (:key desc) (ui/value control)))]
    (ui/value! control (prefs/get-prefs prefs (:key desc) (:default desc)))
    (ui/on-focus! control (fn [focus] (when-not focus (commit))))
    (when-not (:multi-line desc)
      (ui/on-action! control (fn [e] (commit))))
    control))

(defmethod create-control! :boolean [prefs grid desc]
  (create-generic CheckBox prefs grid desc))

(defmethod create-control! :color [prefs grid desc]
  (create-generic ColorPicker prefs grid desc))

(defmethod create-control! :string [prefs grid desc]
  (if (:multi-line desc)
    (create-generic TextArea prefs grid desc)
    (create-generic TextField prefs grid desc)))

(defn- filter-exist-files [files]
  (filter #(.exists (io/file %)) files))

(defn- suggest-code-editor-macos []
  (let [files (list "/Applications/Visual Studio Code.app/Contents/MacOS/Electron"
                    "/Applications/Sublime Text.app/Contents/SharedSupport/bin/subl"
                    "/Applications/TextMate.app/Contents/Resources/mate")]
    (filter-exist-files files)))

(defn- suggest-code-editor-win []
  (let [files (list "C:/Program Files/Microsoft VS Code/Code.exe"
                    "C:/Program Files (x86)/Microsoft VS Code/Code.exe"
                    "C:/Program Files/Sublime Text/sublime_text.exe"
                    "C:/Program Files (x86)/Sublime Text/sublime_text.exe"
                    "C:/Program Files/Notepad++/notepad++.exe")]
    (filter-exist-files files)))

(defn- suggest-code-editor-linux []
  (let [files (list "/usr/bin/code"
                    "/usr/share/code"
                    "/snap/code/current"
                    "/usr/bin/subl"
                    "/snap/sublime-text/current/opt/sublime_text")]
    (filter-exist-files files)))

(defn- suggest-code-editor []
  (cond
    (PlatformUtil/isMac) (suggest-code-editor-macos)
    (PlatformUtil/isWindows) (suggest-code-editor-win)
    (PlatformUtil/isLinux) (suggest-code-editor-linux)
    :else '()))

(defn- items-for-auto-type [^String type]
  (case type
    "code" (suggest-code-editor)
    '()))

(defmethod create-control! :string-auto [prefs grid desc]
  (let [auto-type (:auto-type desc)
        items (items-for-auto-type auto-type)
        control (TextField.)
        popup (ContextMenu.)
        commit (fn [] (prefs/set-prefs prefs (:key desc) (ui/value control)))]

    (ui/value! control (prefs/get-prefs prefs (:key desc) (:default desc)))
    (ui/on-focus! control (fn [focus] (when-not focus (commit))))
    (ui/on-action! control (fn [_] (commit)))

    (ui/observe (.focusedProperty control) (fn [_ _ _] (.hide popup)))

    (ui/observe (.textProperty control)
                (fn [_ _ ^String new-val]

                  (let [text (if new-val (.trim new-val) "")
                        match-items (filter #(.contains (.toLowerCase ^String %) (.toLowerCase text)) items)
                        should-show (and (not-empty match-items) (not (.isEmpty text)))
                        menu-items (map (fn [^String item]
                                          (let [menu (MenuItem. item)]
                                            (.setOnAction menu (ui/event-handler menu
                                                                 (.setText control item)
                                                                 (.positionCaret control (.length item))
                                                                 (.hide popup)
                                                                 (commit)))
                                            menu)) match-items)]
                    (if should-show
                      (do
                        (-> popup .getItems .clear)
                        (-> popup .getItems (.addAll ^java.util.Collection menu-items))
                        (.show popup control (Side/BOTTOM) 0 0))
                      (.hide popup))
                    )))
    control))

(defmethod create-control! :long [prefs grid desc]
  (create-generic LongField prefs grid desc))

(defmethod create-control! :choicebox [prefs grid desc]
  (let [control (ChoiceBox.)
        options (:options desc)
        options-map (apply hash-map (flatten options))
        inv-options-map (clojure.set/map-invert options-map)]
    (.setConverter control
      (proxy [StringConverter] []
        (toString [value]
          (get options-map value))
        (fromString [s]
          (inv-options-map s))))
    (.addAll (.getItems control) ^java.util.Collection (map first options))
    (.select (.getSelectionModel control) (prefs/get-prefs prefs (:key desc) (:default desc)))

    (ui/observe (.valueProperty control) (fn [observable old-val new-val]
                                           (prefs/set-prefs prefs (:key desc) new-val)))
    control))

(defn- create-prefs-row! [prefs ^GridPane grid row desc]
  (let [label (Label. (:label desc))
        ^Parent control (create-control! prefs grid desc)]
    (GridPane/setConstraints label 0 row)
    (GridPane/setConstraints control 1 row)
    (ui/tooltip! label (:tooltip desc))
    (when (:multi-line desc)
      (GridPane/setValignment label VPos/TOP))
    (.add (.getChildren grid) label)
    (.add (.getChildren grid) control)))

(defn- add-page! [prefs ^TabPane pane page-desc]
  (let [tab (Tab. (:name page-desc))
        grid (GridPane.)]
    (doto (.getColumnConstraints grid)
      (.add (doto (ColumnConstraints.)
              (.setMinWidth ColumnConstraints/CONSTRAIN_TO_PREF)
              (.setHgrow Priority/NEVER)))
      (.add (doto (ColumnConstraints.)
              (.setHgrow Priority/ALWAYS))))
    (.setHgap grid 4)
    (.setVgap grid 6)
    (.setContent tab grid)
    (doall (map-indexed (fn [i desc] (create-prefs-row! prefs grid i desc))
                        (:prefs page-desc)))
    (ui/add-style! grid "prefs")
    (.add (.getTabs pane) tab)))

(defn- pref-pages
  []
  (cond-> [{:name  "General"
            :prefs [{:label "Load External Changes on App Focus" :type :boolean :key "external-changes-load-on-app-focus" :default true}
                    {:label "Open Bundle Target Folder" :type :boolean :key "open-bundle-target-folder" :default true}
                    {:label "Enable Texture Compression" :type :boolean :key "general-enable-texture-compression" :default false}
                    {:label "Escape Quits Game" :type :boolean :key "general-quit-on-esc" :default false}
                    {:label "Track Active Tab in Asset Browser" :type :boolean :key "asset-browser-track-active-tab?" :default false}
                    {:label "Path to Custom Keymap" :type :string :key "custom-keymap-path" :default ""}]}
           {:name  "Code"
            :prefs [{:label "Custom Editor" :type :string-auto :key "code-custom-editor" :auto-type "code" :default ""}
                    {:label "Open File" :type :string :key "code-open-file" :default "{file}"}
                    {:label "Open File at Line" :type :string :key "code-open-file-at-line" :default "{file}:{line}"}
                    {:label "Code Editor Font (Requires Restart)" :type :string :key "code-editor-font-name" :default "Dejavu Sans Mono"}]}
           {:name  "Extensions"
            :prefs [{:label "Build Server" :type :string :key "extensions-server" :default native-extensions/defold-build-server-url}
                    {:label "Build Server Headers" :type :string :key "extensions-server-headers" :default native-extensions/defold-build-server-headers :multi-line true}]}
           {:name  "Tools"
            :prefs [{:label "ADB path" :type :string :key "adb-path" :default "" :tooltip "Path to ADB command that might be used to install and launch the Android app when it's bundled"}
                    {:label "ios-deploy path" :type :string :key "ios-deploy-path" :default "" :tooltip "Path to ios-deploy command that might be used to install and launch iOS app when it's bundled"}]}]

    (system/defold-dev?)
    (conj {:name "Dev"
           :prefs [{:label "Custom Engine" :type :string :key engine/custom-engine-pref-key :default ""}]})))

(defn open-prefs [preferences]
  (let [root ^Parent (ui/load-fxml "prefs.fxml")
        stage (ui/make-dialog-stage (ui/main-stage))
        scene (Scene. root)]

    (ui/with-controls root [^TabPane prefs]
      (doseq [p (pref-pages)]
        (add-page! preferences prefs p)))

    (ui/title! stage "Preferences")
    (.setScene stage scene)

    (.addEventFilter scene KeyEvent/KEY_PRESSED
                     (ui/event-handler event
                                       (let [code (.getCode ^KeyEvent event)]
                                         (when (= code KeyCode/ESCAPE)
                                           (.close stage)))))

    (ui/show-and-wait! stage)))
