(ns editor.properties-view
  (:require [clojure.set :as set]
            [camel-snake-kebab :as camel]
            [dynamo.graph :as g]
            [editor.protobuf :as protobuf]
            [editor.core :as core]
            [editor.dialogs :as dialogs]
            [editor.ui :as ui]
            [editor.types :as types]
            [editor.properties :as properties]
            [editor.workspace :as workspace])
  (:import [com.defold.editor Start]
           [com.dynamo.proto DdfExtensions]
           [com.google.protobuf ProtocolMessageEnum]
           [com.jogamp.opengl.util.awt Screenshot]
           [javafx.animation AnimationTimer]
           [javafx.application Platform]
           [javafx.beans.value ChangeListener]
           [javafx.collections FXCollections ObservableList]
           [javafx.embed.swing SwingFXUtils]
           [javafx.event ActionEvent EventHandler]
           [javafx.fxml FXMLLoader]
           [javafx.geometry Insets Pos]
           [javafx.scene Scene Node Parent]
           [javafx.scene.control Button CheckBox ChoiceBox ColorPicker Label TextField TitledPane TextArea TreeItem TreeCell Menu MenuItem MenuBar Tab ProgressBar]
           [javafx.scene.image Image ImageView WritableImage PixelWriter]
           [javafx.scene.input MouseEvent]
           [javafx.scene.layout AnchorPane GridPane StackPane HBox Priority]
           [javafx.scene.paint Color]
           [javafx.stage Stage FileChooser]
           [javafx.util Callback StringConverter]
           [java.io File]
           [java.nio.file Paths]
           [java.util.prefs Preferences]
           [javax.media.opengl GL GL2 GLContext GLProfile GLDrawableFactory GLCapabilities]
           [com.google.protobuf ProtocolMessageEnum]))

(def create-property-control! nil)

(defmulti create-property-control! (fn [edit-type _ _ _] (:type edit-type)))

(defmethod create-property-control! String [_ _ _ set-values-fn]
  (let [text (TextField.)
        update-ui-fn (fn [values]
                       (ui/text! text (properties/unify-values (map str values))))]
    (ui/on-action! text (fn [_]
                          (set-values-fn (repeat (.getText text)))))
    [text update-ui-fn]))

(defn- to-int [s]
  (try
    (Integer/parseInt s)
    (catch Throwable _
      nil)))

(defmethod create-property-control! g/Int [_ _ get-values-fn set-values-fn]
  (let [text (TextField.)
        update-ui-fn (fn [values] (ui/text! text (str (properties/unify-values (map int values)))))]
    (ui/on-action! text (fn [_] (if-let [v (to-int (.getText text))]
                                  (set-values-fn (repeat v))
                                  (update-ui-fn (get-values-fn)))))
    [text update-ui-fn]))

(defmethod create-property-control! g/Bool [_ _ _ set-values-fn]
  (let [check (CheckBox.)
        update-ui-fn (fn [values] (let [v (properties/unify-values (map boolean values))]
                                    (if (nil? v)
                                      (.setIndeterminate check true)
                                      (doto check
                                        (.setIndeterminate false)
                                        (.setSelected v)))))]
    (ui/on-action! check (fn [_] (set-values-fn (repeat (.isSelected check)))))
    [check update-ui-fn]))

(defn- to-double [s]
 (try
   (Double/parseDouble s)
   (catch Throwable _
     nil)))

