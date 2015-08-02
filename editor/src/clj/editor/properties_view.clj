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
           [javafx.scene.control Control Button CheckBox ChoiceBox ColorPicker Label TextField TitledPane TextArea TreeItem TreeCell Menu MenuItem MenuBar Tab ProgressBar]
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

(defn- to-int [s]
  (try
    (Integer/parseInt s)
    (catch Throwable _
      nil)))

(defn- to-double [s]
 (try
   (Double/parseDouble s)
   (catch Throwable _
     nil)))

(def create-property-control! nil)

(defmulti create-property-control! (fn [edit-type _ property-fn] (:type edit-type)))

(defmethod create-property-control! String [_ _ property-fn]
  (let [text (TextField.)
        update-ui-fn (fn [values]
                       (ui/text! text (str (properties/unify-values values))))]
    (ui/on-action! text (fn [_]
                          (properties/set-values! (property-fn) (repeat (.getText text)))))
    [text update-ui-fn]))

(defmethod create-property-control! g/Int [_ _ property-fn]
  (let [text (TextField.)
        update-ui-fn (fn [values] (ui/text! text (str (properties/unify-values values))))]
    (ui/on-action! text (fn [_] (if-let [v (to-int (.getText text))]
                                  (properties/set-values! (property-fn) (repeat v))
                                  (update-ui-fn (properties/values (property-fn))))))
    [text update-ui-fn]))

(defmethod create-property-control! g/Num [_ _ property-fn]
  (let [text (TextField.)
        update-ui-fn (fn [values] (ui/text! text (str (properties/unify-values values))))]
    (ui/on-action! text (fn [_] (if-let [v (to-double (.getText text))]
                                  (properties/set-values! (property-fn) (repeat v))
                                  (update-ui-fn (properties/values (property-fn))))))
    [text update-ui-fn]))

(defmethod create-property-control! g/Bool [_ _ property-fn]
  (let [check (CheckBox.)
        update-ui-fn (fn [values] (let [v (properties/unify-values values)]
                                    (if (nil? v)
                                      (.setIndeterminate check true)
                                      (doto check
                                        (.setIndeterminate false)
                                        (.setSelected v)))))]
    (ui/on-action! check (fn [_] (properties/set-values! (property-fn) (repeat (.isSelected check)))))
    [check update-ui-fn]))

