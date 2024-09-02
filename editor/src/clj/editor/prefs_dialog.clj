;; Copyright 2020-2024 The Defold Foundation
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
  (:require [editor.engine :as engine]
            [editor.engine.native-extensions :as native-extensions]
            [editor.prefs :as prefs]
            [editor.system :as system]
            [editor.ui :as ui])
  (:import [com.defold.control DefoldStringConverter LongField]
           [javafx.geometry VPos]
           [javafx.scene Parent Scene]
           [javafx.scene.control CheckBox ChoiceBox ColorPicker Label Tab TabPane TextArea TextField]
           [javafx.scene.input KeyCode KeyEvent]
           [javafx.scene.layout ColumnConstraints GridPane Priority]))

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

(defmethod create-control! :long [prefs grid desc]
  (create-generic LongField prefs grid desc))

(defmethod create-control! :choicebox [prefs grid desc]
  (let [control (ChoiceBox.)
        options (:options desc)
        options-map (apply hash-map (flatten options))
        inv-options-map (clojure.set/map-invert options-map)]
    (.setConverter control (DefoldStringConverter. options-map inv-options-map))
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
                    {:label "Lint Code on Build" :type :boolean :key "general-lint-on-build" :default true}
                    {:label "Path to Custom Keymap" :type :string :key "custom-keymap-path" :default ""}]}
           {:name  "Code"
            :prefs [{:label "Custom Editor" :type :string :key "code-custom-editor" :default ""}
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