(defmethod create-property-control! types/Vec3 [_ _ get-values-fn set-values-fn]
  (let [x (TextField.)
        y (TextField.)
        z (TextField.)
        box (doto (HBox.)
              (.setAlignment (Pos/BASELINE_LEFT)))
        update-ui-fn (fn [values]
                       (doseq [[t v] (map-indexed (fn [i t] [t (str (properties/unify-values (map #(double (nth % i)) values)))]) [x y z])]
                         (ui/text! t v)))]
    (.setSpacing box 6)
    (doseq [[t f] (map-indexed (fn [i t]
                                 [t (fn [_] (let [v (to-double (.getText ^TextField t))
                                                  current-vals (get-values-fn)]
                                              (if v
                                                (set-values-fn (mapv #(assoc (vec %) i v) current-vals))
                                                (update-ui-fn current-vals))))])
                               [x y z])]
      (ui/on-action! ^TextField t f))
    (doseq [[t label] (map vector [x y z] ["x" "y" "z"])]
      (HBox/setHgrow ^TextField t Priority/SOMETIMES)
      (.setPrefWidth ^TextField t 60)
      (doto (.getChildren box)
        (.add (Label. label))
        (.add t)))
    [box update-ui-fn]))

#_(defmethod create-property-control! types/Color [_ _ on-new-value]
   (let [color-picker (ColorPicker.)
         refresher (fn [val _] (.setValue color-picker (Color. (nth val 0) (nth val 1) (nth val 2) (nth val 3))))
         handler (ui/event-handler event
                                   (let [^Color c (.getValue color-picker)]
                                     (on-new-value [(.getRed c) (.getGreen c) (.getBlue c) (.getOpacity c)] refresher)))]
     (.setOnAction color-picker handler)
     [color-picker refresher]))

(def ^:private ^:dynamic *programmatic-setting* nil)

(defmethod create-property-control! :choicebox [edit-type _ get-values-fn set-values-fn]
  (let [options (:options edit-type)
        inv-options (clojure.set/map-invert options)
        cb (doto (ChoiceBox.)
             (-> (.getItems) (.addAll (object-array (map first options))))
             (.setConverter (proxy [StringConverter] []
                              (toString [value]
                                (get options value (str value)))
                              (fromString [s]
                                (inv-options s)))))
        update-ui-fn (fn [values]
                       (binding [*programmatic-setting* true]
                         (.setValue cb (properties/unify-values values))))]
    (ui/observe (.valueProperty cb) (fn [observable old-val new-val]
                                      (when-not *programmatic-setting*
                                        (set-values-fn (repeat new-val)))))
    [cb update-ui-fn]))

(defmethod create-property-control! (g/protocol workspace/Resource) [_ workspace _ set-values-fn]
  (let [box (HBox.)
        button (Button. "...")
        text (TextField.)
        update-ui-fn (fn [values] (let [val (properties/unify-values values)]
                                    (ui/text! text (when val (workspace/proj-path val)))))]
    (ui/on-action! button (fn [_]  (when-let [resource (first (dialogs/make-resource-dialog workspace {}))]
                                     (set-values-fn (repeat resource)))))
    (ui/on-action! text (fn [_] (let [path (ui/text text)
                                      resource (or (workspace/find-resource workspace path)
                                                   (workspace/file-resource workspace path))]
                                  (set-values-fn (repeat resource)))))
    (ui/children! box [text button])
    [box update-ui-fn]))

(defmethod create-property-control! :default [_ _ _ _]
  (let [text (TextField.)
        update-ui-fn (fn [values] (ui/text! text (properties/unify-values (map str values))))]
    (.setDisable text true)
    [text update-ui-fn]))

(defn- niceify-label
  [k]
  (-> k
    name
    camel/->Camel_Snake_Case_String
    (clojure.string/replace "_" " ")))

(defn- create-properties-row [workspace ^GridPane grid key property row]
  (let [label (Label. (niceify-label key))
        get-values-fn (fn []
                        (let [properties (ui/user-data grid ::properties)]
                          (get-in properties [key :values])))
        set-values-fn (fn [values]
                        (let [properties (ui/user-data grid ::properties)]
                          (properties/set-value! (get properties key) values)))
        [control update-ui-fn] (create-property-control! (:edit-type property) workspace get-values-fn set-values-fn)]
    (update-ui-fn (:values property))
    (GridPane/setConstraints label 1 row)
    (GridPane/setConstraints control 2 row)
    (.add (.getChildren grid) label)
    (.add (.getChildren grid) control)
    [key update-ui-fn]))

(defn- create-properties [workspace grid properties]
  ; TODO - add multi-selection support for properties view
  (doall (map-indexed (fn [row [key property]] (create-properties-row workspace grid key property row)) properties)))

(defn- make-grid [parent workspace properties]
  (let [grid (GridPane.)]
      (.setPadding grid (Insets. 10 10 10 10))
      (.setHgap grid 4)
      (.setVgap grid 6)
      (ui/user-data! grid ::properties properties)
      (let [update-fns (create-properties workspace grid properties)]
        ; NOTE: Note update-fns is a sequence of [[property-key update-ui-fn] ...]
        (ui/user-data! parent ::update-fns (into {} update-fns)))
      (ui/children! parent [grid])
      grid))

(defn- refresh-grid [parent ^GridPane grid workspace properties]
  (ui/user-data! grid ::properties properties)
  (let [update-fns (ui/user-data parent ::update-fns)]
    (doseq [[key property] properties]
      (when-let [update-ui-fn (get update-fns key)]
        (update-ui-fn (:values property))))))

(defn- properties->template [properties]
  (mapv (fn [[k v]] [k (select-keys v [:edit-type])]) properties))

(defn- update-grid [parent self workspace properties]
  ; NOTE: We cache the ui based on the ::template user-data
  (let [all-properties (properties/coalesce properties)
        template (properties->template all-properties)
        prev-template (ui/user-data parent ::template)]
    (if (not= template prev-template)
      (let [grid (make-grid parent workspace all-properties)]
        (ui/user-data! parent ::template template)
        (g/set-property! self :prev-grid-pane grid)
        grid)
      (do
        (let [grid (:prev-grid-pane self)]
          (refresh-grid parent grid workspace all-properties)
          grid)))))

(g/defnode PropertiesView
  (property parent-view Parent)
  (property repainter AnimationTimer)
  (property workspace g/Any)
  (property prev-grid-pane GridPane)

  (input selected-node-properties g/Any)

  (output grid-pane GridPane :cached (g/fnk [parent-view _self workspace selected-node-properties] (update-grid parent-view _self workspace selected-node-properties)))

  (trigger stop-animation :deleted (fn [tx graph self label trigger]
                                     (.stop ^AnimationTimer (:repainter self))
                                     nil)))

(defn make-properties-view [workspace project view-graph parent]
  (let [view      (g/make-node! view-graph PropertiesView :parent-view parent :workspace workspace)
        self-ref  (g/node-id view)
        repainter (proxy [AnimationTimer] []
                    (handle [now]
                      (let [self (g/node-by-id self-ref)
                            grid (g/node-value self :grid-pane)])))]
    (g/transact
      (concat
        (g/set-property view :repainter repainter)
        (g/connect (g/node-id project) :selected-node-properties (g/node-id view) :selected-node-properties)))
    (.start repainter)
    (g/refresh view)))
