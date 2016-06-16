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
            [editor.resource :as resource]
            [util.id-vec :as iv])
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
           [javafx.scene.control Control Button CheckBox ComboBox ColorPicker Label Slider TextField TextInputControl ToggleButton Tooltip TitledPane TextArea TreeItem Menu MenuItem MenuBar Tab ProgressBar]
           [javafx.scene.image Image ImageView WritableImage PixelWriter]
           [javafx.scene.input MouseEvent]
           [javafx.scene.layout Pane AnchorPane GridPane StackPane HBox VBox Priority ColumnConstraints]
           [javafx.scene.paint Color]
           [javafx.stage Stage FileChooser]
           [javafx.util Callback StringConverter]
           [java.io File]
           [java.nio.file Paths]
           [java.util.prefs Preferences]
           [javax.media.opengl GL GL2 GLContext GLProfile GLDrawableFactory GLCapabilities]
           [com.google.protobuf ProtocolMessageEnum]
           [editor.properties CurveSpread Curve]))

(set! *warn-on-reflection* true)

(def ^{:private true :const true} grid-hgap 4)
(def ^{:private true :const true} grid-vgap 6)

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

(declare update-field-message)

(defn- update-text-fn [^TextInputControl text values message read-only?]
  (ui/text! text (str (properties/unify-values values)))
  (update-field-message [text] message)
  (ui/editable! text (not read-only?)))

(defn- auto-commit! [^Node node update-fn]
  (ui/on-focus! node (fn [got-focus] (if got-focus
                                       (ui/user-data! node ::auto-commit? false)
                                       (when (ui/user-data node ::auto-commit?)
                                         (update-fn nil)))))
  (ui/on-edit! node (fn [old new] (ui/user-data! node ::auto-commit? true))))

(defmulti create-property-control! (fn [edit-type _ property-fn] (:type edit-type)))

(defmethod create-property-control! String [_ _ property-fn]
  (let [text         (TextField.)
        update-ui-fn (partial update-text-fn text)
        update-fn    (fn [_]
                       (properties/set-values! (property-fn) (repeat (.getText text))))]
    (doto text
      (.setPrefWidth Double/MAX_VALUE)
      (ui/on-action! update-fn)
      (auto-commit! update-fn))
    [text update-ui-fn]))

(defmethod create-property-control! g/Int [_ _ property-fn]
  (let [text         (TextField.)
        update-ui-fn (partial update-text-fn text)
        update-fn    (fn [_]
                       (if-let [v (to-int (.getText text))]
                         (let [property (property-fn)]
                           (properties/set-values! property (repeat v))
                           (update-ui-fn (properties/values property)
                                         (properties/validation-message property)
                                         (properties/read-only? property)))))]
    (doto text
      (.setPrefWidth Double/MAX_VALUE)
      (ui/on-action! update-fn)
      (auto-commit! update-fn))
    [text update-ui-fn]))

(defmethod create-property-control! g/Num [_ _ property-fn]
  (let [text         (TextField.)
        update-ui-fn (partial update-text-fn text)
        update-fn    (fn [_] (if-let [v (to-double (.getText text))]
                               (properties/set-values! (property-fn) (repeat v))
                               (update-ui-fn (properties/values (property-fn))
                                             (properties/validation-message (property-fn)))))]
    (doto text
      (.setPrefWidth Double/MAX_VALUE)
      (ui/on-action! update-fn)
      (auto-commit! update-fn))
    [text update-ui-fn]))

(defmethod create-property-control! g/Bool [_ _ property-fn]
  (let [check (CheckBox.)
        update-ui-fn (fn [values message read-only?]
                       (let [v (properties/unify-values values)]
                         (if (nil? v)
                           (.setIndeterminate check true)
                           (doto check
                             (.setIndeterminate false)
                             (.setSelected v))))
                       (update-field-message [check] message)
                       (ui/editable! check (not read-only?)))]
    (ui/on-action! check (fn [_] (properties/set-values! (property-fn) (repeat (.isSelected check)))))
    [check update-ui-fn]))

