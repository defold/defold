(ns editor.properties-view
  (:require [clojure.set :as set]
            [camel-snake-kebab :as camel]
            [dynamo.graph :as g]
            [editor.protobuf :as protobuf]
            [editor.core :as core]
            [editor.dialogs :as dialogs]
            [editor.ui :as ui]
            [editor.jfx :as jfx]
            [editor.types :as types]
            [editor.properties :as properties]
            [editor.workspace :as workspace]
            [editor.resource :as resource])
  (:import [com.defold.editor Start]
           [com.dynamo.proto DdfExtensions]
           [com.google.protobuf ProtocolMessageEnum]
           [com.jogamp.opengl.util.awt Screenshot]
           [javafx.application Platform]
           [javafx.beans.value ChangeListener]
           [javafx.collections FXCollections ObservableList]
           [javafx.embed.swing SwingFXUtils]
           [javafx.event ActionEvent EventHandler]
           [javafx.fxml FXMLLoader]
           [javafx.geometry Insets Pos Point2D]
           [javafx.scene Scene Node Parent]
           [javafx.scene.control Control Button CheckBox ChoiceBox ColorPicker Label Slider TextField Tooltip TitledPane TextArea TreeItem Menu MenuItem MenuBar Tab ProgressBar]
           [javafx.scene.image Image ImageView WritableImage PixelWriter]
           [javafx.scene.input MouseEvent]
           [javafx.scene.layout Pane AnchorPane GridPane StackPane HBox VBox Priority]
           [javafx.scene.paint Color]
           [javafx.stage Stage FileChooser]
           [javafx.util Callback StringConverter]
           [java.io File]
           [java.nio.file Paths]
           [java.util.prefs Preferences]
           [javax.media.opengl GL GL2 GLContext GLProfile GLDrawableFactory GLCapabilities]
           [com.google.protobuf ProtocolMessageEnum]
           [editor.properties CurveSpread Curve]))

(def ^:private grid-hgap 4)
(def ^:private grid-vgap 6)

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

(declare update-field-message)

(defmulti create-property-control! (fn [edit-type _ property-fn] (:type edit-type)))

(defmethod create-property-control! String [_ _ property-fn]
  (let [text (TextField.)
        update-ui-fn (fn [values message]
                       (ui/text! text (str (properties/unify-values values)))
                       (update-field-message [text] message))]
    (ui/on-action! text (fn [_]
                          (properties/set-values! (property-fn) (repeat (.getText text)))))
    [text update-ui-fn]))

(defmethod create-property-control! g/Int [_ _ property-fn]
  (let [text (TextField.)
        update-ui-fn (fn [values message]
                       (ui/text! text (str (properties/unify-values values)))
                       (update-field-message [text] message))]
    (ui/on-action! text (fn [_] (if-let [v (to-int (.getText text))]
                                  (properties/set-values! (property-fn) (repeat v))
                                  (update-ui-fn (properties/values (property-fn)) (properties/validation-message (property-fn))))))
    [text update-ui-fn]))

(defmethod create-property-control! g/Num [_ _ property-fn]
  (let [text (TextField.)
        update-ui-fn (fn [values message]
                       (ui/text! text (str (properties/unify-values values)))
                       (update-field-message [text] message))]
    (ui/on-action! text (fn [_] (if-let [v (to-double (.getText text))]
                                  (properties/set-values! (property-fn) (repeat v))
                                  (update-ui-fn (properties/values (property-fn)) (properties/validation-message (property-fn))))))
    [text update-ui-fn]))

(defmethod create-property-control! g/Bool [_ _ property-fn]
  (let [check (CheckBox.)
        update-ui-fn (fn [values message]
                       (let [v (properties/unify-values values)]
                         (if (nil? v)
                           (.setIndeterminate check true)
                           (doto check
                             (.setIndeterminate false)
                             (.setSelected v))))
                       (update-field-message [check] message))]
    (ui/on-action! check (fn [_] (properties/set-values! (property-fn) (repeat (.isSelected check)))))
    [check update-ui-fn]))

