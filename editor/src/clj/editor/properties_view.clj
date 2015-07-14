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

(defmulti create-property-control! (fn [edit-type _ _] (:type edit-type)))

(defmethod create-property-control! String [_ workspace on-new-value]
  (let [text (TextField.)
        refresher (fn [val _] (ui/text! text (str val)))]
    (ui/on-action! text (fn [_] (on-new-value (.getText text))))
    [text refresher]))

(defn- to-int [s]
  (try
    (Integer/parseInt s)
    (catch Throwable _
      nil)))

(defmethod create-property-control! g/Int [_ workspace on-new-value]
  (let [text (TextField.)
        refresher (fn [val _] (ui/text! text (str val)))]
    (ui/on-action! text (fn [_] (on-new-value (to-int (.getText text)))))
    [text refresher]))

(defmethod create-property-control! g/Bool [_ workspace on-new-value]
  (let [check (CheckBox.)
        refresher (fn [val _] (.setSelected check (boolean val)))]
    (ui/on-action! check (fn [_] (on-new-value (.isSelected check))))
    [check refresher]))

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
        refresher (fn [vec _]
                 (doseq-indexed [t [x y z] i]
                   (ui/text! t (str (nth vec i)))))]

    (.setSpacing box 6)
    (doseq [t [x y z]]
      (ui/on-action! ^TextField t (fn [_] (on-new-value (mapv #(to-double (.getText ^TextField %)) [x y z]))))
      (HBox/setHgrow ^TextField t Priority/SOMETIMES)
      (.setPrefWidth ^TextField t 60)
      (.add (.getChildren box) t))
    [box refresher]))

(defmethod create-property-control! types/Color [_ workspace on-new-value]
 (let [color-picker (ColorPicker.)
       handler (ui/event-handler event
                                 (let [^Color c (.getValue color-picker)]
                                   (on-new-value [(.getRed c) (.getGreen c) (.getBlue c) (.getOpacity c)])))
       refresher (fn [val _] (.setValue color-picker (Color. (nth val 0) (nth val 1) (nth val 2) (nth val 3))))]
   (.setOnAction color-picker handler)
   [color-picker refresher]))

(def ^:private ^:dynamic *programmatic-setting* nil)

(defmethod create-property-control! :choicebox [edit-type workspace on-new-value]
  (let [cb (ChoiceBox.)
        last-refresher-options (atom nil)
        refresher (fn [val edit-type]
                    (binding [*programmatic-setting* true]
                      (let [options (:options edit-type)]
                        (when (or (nil? @last-refresher-options) (not= options @last-refresher-options))
                          (let [inv-options (clojure.set/map-invert options)]
                            (reset! last-refresher-options options)
                            (.clear (.getItems cb))
                            (.addAll (.getItems cb) (object-array (map first options)))
                            (.setConverter cb (proxy [StringConverter] []
                                                (toString [value]
                                                  (get options value (str value)))
                                                (fromString [s]
                                                  (inv-options s))))))
                        (.setValue cb val))))]
    (ui/observe (.valueProperty cb) (fn [observable old-val new-val]
                                      (when-not *programmatic-setting*
                                        (on-new-value new-val))))
    [cb refresher]))

(defmethod create-property-control! :default [_ workspace on-new-value]
  (let [text (TextField.)
        refresher (fn [val _] (ui/text! text (str val)))]
    (.setDisable text true)
    [text refresher]))

(defmethod create-property-control! (g/protocol workspace/Resource) [_ workspace on-new-value]
  (let [box (HBox.)
        button (Button. "...")
        text (TextField.)
        refresher (fn [val _] (ui/text! text (when val (workspace/proj-path val))))]
    (ui/on-action! button (fn [_]  (when-let [resource (first (dialogs/make-resource-dialog workspace {}))]
                                     (on-new-value resource))))
    (ui/children! box [text button])
    (ui/on-action! text (fn [_] (on-new-value (workspace/file-resource workspace (ui/text text))) ))
    [box refresher]))

(defn- niceify-label
  [k]
  (-> k
    name
    camel/->Camel_Snake_Case_String
    (clojure.string/replace "_" " ")))

(defn- property-edit-type [node key]
  (or (get-in (g/node-value node :properties) [key :edit-type])
      {:type (g/property-value-type (key (g/properties (g/node-type node))))}))

(defn- property-visible [node key]
  (or (get-in (g/node-value node :properties) [key :visible] true)))

(defn- create-properties-row [workspace ^GridPane grid node key property row]
  (let [label (Label. (niceify-label key))
        ; TODO: Possible to solve mutual references without an atom here?
        refresher-atom (atom nil)
        on-new-value (fn [new-val]
                       (let [old-val (key (g/refresh node))]
                         (when-not (= new-val old-val)
                           (if (g/property-valid-value? property new-val)
                             (do
                               ;; TODO Consider using the :validator-fn feature of atom to apply validation
                               (@refresher-atom new-val (property-edit-type node key))
                               ;; TODO Apply a label to this transaction for undo menu
                               (g/transact (g/set-property node key new-val)))
                             (@refresher-atom old-val)))))
        [control refresher] (create-property-control! (property-edit-type node key) workspace on-new-value)]
    (reset! refresher-atom refresher)
    (refresher (get node key) (property-edit-type node key))
    (GridPane/setConstraints label 1 row)
    (GridPane/setConstraints control 2 row)
    (.add (.getChildren grid) label)
    (.add (.getChildren grid) control)
    [[key (g/node-id node)] refresher]))

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
  (let [properties (-> node g/node-type g/properties)]
    (mapcat (fn [[key p]]
              (let [row (/ (.size (.getChildren grid)) 2)]
                (when (property-visible node key)
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
      (let [refreshers (create-properties workspace grid nodes)]
        ; NOTE: Note refreshers is a sequence of [[property-key node-id] refresher..]
        (ui/user-data! parent ::refreshers (apply hash-map refreshers)))
      (ui/children! parent [grid])
      grid))

(defn- refresh-grid [parent workspace nodes]
  (let [refreshers (ui/user-data parent ::refreshers)]
    (doseq [node nodes]
      (doseq [[key p] (-> node g/node-type g/properties)]
        (when-let [refresher (get refreshers [key (g/node-id node)])]
          (refresher (get node key) (property-edit-type node key)))))))

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
  (property prev-grid-pane GridPane)

  (input selection g/Any)

  (output grid-pane GridPane :cached (g/fnk [parent-view _self workspace selection] (update-grid parent-view _self workspace selection)))

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