(defn- create-multi-textfield! [labels property-fn]
  (let [text-fields (mapv (fn [l] (TextField.)) labels)
        box (doto (HBox.)
              (.setAlignment (Pos/BASELINE_LEFT)))
        update-ui-fn (fn [values]
                       (doseq [[t v] (map-indexed (fn [i t] [t (str (properties/unify-values (map #(nth % i) values)))]) text-fields)]
                         (ui/text! t v)))]
    (.setSpacing box 6)
    (doseq [[t f] (map-indexed (fn [i t]
                                 [t (fn [_] (let [v (to-double (.getText ^TextField t))
                                                  current-vals (properties/values (property-fn))]
                                              (if v
                                                (properties/set-values! (property-fn) (mapv #(assoc (vec %) i v) current-vals))
                                                (update-ui-fn current-vals))))])
                               text-fields)]
      (ui/on-action! ^TextField t f))
    (doseq [[t label] (map vector text-fields labels)]
      (HBox/setHgrow ^TextField t Priority/SOMETIMES)
      (.setPrefWidth ^TextField t 60)
      (doto (.getChildren box)
        (.add (Label. label))
        (.add t)))
    [box update-ui-fn]))

(defmethod create-property-control! types/Vec3 [_ _ property-fn]
  (create-multi-textfield! ["x" "y" "z"] property-fn))

(defmethod create-property-control! types/Vec4 [_ _ property-fn]
  (create-multi-textfield! ["x" "y" "z" "w"] property-fn))

#_(defmethod create-property-control! types/Color [_ _ on-new-value]
   (let [color-picker (ColorPicker.)
         refresher (fn [val _] (.setValue color-picker (Color. (nth val 0) (nth val 1) (nth val 2) (nth val 3))))
         handler (ui/event-handler event
                                   (let [^Color c (.getValue color-picker)]
                                     (on-new-value [(.getRed c) (.getGreen c) (.getBlue c) (.getOpacity c)] refresher)))]
     (.setOnAction color-picker handler)
     [color-picker refresher]))

(def ^:private ^:dynamic *programmatic-setting* nil)

(defmethod create-property-control! :choicebox [edit-type _ property-fn]
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
                                        (properties/set-values! (property-fn) (repeat new-val)))))
    [cb update-ui-fn]))

(defmethod create-property-control! (g/protocol workspace/Resource) [_ workspace property-fn]
  (let [box (HBox.)
        button (Button. "...")
        text (TextField.)
        update-ui-fn (fn [values] (let [val (properties/unify-values values)]
                                    (ui/text! text (when val (workspace/proj-path val)))))]
    (ui/on-action! button (fn [_]  (when-let [resource (first (dialogs/make-resource-dialog workspace {}))]
                                     (properties/set-values! (property-fn) (repeat resource)))))
    (ui/on-action! text (fn [_] (let [path (ui/text text)
                                      resource (or (workspace/find-resource workspace path)
                                                   (workspace/file-resource workspace path))]
                                  (properties/set-values! (property-fn) (repeat resource)))))
    (ui/children! box [text button])
    [box update-ui-fn]))

(defmethod create-property-control! :default [_ _ _]
  (let [text (TextField.)
        update-ui-fn (fn [values] (ui/text! text (properties/unify-values (map str values))))]
    (.setDisable text true)
    [text update-ui-fn]))

(defn- create-properties-row [workspace ^GridPane grid key property row]
  (let [label (Label. (properties/label property))
        property-fn (fn []
                      (let [properties (ui/user-data grid ::properties)]
                          (get properties key)))
        [control update-ui-fn] (create-property-control! (:edit-type property) workspace property-fn)
        reset-btn (doto (Button. "x")
                    (.setVisible (properties/overridden? property))
                    (ui/on-action! (fn [_]
                                     (properties/clear-override! (property-fn))
                                     (.requestFocus control))))]
    (update-ui-fn (properties/values property))
    (GridPane/setConstraints label 1 row)
    (GridPane/setConstraints control 2 row)
    (GridPane/setConstraints reset-btn 3 row)
    (.add (.getChildren grid) label)
    (.add (.getChildren grid) control)
    (.add (.getChildren grid) reset-btn)
    [key (fn [property]
           (.setVisible reset-btn (properties/overridden? property))
           (update-ui-fn (properties/values property)))]))

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
        (update-ui-fn property)))))

(defn- properties->template [properties]
  (mapv (fn [[k v]] [k (select-keys v [:edit-type])]) properties))

(defn- update-grid [parent id workspace properties]
  ; NOTE: We cache the ui based on the ::template user-data
  (let [all-properties (properties/coalesce properties)
        template (properties->template all-properties)
        prev-template (ui/user-data parent ::template)]
    (if (not= template prev-template)
      (let [grid (make-grid parent workspace all-properties)]
        (ui/user-data! parent ::template template)
        (g/set-property! id :prev-grid-pane grid)
        grid)
      (do
        (let [grid (g/node-value id :prev-grid-pane)]
          (refresh-grid parent grid workspace all-properties)
          grid)))))

(g/defnode PropertiesView
  (property parent-view Parent)
  (property repainter AnimationTimer)
  (property workspace g/Any)
  (property prev-grid-pane GridPane)

  (input selected-node-properties g/Any)

  (output grid-pane GridPane :cached (g/fnk [parent-view _id workspace selected-node-properties] (update-grid parent-view _id workspace selected-node-properties)))

  (trigger stop-animation :deleted (fn [tx graph self label trigger]
                                     (.stop ^AnimationTimer (g/node-value self :repainter))
                                     nil)))

(defn make-properties-view [workspace project view-graph parent]
  (let [view-id   (g/make-node! view-graph PropertiesView :parent-view parent :workspace workspace)
        repainter (proxy [AnimationTimer] []
                    (handle [now]
                      (g/node-value view-id :grid-pane)))]
    (g/transact
      (concat
        (g/set-property view-id :repainter repainter)
        (g/connect project :selected-node-properties view-id :selected-node-properties)))
    (.start repainter)
    view-id))
