(ns editor.properties-view
  (:require [camel-snake-kebab :as camel]
            [dynamo.graph :as g]
            [dynamo.types :as t]
            [dynamo.file.protobuf :as protobuf]
            [editor.ui :as ui]
            [editor.workspace :as workspace]
            [editor.core :as core]
            [editor.dialogs :as dialogs])
  (:import [com.defold.editor Start]
           [com.jogamp.opengl.util.awt Screenshot]
           [javafx.animation AnimationTimer]
           [javafx.application Platform]
           [javafx.beans.value ChangeListener]
           [javafx.collections FXCollections ObservableList]
           [javafx.embed.swing SwingFXUtils]
           [javafx.event ActionEvent EventHandler]
           [javafx.fxml FXMLLoader]
           [javafx.geometry Insets]
           [javafx.scene Scene Node Parent]
           [javafx.scene.control Button ChoiceBox ColorPicker Label TextField TitledPane TextArea TreeItem TreeCell Menu MenuItem MenuBar Tab ProgressBar]
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
           [com.google.protobuf ProtocolMessageEnum]
           [com.dynamo.proto DdfExtensions]))


; From https://github.com/mikera/clojure-utils/blob/master/src/main/clojure/mikera/cljutils/loops.clj
(defmacro doseq-indexed
  "loops over a set of values, binding index-sym to the 0-based index of each value"
  ([[val-sym values index-sym] & code]
  `(loop [vals# (seq ~values)
          ~index-sym (long 0)]
     (if vals#
       (let [~val-sym (first vals#)]
             ~@code
             (recur (next vals#) (inc ~index-sym)))
       nil))))

(def create-property-control! nil)

(defmulti create-property-control! (fn [t _ _] t))

(defmethod create-property-control! String [_ workspace on-new-value]
  (let [text (TextField.)
        setter #(.setText text (str %))]
    (.setOnAction text (ui/event-handler event (on-new-value (.getText text))))
    [text setter]))

(defn- to-int [s]
  (try
    (Integer/parseInt s)
    (catch Throwable _
      nil)))

(defmethod create-property-control! t/Int [_ workspace on-new-value]
  (let [text (TextField.)
        setter #(.setText text (str %))]
    (.setOnAction text (ui/event-handler event (on-new-value (to-int (.getText text)))))
    [text setter]))

(defn- to-double [s]
  (try
    (Double/parseDouble s)
    (catch Throwable _
      nil)))

(defmethod create-property-control! t/Vec3 [_ workspace on-new-value]
  (let [x (TextField.)
        y (TextField.)
        z (TextField.)
        box (HBox.)
        setter (fn [vec]
                 (doseq-indexed [t [x y z] i]
                   (.setText ^TextField t (str (nth vec i)))))
        handler (ui/event-handler event (on-new-value (mapv #(to-double (.getText ^TextField %)) [x y z])))]

    (.setSpacing box 6)
    (doseq [t [x y z]]
      (.setOnAction ^TextField t handler)
      (HBox/setHgrow ^TextField t Priority/SOMETIMES)
      (.setPrefWidth ^TextField t 60)
      (.add (.getChildren box) t))
    [box setter]))

(defmethod create-property-control! t/Color [_ workspace on-new-value]
 (let [color-picker (ColorPicker.)
       handler (ui/event-handler event
                                 (let [^Color c (.getValue color-picker)]
                                   (on-new-value [(.getRed c) (.getGreen c) (.getBlue c) (.getOpacity c)])))
       setter #(.setValue color-picker (Color. (nth % 0) (nth % 1) (nth % 2) (nth % 3)))]
   (.setOnAction color-picker handler)
   [color-picker setter]))

(defn- proto-display-name [value]
  (-> value (.getValueDescriptor) (.getOptions) (.getExtension DdfExtensions/displayName)))

(def ^:private ^:dynamic *programmatic-setting* nil)

(defmethod create-property-control! ProtocolMessageEnum [t workspace on-new-value]
  (let [cb (ChoiceBox.)
        setter #(binding [*programmatic-setting* true]
                   (.setValue cb %))]
    (try
      (let [values (.getMethod t "values" (into-array Class []))
            enum-values (.invoke values nil (object-array 0))]
        (doto (.getItems cb) (.addAll (object-array (map #(protobuf/pb-enum->val (.getValueDescriptor %)) enum-values))))
        (.setConverter cb (proxy [StringConverter] []
                            (toString [value]
                              (proto-display-name (protobuf/val->pb-enum t value)))
                            (fromString [s]
                              (when-let [enum-val (first (filter #(= s (proto-display-name %)) enum-values))]
                                (protobuf/pb-enum->val enum-val)))))
        (-> cb (.valueProperty) (.addListener (reify ChangeListener (changed [this observable old-val new-val]
                                                                      (when-not *programmatic-setting*
                                                                        (on-new-value new-val))))))))
    [cb setter]))

(defmethod create-property-control! :default [_ workspace on-new-value]
  (let [text (TextField.)
        setter #(.setText text (str %))]
    (.setDisable text true)
    [text setter]))

(defmethod create-property-control! (t/protocol workspace/Resource) [_ workspace on-new-value]
  (let [box (HBox.)
        button (Button. "...")
        text (TextField.)
        setter #(.setText text (when % (workspace/proj-path %)))]
    (ui/on-action! button (fn [_]  (when-let [resource (first (dialogs/make-resource-dialog workspace {}))]
                                     (on-new-value resource))))
    (ui/children! box [text button])
    (ui/on-action! text (fn [_] (on-new-value (workspace/file-resource workspace (ui/text text))) ))
    [box setter]))

(defn- niceify-label
  [k]
  (-> k
    name
    camel/->Camel_Snake_Case_String
    (clojure.string/replace "_" " ")))

(defn- create-properties-row [workspace ^GridPane grid node key property row]
  (when (not (false? (:visible property)))
    (let [label (Label. (niceify-label key))
          ; TODO: Possible to solve mutual references without an atom here?
          setter-atom (atom nil)
          on-new-value (fn [new-val]
                         (let [old-val (key (g/refresh node))]
                           (when-not (= new-val old-val)
                             (if (t/property-valid-value? property new-val)
                               (do
                                 ;; TODO Consider using the :validator-fn feature of atom to apply validation
                                 (@setter-atom new-val)
                                 ;; TODO Apply a label to this transaction for undo menu
                                 (g/transact (g/set-property node key new-val)))
                               (@setter-atom old-val)))))
          ; TODO - hax until property system has more flexibility for meta edit data
          t (or (first (filter class? (t/property-tags property))) (t/property-value-type property))
          [control setter] (create-property-control! t workspace on-new-value)]
      (reset! setter-atom setter)
      (setter (get node key))
      (GridPane/setConstraints label 1 row)
      (GridPane/setConstraints control 2 row)
      (.add (.getChildren grid) label)
      (.add (.getChildren grid) control))))

(defn- create-properties [workspace grid node]
  (let [properties (g/properties node)]
    (doseq [[key p] properties]
      (let [row (/ (.size (.getChildren grid)) 2)]
        (create-properties-row workspace grid node key p row))))
  (when (satisfies? core/MultiNode node)
    (doseq [n (core/sub-nodes node)]
      (create-properties workspace grid n))))

(defn- update-grid [parent workspace node]
  (.clear (.getChildren parent))
  (when node
    (let [grid (GridPane.)]
      (.setPadding grid (Insets. 10 10 10 10))
      (.setHgap grid 4)
      (.setVgap grid 6)
      (create-properties workspace grid node)
      (.add (.getChildren parent) grid)
      grid)))

(g/defnode PropertiesView
  (property parent-view Parent)
  (property repainter AnimationTimer)
  (property workspace t/Any)

  (input selection t/Any)

  (output grid-pane GridPane :cached (g/fnk [parent-view workspace selection] (update-grid parent-view workspace (first selection)))) ; TODO - add multi-selection support for properties view

  (trigger stop-animation :deleted (fn [tx graph self label trigger]
                                     (.stop ^AnimationTimer (:repainter self))
                                     nil)))

(defn make-properties-view [workspace view-graph parent]
  (let [view      (g/make-node! view-graph PropertiesView :parent-view parent :workspace workspace)
        self-ref  (g/node-id view)
        repainter (proxy [AnimationTimer] []
                    (handle [now]
                      (let [self (g/node-by-id (g/now) self-ref)
                            grid (g/node-value self :grid-pane)])))]
    (g/transact (g/set-property view :repainter repainter))
    (.start repainter)
    (g/refresh view)))
