(ns editor.prefs-dialog
  (:require [clojure.java.io :as io]
            [service.log :as log]
            [editor.ui :as ui]
            [editor.prefs :as prefs]
            [editor.system :as system]
            [editor.engine.native-extensions :as native-extensions])
  (:import [com.defold.control LongField]
           [javafx.scene Parent Scene]
           [javafx.scene.paint Color]
           [javafx.scene.control Button ColorPicker Control CheckBox ChoiceBox Label TextField Tab TabPane]
           [javafx.scene.layout AnchorPane GridPane]
           [javafx.scene.input KeyCode KeyEvent]
           [javafx.stage Stage Modality DirectoryChooser FileChooser]
           [javafx.util StringConverter]))

(set! *warn-on-reflection* true)

(defmulti create-control! (fn [prefs grid desc] (:type desc)))

(defn- create-generic [^Class class prefs grid desc]
  (let [control (.newInstance class)
        commit (fn [] (prefs/set-prefs prefs (:key desc) (ui/value control)))]
    (ui/value! control (prefs/get-prefs prefs (:key desc) (:default desc)))
    (ui/on-focus! control (fn [focus] (when-not focus (commit))))
    (ui/on-action! control (fn [e] (commit)))
    control))

(defmethod create-control! :boolean [prefs grid desc]
  (create-generic CheckBox prefs grid desc))

(defmethod create-control! :color [prefs grid desc]
  (create-generic ColorPicker prefs grid desc))

(defmethod create-control! :string [prefs grid desc]
  (create-generic TextField prefs grid desc))

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
    (.addAll (.getItems control) ^java.util.Collection (map first options) )
    (.select (.getSelectionModel control) (prefs/get-prefs prefs (:key desc) (:default desc)))

    (ui/observe (.valueProperty control) (fn [observable old-val new-val]
                                           (prefs/set-prefs prefs (:key desc) new-val)))
    control))

(defn- create-prefs-row! [prefs ^GridPane grid row desc]
  (let [label (Label. (str (:label desc) ":"))
        ^Parent control (create-control! prefs grid desc)]
    (GridPane/setConstraints label 1 row)
    (GridPane/setConstraints control 2 row)

    (.add (.getChildren grid) label)
    (.add (.getChildren grid) control)))

(defn- add-page! [prefs ^TabPane pane page-desc]
  (let [tab (Tab. (:name page-desc))
        grid (GridPane.)]
    (.setHgap grid 4)
    (.setVgap grid 6)
    (.setContent tab grid)
    (doall (map-indexed (fn [i desc] (create-prefs-row! prefs grid i desc))
                        (:prefs page-desc)))
    (ui/add-style! grid "prefs")
    (.add (.getTabs pane) tab)))

(defn- pref-pages
  []
  [{:name  "General"
    :prefs [{:label "Enable Texture Profiles" :type :boolean :key "general-enable-texture-profiles" :default true}
            {:label "Escape Quits Game" :type :boolean :key "general-quit-on-esc" :default false}
            {:label "Track Active Item in Asset Browser" :type :boolean :key "asset-browser-track-active-item?" :default false}]}
   {:name  "Scene"
    :prefs [{:label "Selection Color" :type :color :key "scene-selection-color" :default (Color/web "#00ff00ff")}
            {:label "Grid" :type :choicebox :key "scene-grid-type" :default :auto :options [[:auto "Auto"] [:manual "Manual"]]}
            {:label "Grid Size" :type :long :key "scene-grid-size" :default 100}
            {:label "Grid Color" :type :color :key "scene-grid-color" :default (Color/web "#999999ff")}]}
   {:name  "Code"
    :prefs [{:label "Custom Editor" :type :string :key "code-custom-editor" :default ""}
            {:label "Open File" :type :string :key "code-open-file" :default "{file}"}
            {:label "Open File at Line" :type :string :key "code-open-file-at-line" :default "{file}:{line}"}]}
   {:name  "Extensions"
    :prefs [{:label "Build Server" :type :string :key "extensions-server" :default native-extensions/defold-build-server-url}]}])

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

    (ui/show! stage)))