(defn- create-multi-textfield! [labels property-fn]
  (let [text-fields (mapv (fn [l] (TextField.)) labels)
        box (doto (HBox.)
              (.setAlignment (Pos/BASELINE_LEFT)))
        update-ui-fn (fn [values message]
                       (doseq [[t v] (map-indexed (fn [i t] [t (str (properties/unify-values (map #(nth % i) values)))]) text-fields)]
                         (ui/text! t v))
                       (update-field-message text-fields message))]
    (.setSpacing box 6)
    (doseq [[t f] (map-indexed (fn [i t]
                                 [t (fn [_] (let [v (to-double (.getText ^TextField t))
                                                  current-vals (properties/values (property-fn))]
                                              (if v
                                                (properties/set-values! (property-fn) (mapv #(assoc (vec %) i v) current-vals))
                                                (update-ui-fn current-vals (properties/validation-message (property-fn))))))])
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
  (create-multi-textfield! ["X" "Y" "Z"] property-fn))

(defmethod create-property-control! types/Vec4 [_ _ property-fn]
  (create-multi-textfield! ["X" "Y" "Z" "W"] property-fn))

(defn- create-multi-keyed-textfield! [fields property-fn]
  (let [text-fields (mapv (fn [_] (TextField.)) fields)
        box (doto (HBox.)
              (.setAlignment (Pos/BASELINE_LEFT)))
        update-ui-fn (fn [values message]
                       (doseq [[t v] (map (fn [f t] [t (str (properties/unify-values (map #(get-in % (:path f)) values)))]) fields text-fields)]
                         (ui/text! t v))
                       (update-field-message text-fields message))]
    (.setSpacing box 6)
    (doseq [[t f] (map (fn [f t]
                         [t (fn [_] (let [v (to-double (.getText ^TextField t))
                                          current-vals (properties/values (property-fn))]
                                      (if v
                                        (properties/set-values! (property-fn) (mapv #(assoc-in % (:path f) v) current-vals))
                                        (update-ui-fn current-vals (properties/validation-message (property-fn))))))])
                       fields text-fields)]
      (ui/on-action! ^TextField t f))
    (doseq [[t f] (map vector text-fields fields)
            :let [children (if (:label f)
                             [(Label. (:label f)) t]
                             [t])]]
      (HBox/setHgrow ^TextField t Priority/SOMETIMES)
      (.setPrefWidth ^TextField t 60)
      (-> (.getChildren box)
        (.addAll ^java.util.Collection children)))
    [box update-ui-fn]))

(defmethod create-property-control! CurveSpread [_ _ property-fn]
  (let [fields [{:label "Value" :path [:points 0 :y]} {:label "Spread" :path [:spread]}]]
    (create-multi-keyed-textfield! fields property-fn)))

(defmethod create-property-control! Curve [_ _ property-fn]
  (let [fields [{:path [:points 0 :y]}]]
    (create-multi-keyed-textfield! fields property-fn)))

(defmethod create-property-control! types/Color [_ _ property-fn]
 (let [color-picker (ColorPicker.)
       update-ui-fn (fn [values message]
                      (let [v (properties/unify-values values)]
                        (if (nil? v)
                          (.setValue color-picker nil)
                          (let [[r g b a] v]
                            (.setValue color-picker (Color. r g b a)))))
                      (update-field-message [color-picker] message))]
   (ui/on-action! color-picker (fn [_] (let [^Color c (.getValue color-picker)
                                             v [(.getRed c) (.getGreen c) (.getBlue c) (.getOpacity c)]]
                                         (properties/set-values! (property-fn) (repeat v)))))
   [color-picker update-ui-fn]))

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
        update-ui-fn (fn [values message]
                       (binding [*programmatic-setting* true]
                         (let [value (properties/unify-values values)]
                           (if (contains? options value)
                             (.setValue cb value)
                             (do
                               (.setValue cb nil)
                               (.. cb (getSelectionModel) (clearSelection)))))
                         (update-field-message [cb] message)))]
    (ui/observe (.valueProperty cb) (fn [observable old-val new-val]
                                      (when-not *programmatic-setting*
                                        (properties/set-values! (property-fn) (repeat new-val)))))
    [cb update-ui-fn]))

(defmethod create-property-control! (g/protocol resource/Resource) [_ workspace property-fn]
  (let [box (HBox.)
        button (Button. "...")
        text (TextField.)
        update-ui-fn (fn [values message]
                       (let [val (properties/unify-values values)]
                         (ui/text! text (when val (resource/proj-path val))))
                       (update-field-message [text] message))]
    (ui/on-action! button (fn [_]  (when-let [resource (first (dialogs/make-resource-dialog workspace {}))]
                                     (properties/set-values! (property-fn) (repeat resource)))))
    (ui/on-action! text (fn [_] (let [path (ui/text text)
                                      resource (workspace/resolve-workspace-resource workspace path)]
                                  (properties/set-values! (property-fn) (repeat resource)))))
    (ui/children! box [text button])
    [box update-ui-fn]))

(defmethod create-property-control! :slider [edit-type workspace property-fn]
  (let [box (HBox. 4.0)
        [^TextField textfield tf-update-ui-fn] (create-property-control! {:type g/Num} workspace property-fn)
        min (:min edit-type 0.0)
        max (:max edit-type 1.0)
        val (:value edit-type max)
        precision (:precision edit-type)
        slider (Slider. min max val)
        update-ui-fn (fn [values message]
                       (tf-update-ui-fn values message)
                       (binding [*programmatic-setting* true]
                         (if-let [v (properties/unify-values values)]
                           (doto slider
                             (.setDisable false)
                             (.setValue v))
                           (.setDisable slider true))
                         (update-field-message [slider] message)))]
    (HBox/setHgrow slider Priority/ALWAYS)
    (.setPrefColumnCount textfield (if precision (count (str precision)) 5))
    (ui/observe (.valueChangingProperty slider) (fn [observable old-val new-val]
                                                  (ui/user-data! slider ::op-seq (gensym))))
    (ui/observe (.valueProperty slider) (fn [observable old-val new-val]
                                          (when-not *programmatic-setting*
                                            (let [val (if precision
                                                        (* precision (Math/round (double (/ new-val precision))))
                                                        new-val)]
                                              (properties/set-values! (property-fn) (repeat val) (ui/user-data slider ::op-seq))))))
    (ui/children! box [textfield slider])
    [box update-ui-fn]))

(defmethod create-property-control! :default [_ _ _]
  (let [text (TextField.)
        wrapper (HBox.)
        update-ui-fn (fn [values message]
                       (ui/text! text (properties/unify-values (map str values)))
                       (update-field-message [wrapper] message))]
    (HBox/setHgrow text Priority/ALWAYS)
    (ui/children! wrapper [text])
    (.setDisable text true)
    [wrapper update-ui-fn]))

(defn- ^Point2D node-screen-coords [^Node node ^Point2D offset]
  (let [scene (.getScene node)
        window (.getWindow scene)
        window-coords (Point2D. (.getX window) (.getY window))
        scene-coords (Point2D. (.getX scene) (.getY scene))
        node-coords (.localToScene node (.getX offset) (.getY offset))]
    (.add node-coords (.add scene-coords window-coords))))

(defn- show-message-tooltip [^Node control]
  (when-let [tip (ui/user-data control ::field-message)]
    ;; FIXME: Hack to position tooltip somewhat below control so .show doesn't immediately trigger an :exit.
    (let [tooltip (Tooltip. tip)
          control-bounds (.getLayoutBounds control)
          tooltip-coords (node-screen-coords control (Point2D. 0.0 (+ (.getHeight control-bounds) 11.0)))]
      (ui/user-data! control ::tooltip tooltip)
      (.show tooltip control (.getX tooltip-coords) (.getY tooltip-coords)))))

(defn- hide-message-tooltip [control]
  (when-let [tooltip ^Tooltip (ui/user-data control ::tooltip)]
    (ui/user-data! control ::tooltip nil)
    (.hide tooltip)))

(defn- update-message-tooltip [control]
  (when (ui/user-data control ::tooltip)
    (hide-message-tooltip control)
    (show-message-tooltip control)))

(defn- install-tooltip-message [ctrl msg]
  (ui/user-data! ctrl ::field-message (:message msg))
  (ui/on-mouse! ctrl
    (when msg
      (fn [verb e]
        (condp = verb
          :enter (show-message-tooltip ctrl)
          :exit (hide-message-tooltip ctrl)
          :move nil)))))

(def ^:private severity-style-map
  {g/FATAL "field-error"
   g/SEVERE "field-error"
   g/WARNING "field-warning"
   g/INFO "field-warning"})

(defn- update-message-style [ctrl msg]
  (if msg
    (ui/add-style! ctrl (severity-style-map (:severity msg)))
    (ui/remove-styles! ctrl (vals severity-style-map))))

(defn- update-field-message [ctrls msg]
  (doseq [ctrl ctrls]
    (install-tooltip-message ctrl msg)
    (update-message-style ctrl msg)
    (if msg
      (update-message-tooltip ctrl)
      (hide-message-tooltip ctrl))))

(defn- create-properties-row [workspace ^GridPane grid key property row property-fn]
  (let [label (Label. (properties/label property))
        [^Node control update-ctrl-fn] (create-property-control! (:edit-type property) workspace (fn [] (property-fn key)))
        reset-btn (doto (Button. "x")
                    (.setVisible (properties/overridden? property))
                    (ui/on-action! (fn [_]
                                     (properties/clear-override! (property-fn key))
                                     (.requestFocus control))))
        update-ui-fn (fn [property]
                       (.setVisible reset-btn (properties/overridden? property))
                       (update-ctrl-fn (properties/values property) (properties/validation-message property)))]

    (update-ui-fn property)

    (GridPane/setConstraints label 1 row)
    (GridPane/setConstraints control 2 row)
    (GridPane/setConstraints reset-btn 3 row)

    (.add (.getChildren grid) label)
    (.add (.getChildren grid) control)
    (.add (.getChildren grid) reset-btn)
    
    [key update-ui-fn]))

(defn- create-properties [workspace grid properties property-fn]
  ; TODO - add multi-selection support for properties view
  (doall (map-indexed (fn [row [key property]] (create-properties-row workspace grid key property row property-fn)) properties)))

(defn- make-grid [parent workspace properties property-fn]
  (let [grid (GridPane.)]
      (.setHgap grid grid-hgap)
      (.setVgap grid grid-vgap)
      (ui/add-child! parent grid)
      (ui/add-style! grid "form")
      (create-properties workspace grid properties property-fn)))

(defn- make-pane [parent workspace properties]
  (let [vbox (VBox. (double 10.0))]
      (.setPadding vbox (Insets. 10 10 10 10))
      (ui/user-data! vbox ::properties properties)
      (let [property-fn (fn [key]
                          (let [properties (:properties (ui/user-data vbox ::properties))]
                            (get properties key)))
            display-order (:display-order properties)
            properties (:properties properties)
            generics [nil (mapv (fn [k] [k (get properties k)]) (filter (comp not properties/category?) display-order))]
            categories (mapv (fn [order]
                               [(first order) (mapv (fn [k] [k (get properties k)]) (rest order))])
                             (filter properties/category? display-order))
            update-fns (loop [sections (cons generics categories)
                              result []]
                         (if-let [[category properties] (first sections)]
                           (let [update-fns (if (empty? properties)
                                              []
                                              (do
                                                (when category
                                                  (let [label (Label. category)]
                                                    (ui/add-child! vbox label)))
                                                (make-grid vbox workspace properties property-fn)))]
                             (recur (rest sections) (into result update-fns)))
                           result))]
        ; NOTE: Note update-fns is a sequence of [[property-key update-ui-fn] ...]
        (ui/user-data! parent ::update-fns (into {} update-fns)))
      (ui/children! parent [vbox])
      vbox))

(defn- refresh-pane [parent ^Pane pane workspace properties]
  (ui/user-data! pane ::properties properties)
  (let [update-fns (ui/user-data parent ::update-fns)]
    (doseq [[key property] (:properties properties)]
      (when-let [update-ui-fn (get update-fns key)]
        (update-ui-fn property)))))

(defn- properties->template [properties]
  (mapv (fn [[k v]] [k (select-keys v [:edit-type])]) (:properties properties)))

(defn- update-pane [parent id workspace properties]
  ; NOTE: We cache the ui based on the ::template user-data
  (let [properties (properties/coalesce properties)
        template (properties->template properties)
        prev-template (ui/user-data parent ::template)]
    (if (not= template prev-template)
      (let [pane (make-pane parent workspace properties)]
        (ui/user-data! parent ::template template)
        (g/set-property! id :prev-pane pane)
        pane)
      (do
        (let [pane (g/node-value id :prev-pane)]
          (refresh-pane parent pane workspace properties)
          pane)))))

(g/defnode PropertiesView
  (property parent-view Parent)
  (property workspace g/Any)
  (property prev-pane Pane)

  (input selected-node-properties g/Any)

  (output pane Pane :cached (g/fnk [parent-view _node-id workspace selected-node-properties] (update-pane parent-view _node-id workspace selected-node-properties))))

(defn make-properties-view [workspace project view-graph ^Node parent]
  (let [view-id       (g/make-node! view-graph PropertiesView :parent-view parent :workspace workspace)
        stage         (.. parent getScene getWindow)
        refresh-timer (ui/->timer 10 (fn [now] (g/node-value view-id :pane)))]
    (g/connect! project :selected-node-properties view-id :selected-node-properties)
    (ui/timer-stop-on-close! stage refresh-timer)
    (ui/timer-start! refresh-timer)
    view-id))
