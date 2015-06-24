(ns editor.properties-view
  (:require [camel-snake-kebab :as camel]
            [dynamo.graph :as g]
            [editor.protobuf :as protobuf]
            [editor.core :as core]
            [editor.dialogs :as dialogs]
            [editor.ui :as ui]
            [editor.types :as types]
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
           [com.google.protobuf ProtocolMessageEnum]))


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
        setter #(ui/text! text (str %))]
    (.setOnAction text (ui/event-handler event (on-new-value (.getText text))))
    [text setter]))

(defn- to-int [s]
  (try
    (Integer/parseInt s)
    (catch Throwable _
      nil)))

(defmethod create-property-control! g/Int [_ workspace on-new-value]
  (let [text (TextField.)
        setter #(ui/text! text (str %))]
    (.setOnAction text (ui/event-handler event (on-new-value (to-int (.getText text)))))
    [text setter]))

(defn- to-double [s]
  (try
    (Double/parseDouble s)
    (catch Throwable _
      nil)))

(defmethod create-property-control! types/Vec3 [_ workspace on-new-value]
  (let [x (TextField.)
        y (TextField.)
        z (TextField.)
        box (HBox.)
        setter (fn [vec]
                 (doseq-indexed [t [x y z] i]
                   (ui/text! t (str (nth vec i)))))
        handler (ui/event-handler event (on-new-value (mapv #(to-double (.getText ^TextField %)) [x y z])))]

    (.setSpacing box 6)
    (doseq [t [x y z]]
      (.setOnAction ^TextField t handler)
      (HBox/setHgrow ^TextField t Priority/SOMETIMES)
      (.setPrefWidth ^TextField t 60)
      (.add (.getChildren box) t))
    [box setter]))

(defmethod create-property-control! types/Color [_ workspace on-new-value]
 (let [color-picker (ColorPicker.)
       handler (ui/event-handler event
                                 (let [^Color c (.getValue color-picker)]
                                   (on-new-value [(.getRed c) (.getGreen c) (.getBlue c) (.getOpacity c)])))
       setter #(.setValue color-picker (Color. (nth % 0) (nth % 1) (nth % 2) (nth % 3)))]
   (.setOnAction color-picker handler)
   [color-picker setter]))

(def ^:private ^:dynamic *programmatic-setting* nil)

(defmethod create-property-control! ProtocolMessageEnum [t workspace on-new-value]
  (let [cb (ChoiceBox.)
        setter #(binding [*programmatic-setting* true]
                   (.setValue cb %))
        values (protobuf/enum-values t)]
    (doto (.getItems cb) (.addAll (object-array (map first values))))
        (.setConverter cb (proxy [StringConverter] []
                            (toString [value]
                              (or (:display-name (get values value)) value))
                            (fromString [s]
                              (first (filter #(= s (:display-name (second %))) values)))))
        (-> cb (.valueProperty) (.addListener (reify ChangeListener (changed [this observable old-val new-val]
                                                                      (when-not *programmatic-setting*
                                                                        (on-new-value new-val))))))
    [cb setter]))

(defmethod create-property-control! :default [_ workspace on-new-value]
  (let [text (TextField.)
        setter #(ui/text! text (str %))]
    (.setDisable text true)
    [text setter]))

(defmethod create-property-control! (g/protocol workspace/Resource) [_ workspace on-new-value]
  (let [box (HBox.)
        button (Button. "...")
        text (TextField.)
        setter #(ui/text! text (when % (workspace/proj-path %)))]
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
                             (if (g/property-valid-value? property new-val)
                               (do
                                 ;; TODO Consider using the :validator-fn feature of atom to apply validation
                                 (@setter-atom new-val)
                                 ;; TODO Apply a label to this transaction for undo menu
                                 (g/transact (g/set-property node key new-val)))
                               (@setter-atom old-val)))))
          ; TODO - hax until property system has more flexibility for meta edit data
          t (or (first (filter class? (g/property-tags property))) (g/property-value-type property))
          [control setter] (create-property-control! t workspace on-new-value)]
      (reset! setter-atom setter)
      (setter (get node key))
      (GridPane/setConstraints label 1 row)
      (GridPane/setConstraints control 2 row)
      (.add (.getChildren grid) label)
      (.add (.getChildren grid) control)
      [[key (g/node-id node)] setter])))

(defn- flatten-nodes [nodes]
  ; TODO: This function is a bit special as we don't support multi-selection yet
  ; We pick the first node and include potential sub-nodes. For multi-selection
  ; we'll probably have to do something quite different from this
  ; See also create-properties about the multi-selection comment
  (if-let [node (first nodes)]
    (concat [node] (when (satisfies? core/MultiNode node)
                     (flatten-nodes (core/sub-nodes node))))
    []))

(defn- create-properties-node [workspace grid node]
  (let [properties (-> node g/node-type g/properties)
        properties-value (g/node-value node :properties)]
    (mapcat (fn [[key p]]
              (let [visible (get-in properties-value [key :visible])
                    ;;enabled (get-in properties-value [key :enabled])
                    row (/ (.size (.getChildren grid)) 2)]
                (when visible
                  (create-properties-row workspace grid node key p row))))
         properties)))

(defn- create-properties [workspace grid nodes]
  ; TODO - add multi-selection support for properties view
  (doall (mapcat (fn [node] (create-properties-node workspace grid node)) nodes)))

(defn- make-grid [parent workspace nodes node-ids]
  (let [grid (GridPane.)]
      (.setPadding grid (Insets. 10 10 10 10))
      (.setHgap grid 4)
      (.setVgap grid 6)
      (let [setters (create-properties workspace grid nodes)]
        ; NOTE: Note setters is a sequence of [[property-key node-id] setter..]
        (ui/user-data! parent ::setters (apply hash-map setters)))
      (ui/children! parent [grid])
      grid))

(defn- refresh-grid [parent workspace nodes]
  (let [setters (ui/user-data parent ::setters)]
    (doseq [node nodes]
      (doseq [[key p] (-> node g/node-type g/properties)]
        (when-let [setter (get setters [key (g/node-id node)])]
          (setter (get node key)))))))

(defn- update-grid [parent self workspace nodes]
  ; NOTE: We cache the ui based on the ::nodes-ids user-data
  (let [all-nodes (flatten-nodes nodes)
        node-ids (map g/node-id all-nodes)
        prev-ids (ui/user-data parent ::node-ids)]
    (if (not= node-ids prev-ids)
      (let [grid (make-grid parent workspace all-nodes node-ids)]
        (ui/user-data! parent ::node-ids node-ids)
        (g/set-property! self :prev-grid-pane grid)
        grid)
      (do
        (refresh-grid parent workspace all-nodes)
        (:prev-grid-pane self)))))

(g/defnode PropertiesView
  (property parent-view Parent)
  (property repainter AnimationTimer)
  (property workspace g/Any)
  (property prev-grid-pane (g/maybe GridPane))

  (input selection g/Any)

  (output grid-pane GridPane :cached (g/fnk [parent-view self workspace selection] (update-grid parent-view self workspace selection)))

  (trigger stop-animation :deleted (fn [tx graph self label trigger]
                                     (.stop ^AnimationTimer (:repainter self))
                                     nil)))

(defn make-properties-view [workspace view-graph parent]
  (let [view      (g/make-node! view-graph PropertiesView :parent-view parent :workspace workspace)
        self-ref  (g/node-id view)
        repainter (proxy [AnimationTimer] []
                    (handle [now]
                      (let [self (g/node-by-id self-ref)
                            grid (g/node-value self :grid-pane)])))]
    (g/transact (g/set-property view :repainter repainter))
    (.start repainter)
    (g/refresh view)))