(defn- create-property-component [ctrls]
  (let [box (doto (GridPane.)
          (ui/add-style! "property-component")
          (ui/children! ctrls))]
    (doall (map-indexed (fn [idx c]
                          (GridPane/setConstraints c idx 0)
                          (if (even? idx)
                            (.. box getColumnConstraints (add (doto (ColumnConstraints.)
                                                                (.setFillWidth false))))
                            (.. box getColumnConstraints (add (doto (ColumnConstraints.)
                                                                (.setFillWidth true)
                                                                (.setHgrow Priority/ALWAYS))))))
                        ctrls))
    box))

(defn- create-multi-textfield! [labels property-fn]
  (let [text-fields  (mapv (fn [l] (TextField.)) labels)
        box          (doto (GridPane.)
                       (.setPrefWidth Double/MAX_VALUE))
        update-ui-fn (fn [values message read-only?]
                       (doseq [[^TextInputControl t v] (map-indexed (fn [i t]
                                                                      [t (str (properties/unify-values
                                                                               (map #(nth % i) values)))])
                                                                    text-fields)]
                         (ui/text! t v)
                         (ui/editable! t (not read-only?)))
                       (update-field-message text-fields message))]
    (doseq [[t f] (map-indexed (fn [i t]
                                 [t (fn [_]
                                      (let [v            (to-double (.getText ^TextField t))
                                            current-vals (properties/values (property-fn))]
                                        (if v
                                          (properties/set-values! (property-fn) (mapv #(assoc (vec %) i v) current-vals))
                                          (update-ui-fn current-vals (properties/validation-message (property-fn))
                                                        (properties/read-only? (property-fn))))))])
                               text-fields)]
      (ui/on-action! ^TextField t f)
      (auto-commit! t f))
    (doall (map-indexed (fn [idx [t label]]
                          (let [comp (create-property-component [(Label. label) t])]
                            (ui/add-child! box comp)
                            (GridPane/setConstraints comp idx 0)
                            (.. box getColumnConstraints (add (doto (ColumnConstraints.)
                                                                (.setFillWidth true))))))
                        (map vector text-fields labels)))
    [box update-ui-fn]))

(defmethod create-property-control! types/Vec3 [_ _ property-fn]
  (create-multi-textfield! ["X" "Y" "Z"] property-fn))

(defmethod create-property-control! types/Vec4 [_ _ property-fn]
  (create-multi-textfield! ["X" "Y" "Z" "W"] property-fn))

(defn- create-multi-keyed-textfield! [fields property-fn]
  (let [text-fields  (mapv (fn [_] (TextField.)) fields)
        box          (doto (HBox.)
                       (.setPrefWidth Double/MAX_VALUE)
                       (.setAlignment (Pos/BASELINE_LEFT)))
        update-ui-fn (fn [values message read-only?]
                       (doseq [[^TextInputControl t v] (map (fn [f t]
                                                              (let [get-fn (or (:get-fn f)
                                                                               #(get-in % (:path f)))]
                                                                [t (str (properties/unify-values
                                                                          (map get-fn values)))]))
                                                            fields text-fields)]
                         (ui/text! t v)
                         (ui/editable! t (not read-only?)))
                       (update-field-message text-fields message))]
    (doseq [[t f] (map (fn [f t]
                         (let [set-fn (or (:set-fn f)
                                          (fn [e v] (assoc-in e (:path f) v)))]
                           [t (fn [_]
                               (let [v            (to-double (.getText ^TextField t))
                                     current-vals (properties/values (property-fn))]
                                 (if v
                                   (properties/set-values! (property-fn) (mapv #(set-fn % v) current-vals))
                                   (update-ui-fn current-vals (properties/validation-message (property-fn))
                                                 (properties/read-only? (property-fn))))))]))
                       fields text-fields)]
      (ui/on-action! ^TextField t f)
      (auto-commit! t (fn [got-focus] (and (not got-focus) (f nil)))))
    (doseq [[t f] (map vector text-fields fields)
            :let  [children (cond-> []
                              (:label f) (conj (Label. (:label f)))
                              (:control f) (conj (:control f))
                              true (conj t))]]
      (HBox/setHgrow ^TextField t Priority/SOMETIMES)
      (.setPrefWidth ^TextField t 60)
      (-> (.getChildren box)
        (.add (create-property-component children))))
    [box update-ui-fn]))

(def ^:private ^:dynamic *programmatic-setting* nil)

(defn- make-curve-toggler ^ToggleButton [property-fn]
  (let [^ToggleButton toggle-button (doto (ToggleButton. nil (jfx/get-image-view "icons/32/Icons_X_03_Bezier.png" 12.0))
                                    (ui/add-styles! ["embedded-properties-button"]))]
    (doto toggle-button
      (ui/on-action! (fn [_]
                       (let [selected (.isSelected toggle-button)
                             vals (mapv (fn [curve] (let [v (-> curve
                                                              properties/curve-vals
                                                              first)]
                                                      (assoc curve :points (iv/iv-vec (cond-> [v]
                                                                                        selected
                                                                                        (conj (assoc v 0 1.0)))))))
                                        (properties/values (property-fn)))]
                         (properties/set-values! (property-fn) vals)))))))

(defmethod create-property-control! CurveSpread [_ _ property-fn]
  (let [^ToggleButton toggle-button (make-curve-toggler property-fn)
        fields [{:label "Value"
                 :get-fn (fn [c] (second (first (properties/curve-vals c))))
                 :set-fn (fn [c v] (properties/->curve-spread [[0 v 1 0]] (:spread c)))
                 :control toggle-button}
                {:label "Spread" :path [:spread]}]
        [^HBox box update-ui-fn] (create-multi-keyed-textfield! fields property-fn)
        ^TextField text-field (some #(and (instance? TextField %) %) (.getChildren ^HBox (first (.getChildren box))))
        update-ui-fn (fn [values message read-only?]
                       (update-ui-fn values message read-only?)
                       (let [curved? (boolean (< 1 (count (properties/curve-vals (first values)))))]
                         (.setSelected toggle-button curved?)
                         (ui/disable! text-field curved?)))]
    [box update-ui-fn]))

(defmethod create-property-control! Curve [_ _ property-fn]
  (let [^ToggleButton toggle-button (make-curve-toggler property-fn)
        fields [{:get-fn (fn [c] (second (first (properties/curve-vals c))))
                 :set-fn (fn [c v] (properties/->curve [[0 v 1 0]]))
                 :control toggle-button}]
        [^HBox box update-ui-fn] (create-multi-keyed-textfield! fields property-fn)
          ^TextField text-field (some #(and (instance? TextField %) %) (.getChildren ^HBox (first (.getChildren box))))
          update-ui-fn (fn [values message read-only?]
                         (update-ui-fn values message read-only?)
                         (let [curved? (boolean (< 1 (count (properties/curve-vals (first values)))))]
                           (.setSelected toggle-button curved?)
                           (ui/disable! text-field curved?)))]
    [box update-ui-fn]))

(defmethod create-property-control! types/Color [_ _ property-fn]
  (let [color-picker (doto (ColorPicker.)
                       (.setPrefWidth Double/MAX_VALUE))
        update-ui-fn  (fn [values message read-only?]
                        (let [v (properties/unify-values values)]
                          (if (nil? v)
                            (.setValue color-picker nil)
                            (let [[r g b a] v]
                              (.setValue color-picker (Color. r g b a)))))
                        (update-field-message [color-picker] message)
                        (ui/editable! color-picker (not read-only?)))]

    (ui/on-action! color-picker (fn [_] (let [^Color c (.getValue color-picker)
                                              v        [(.getRed c) (.getGreen c) (.getBlue c) (.getOpacity c)]]
                                          (properties/set-values! (property-fn) (repeat v)))))
    [color-picker update-ui-fn]))

(defmethod create-property-control! :choicebox [edit-type _ property-fn]
  (let [options      (:options edit-type)
        inv-options  (clojure.set/map-invert options)
        converter    (proxy [StringConverter] []
                       (toString [value]
                         (get options value (str value)))
                       (fromString [s]
                         (inv-options s)))
        cb           (doto (ComboBox.)
                       (.setPrefWidth Double/MAX_VALUE)
                       (.setConverter converter)
                       (ui/cell-factory! (fn [val]  {:text (options val)}))
                       (-> (.getItems) (.addAll (object-array (map first options)))))
        update-ui-fn (fn [values message read-only?]
                       (binding [*programmatic-setting* true]
                         (let [value (properties/unify-values values)]
                           (if (contains? options value)
                             (do
                               (.setValue cb value)
                               (.setText (.getEditor cb) (options value)))
                             (do
                               (.setValue cb nil)
                               (.. cb (getSelectionModel) (clearSelection)))))
                         (update-field-message [cb] message)
                         (ui/editable! cb (not read-only?))))]
    (ui/observe (.valueProperty cb) (fn [observable old-val new-val]
                                      (when-not *programmatic-setting*
                                        (properties/set-values! (property-fn) (repeat new-val)))))
    [cb update-ui-fn]))

(defmethod create-property-control! (g/protocol resource/Resource) [edit-type workspace property-fn]
  (let [box          (doto (GridPane.)
                       (.setPrefWidth Double/MAX_VALUE))
        button       (doto (Button. "...")
                       (ui/add-style! "small-button"))
        text         (TextField.)
        from-type    (or (:from-type edit-type) identity)
        to-type      (or (:to-type edit-type) identity)
        dialog-opts  (if (:ext edit-type) {:ext (:ext edit-type)} {})
        update-ui-fn (fn [values message read-only?]
                       (let [val (properties/unify-values (map to-type values))]
                         (ui/text! text (when val (resource/proj-path val))))
                       (update-field-message [text] message)
                       (ui/editable! text (not read-only?))
                       (ui/editable! button (not read-only?)))]
    (ui/add-style! box "composite-property-control-container")
    (ui/on-action! button (fn [_]  (when-let [resource (first (dialogs/make-resource-dialog workspace dialog-opts))]
                                     (properties/set-values! (property-fn) (repeat (from-type resource))))))
    (ui/on-action! text (fn [_] (let [path     (ui/text text)
                                      resource (workspace/resolve-workspace-resource workspace path)]
                                  (properties/set-values! (property-fn) (repeat (from-type resource))))))
    (ui/children! box [text button])
    (GridPane/setConstraints text 0 0)
    (GridPane/setConstraints button 1 0)
    (.. box getColumnConstraints (add (doto (ColumnConstraints.)
                                        (.setFillWidth true)
                                        (.setPercentWidth 90)
                                        (.setHgrow Priority/ALWAYS))))
    (.. box getColumnConstraints (add (doto (ColumnConstraints.)
                                        (.setFillWidth true)
                                        (.setPercentWidth 10))))
    [box update-ui-fn]))

(defmethod create-property-control! :slider [edit-type workspace property-fn]
  (let [box (doto (GridPane.)
              (.setPrefWidth Double/MAX_VALUE))
        [^TextField textfield tf-update-ui-fn] (create-property-control! {:type g/Num} workspace property-fn)
        min (:min edit-type 0.0)
        max (:max edit-type 1.0)
        val (:value edit-type max)
        precision (:precision edit-type)
        slider (Slider. min max val)
        update-ui-fn (fn [values message read-only?]
                       (tf-update-ui-fn values message read-only?)
                       (binding [*programmatic-setting* true]
                         (if-let [v (properties/unify-values values)]
                           (doto slider
                             (.setDisable false)
                             (.setValue v))
                           (.setDisable slider true))
                         (update-field-message [slider] message)
                         (ui/editable! slider (not read-only?))))]
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
    (GridPane/setConstraints textfield 0 0)
    (GridPane/setConstraints slider 1 0)
    (.. box getColumnConstraints (add (doto (ColumnConstraints.)
                                        (.setFillWidth true)
                                        (.setPercentWidth 20))))
    (.. box getColumnConstraints (add (doto (ColumnConstraints.)
                                        (.setFillWidth true)
                                        (.setPercentWidth 80))))
    [box update-ui-fn]))

(defmethod create-property-control! :default [_ _ _]
  (let [text         (TextField.)
        wrapper      (doto (HBox.)
                       (.setPrefWidth Double/MAX_VALUE))
        update-ui-fn (fn [values message read-only?]
                       (ui/text! text (properties/unify-values (map str values)))
                       (update-field-message [wrapper] message)
                       (ui/editable! text (not read-only?)))]
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

(def ^:private severity-tooltip-style-map
  {g/FATAL "tooltip-error"
   g/SEVERE "tooltip-error"
   g/WARNING "tooltip-warning"
   g/INFO "tooltip-warning"})

(defn- show-message-tooltip [^Node control]
  (when-let [tip (ui/user-data control ::field-message)]
    ;; FIXME: Hack to position tooltip somewhat below control so .show doesn't immediately trigger an :exit.
    (let [tooltip (Tooltip. (:message tip))
          control-bounds (.getLayoutBounds control)
          tooltip-coords (node-screen-coords control (Point2D. 0.0 (+ (.getHeight control-bounds) 11.0)))]
      (ui/add-style! tooltip (severity-tooltip-style-map (:severity tip)))
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
  (ui/user-data! ctrl ::field-message msg)
  (ui/on-mouse! ctrl
    (when msg
      (fn [verb e]
        (condp = verb
          :enter (show-message-tooltip ctrl)
          :exit (hide-message-tooltip ctrl)
          :move nil)))))

(def ^:private severity-field-style-map
  {g/FATAL "field-error"
   g/SEVERE "field-error"
   g/WARNING "field-warning"
   g/INFO "field-warning"})

(defn- update-field-message-style [ctrl msg]
  (if msg
    (ui/add-style! ctrl (severity-field-style-map (:severity msg)))
    (ui/remove-styles! ctrl (vals severity-field-style-map))))

(defn- update-field-message [ctrls msg]
  (doseq [ctrl ctrls]
    (install-tooltip-message ctrl msg)
    (update-field-message-style ctrl msg)
    (if msg
      (update-message-tooltip ctrl)
      (hide-message-tooltip ctrl))))

(defn- create-property-label [label]
  (doto (Label. label) (ui/add-style! "property-label")))

(defn- create-properties-row [workspace ^GridPane grid key property row property-fn]
  (let [label (create-property-label (properties/label property))
        [^Node control update-ctrl-fn] (create-property-control! (:edit-type property) workspace
                                                                 (fn [] (property-fn key)))
        reset-btn (doto (Button. nil (jfx/get-image-view "icons/32/Icons_S_02_Reset.png"))
                    (.setVisible (properties/overridden? property))
                    (ui/add-styles! ["clear-button" "small-button"])
                    (ui/on-action! (fn [_]
                                     (properties/clear-override! (property-fn key))
                                     (.requestFocus control))))
        update-ui-fn (fn [property]
                       (let [overridden? (properties/overridden? property)]
                         (let [f (if overridden? ui/add-style! ui/remove-style!)]
                           (doseq [c [label control]]
                             (f c "overridden")))
                         (.setVisible reset-btn overridden?)
                         (update-ctrl-fn (properties/values property)
                                         (properties/validation-message property)
                                         (properties/read-only? property))))]

    (GridPane/setConstraints label 0 row)
    (GridPane/setConstraints reset-btn 1 row)
    (GridPane/setConstraints control 2 row)

    (.add (.getChildren grid) label)
    (.add (.getChildren grid) control)
    (.add (.getChildren grid) reset-btn)

    (GridPane/setFillWidth label true)
    (GridPane/setFillWidth reset-btn true)
    (GridPane/setFillWidth control true)

    [key update-ui-fn]))

(defn- create-properties [workspace grid properties property-fn]
  ; TODO - add multi-selection support for properties view
  (doall (map-indexed (fn [row [key property]]
                        (create-properties-row workspace grid key property row property-fn))
                      properties)))

(defn- make-grid [parent workspace properties property-fn]
  (let [grid (doto (GridPane.)
               (.setHgap grid-hgap)
               (.setVgap grid-vgap))
        cc1  (doto (ColumnConstraints.) (.setFillWidth true) (.setHgrow Priority/SOMETIMES))
        cc2  (doto (ColumnConstraints.) (.setFillWidth true) (.setHgrow Priority/NEVER))
        cc3  (doto (ColumnConstraints.) (.setFillWidth true) (.setPercentWidth 70) (.setHgrow Priority/ALWAYS))]
    (.. grid getColumnConstraints (add cc1))
    (.. grid getColumnConstraints (add cc2))
    (.. grid getColumnConstraints (add cc3))

    (ui/add-child! parent grid)
    (ui/add-style! grid "form")
    (create-properties workspace grid properties property-fn)))

(defn- create-category-label [label]
  (doto (Label. label) (ui/add-style! "property-category")))

(defn- make-pane [parent workspace properties]
  (let [vbox (doto (VBox. (double 10.0))
               (.setPadding (Insets. 10 10 10 10))
               (.setFillWidth true)
               (AnchorPane/setBottomAnchor 0.0)
               (AnchorPane/setLeftAnchor 0.0)
               (AnchorPane/setRightAnchor 0.0)
               (AnchorPane/setTopAnchor 0.0))]
      (ui/user-data! vbox ::properties properties)
      (let [property-fn   (fn [key]
                            (let [properties (:properties (ui/user-data vbox ::properties))]
                              (get properties key)))
            display-order (:display-order properties)
            properties    (:properties properties)
            generics      [nil (mapv (fn [k] [k (get properties k)]) (filter (comp not properties/category?) display-order))]
            categories    (mapv (fn [order]
                                  [(first order) (mapv (fn [k] [k (get properties k)]) (rest order))])
                                (filter properties/category? display-order))
            update-fns    (loop [sections (cons generics categories)
                                 result   []]
                            (if-let [[category properties] (first sections)]
                              (let [update-fns (if (empty? properties)
                                                 []
                                                 (do
                                                   (when category
                                                     (let [label (create-category-label category)]
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
    (when (not= template prev-template)
      (let [pane (make-pane parent workspace properties)]
        (ui/user-data! parent ::template template)
        (g/set-property! id :prev-pane pane)))
    (let [pane (g/node-value id :prev-pane)]
      (refresh-pane parent pane workspace properties)
      pane)))

(g/defnode PropertiesView
  (property parent-view Parent)
  (property workspace g/Any)
  (property prev-pane Pane)

  (input selected-node-properties g/Any)

  (output pane Pane :cached (g/fnk [parent-view _node-id workspace selected-node-properties]
                                   (update-pane parent-view _node-id workspace selected-node-properties))))

(defn make-properties-view [workspace project view-graph ^Node parent]
  (let [view-id       (g/make-node! view-graph PropertiesView :parent-view parent :workspace workspace)
        stage         (.. parent getScene getWindow)]
    (g/connect! project :selected-node-properties view-id :selected-node-properties)
    view-id))
