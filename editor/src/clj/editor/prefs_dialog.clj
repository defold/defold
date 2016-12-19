(ns editor.prefs-dialog
  (:require [service.log :as log]
            [editor.ui :as ui]
            [editor.prefs :as prefs])
  (:import [com.defold.control LongField]
           [javafx.scene Parent Scene]
           [javafx.scene.paint Color]
           [javafx.scene.control ColorPicker Control CheckBox ChoiceBox Label TextField Tab TabPane]
           [javafx.scene.layout GridPane]
           [javafx.scene.input KeyCode KeyEvent]
           [javafx.stage Stage Modality DirectoryChooser]
           [javafx.util StringConverter]))

(set! *warn-on-reflection* true)

(defonce ^:dynamic *pages* (atom {}))

(defmacro defpage [name & prefs]
  (let [qname (keyword (str *ns*) name)]
    `(swap! *pages* assoc ~qname {:name ~name
                                  :prefs [~@prefs]})))

(def create-control! nil)

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

(defpage "General"
  {:label "Enable Texture Profiles" :type :boolean :key "general-enable-texture-profiles" :default true}
  {:label "Enable Extensions" :type :boolean :key "enable-extensions" :default false}
  {:label "Escape Quits Game" :type :boolean :key "general-quit-on-esc" :default false})

(defpage "Scene"
  {:label "Selection Color" :type :color :key "scene-selection-color" :default (Color/web "#00ff00ff")}
  {:label "Top Background Color" :type :color :key "scene-top-backgroundcolor" :default (Color/web "#ff0000ff")}
  {:label "Bottom Background Color" :type :color :key "scene-bottom-backgroundcolor" :default (Color/web "#0000ffff")}
  {:label "Grid" :type :choicebox :key "scene-grid-type" :default :auto :options [[:auto "Auto"] [:manual "Manual"]]}
  {:label "Grid Size" :type :long :key "scene-grid-size" :default 100}
  {:label "Grid Color" :type :color :key "scene-grid-color" :default (Color/web "#999999ff")}
  {:label "Mouse Type" :type :choicebox :key "scene-mouse-type" :default :one-button :options [[:one-button "One Button"] [:three "Three Buttons"]]})

(defn open-prefs [prefs]
  (let [root ^Parent (ui/load-fxml "prefs.fxml")
        stage (ui/make-stage)
        scene (Scene. root)]

    (doseq [p (vals @*pages*)]
      (add-page! prefs root p))

    (.initOwner stage (ui/main-stage))
    (ui/title! stage "Preferences")
    (.initModality stage Modality/WINDOW_MODAL)
    (.setScene stage scene)

    (.addEventFilter scene KeyEvent/KEY_PRESSED
                     (ui/event-handler event
                                       (let [code (.getCode ^KeyEvent event)]
                                         (when (= code KeyCode/ESCAPE)
                                           (.close stage)))))

    (ui/show! stage)))

#_(let [prefs (prefs/make-prefs "defold")]
  (ui/run-now (open-prefs prefs)))
