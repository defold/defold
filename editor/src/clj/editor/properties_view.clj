(ns editor.properties-view
  (:require [clojure.set :as set]
            [clojure.string :as string]
            [camel-snake-kebab :as camel]
            [dynamo.graph :as g]
            [editor.protobuf :as protobuf]
            [editor.core :as core]
            [schema.core :as s]
            [editor.dialogs :as dialogs]
            [editor.ui :as ui]
            [editor.ui.fuzzy-combo-box :as fuzzy-combo-box]
            [editor.jfx :as jfx]
            [editor.types :as types]
            [editor.properties :as properties]
            [editor.workspace :as workspace]
            [editor.resource :as resource]
            [editor.math :as math]
            [editor.field-expression :as field-expression]
            [util.id-vec :as iv]
            [util.profiler :as profiler])
  (:import [com.defold.editor Start]
           [com.dynamo.proto DdfExtensions]
           [com.google.protobuf ProtocolMessageEnum]
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
           [javafx.scene.layout Pane AnchorPane GridPane StackPane HBox VBox Priority ColumnConstraints Region]
           [javafx.scene.paint Color]
           [javafx.util Callback StringConverter]
           [java.util.prefs Preferences]
           [com.jogamp.opengl GL GL2 GLContext GLProfile GLDrawableFactory GLCapabilities]
           [com.google.protobuf ProtocolMessageEnum]
           [editor.properties CurveSpread Curve]))

(set! *warn-on-reflection* true)

(def ^{:private true :const true} grid-hgap 4)
(def ^{:private true :const true} grid-vgap 6)
(def ^{:private true :const true} all-available 5000)

(declare update-field-message)

(defn- update-multi-text-fn [texts get-fns values message read-only?]
  (update-field-message texts message)
  (doseq [[^TextInputControl text get-fn] (map vector texts get-fns)]
    (doto text
      (ui/text! (str (properties/unify-values (map get-fn values))))
      (ui/editable! (not read-only?)))
    (if (ui/focus? text)
      (.selectAll text)
      (.home text))))

(defn- update-text-fn
  ([^TextInputControl text values message read-only?]
    (update-text-fn text identity values message read-only?))
  ([^TextInputControl text get-fn values message read-only?]
    (update-multi-text-fn [text] [get-fn] values message read-only?)))

(defn edit-type->type [edit-type]
  (or (some-> edit-type :type g/value-type-dispatch-value)
      (:type edit-type)))

(defn- select-all-on-click! [^TextInputControl t]
  (doto t
    ;; Filter is necessary because the listener will be called after the text field has received focus, i.e. too late
    (.addEventFilter MouseEvent/MOUSE_PRESSED (ui/event-handler e
                                                (when (not (ui/focus? t))
                                                  (.deselect t)
                                                  (ui/user-data! t ::selection-at-focus true))))
    ;; Filter is necessary because the TextArea captures the event
    (.addEventFilter MouseEvent/MOUSE_RELEASED (ui/event-handler e
                                                 (when (ui/user-data t ::selection-at-focus)
                                                   (when (string/blank? (.getSelectedText t))
                                                     (.consume e)
                                                     (ui/run-later (.selectAll t))))
                                                 (ui/user-data! t ::selection-at-focus nil)))))

(defmulti customize! (fn [control update-fn] (class control)))

(defmethod customize! TextField [^TextField t update-fn]
  (doto t
    (GridPane/setHgrow Priority/ALWAYS)
    (ui/on-action! update-fn)
    (ui/auto-commit! update-fn)
    (select-all-on-click!)))

(defmethod customize! TextArea [^TextArea t update-fn]
  (doto t
    (GridPane/setHgrow Priority/ALWAYS)
    (ui/auto-commit! update-fn)
    (select-all-on-click!)))

(defmulti create-property-control! (fn [edit-type _ property-fn]
                                     (edit-type->type edit-type)))

(defmethod create-property-control! g/Str [_ _ property-fn]
  (let [text         (TextField.)
        update-ui-fn (partial update-text-fn text)
        update-fn    (fn [_]
                       (properties/set-values! (property-fn) (repeat (.getText text))))]
    (customize! text update-fn)
    [text update-ui-fn]))

(defmethod create-property-control! g/Int [_ _ property-fn]
  (let [text         (TextField.)
        update-ui-fn (partial update-text-fn text)
        update-fn    (fn [_]
                       (if-let [v (field-expression/to-int (.getText text))]
                         (let [property (property-fn)]
                           (properties/set-values! property (repeat v))
                           (update-ui-fn (properties/values property)
                                         (properties/validation-message property)
                                         (properties/read-only? property)))))]
    (customize! text update-fn)
    [text update-ui-fn]))

(defmethod create-property-control! g/Num [_ _ property-fn]
  (let [text         (TextField.)
        update-ui-fn (partial update-text-fn text)
        update-fn    (fn [_] (if-let [v (field-expression/to-double (.getText text))]
                               (properties/set-values! (property-fn) (repeat v))
                               (update-ui-fn (properties/values (property-fn))
                                             (properties/validation-message (property-fn))
                                             (properties/read-only? (property-fn)))))]
    (customize! text update-fn)
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

(defn- create-grid-pane ^GridPane [ctrls]
  (let [box (doto (GridPane.)
              (ui/add-style! "property-component")
              (ui/children! ctrls))]
    (doall (map-indexed (fn [idx c]
                          (GridPane/setConstraints c idx 0))
                        ctrls))
    box))

(defn- create-multi-textfield! [labels property-fn]
  (let [text-fields  (mapv (fn [_] (TextField.)) labels)
        box          (doto (GridPane.)
                       (.setHgap grid-hgap))
        get-fns (map-indexed (fn [i _] #(nth % i)) text-fields)
        update-ui-fn (partial update-multi-text-fn text-fields get-fns)]
    (doseq [[t f] (map-indexed (fn [i t]
                                 [t (fn [_]
                                      (let [v            (field-expression/to-double (.getText ^TextField t))
                                            current-vals (properties/values (property-fn))]
                                        (if v
                                          (properties/set-values! (property-fn) (mapv #(assoc (vec %) i v) current-vals))
                                          (update-ui-fn current-vals (properties/validation-message (property-fn))
                                                        (properties/read-only? (property-fn))))))])
                               text-fields)]
      (customize! t f))
    (doall (map-indexed (fn [idx [^TextField t label]]
                          (let [children (cond-> []
                                           (seq label) (conj (doto (Label. label)
                                                               (.setMinWidth Region/USE_PREF_SIZE)))
                                           true (conj t))
                                comp (doto (create-grid-pane children)
                                       (GridPane/setConstraints idx 0)
                                       (GridPane/setHgrow Priority/ALWAYS))]
                            (ui/add-child! box comp)))
             (map vector text-fields labels)))
    [box update-ui-fn]))

(defmethod create-property-control! types/Vec2 [edit-type _ property-fn]
  (let [{:keys [labels]
         :or {labels ["X" "Y"]}} edit-type]
    (create-multi-textfield! labels property-fn)))

(defmethod create-property-control! types/Vec3 [edit-type _ property-fn]
  (let [{:keys [labels]
         :or {labels ["X" "Y" "Z"]}} edit-type]
    (create-multi-textfield! labels property-fn)))

(defmethod create-property-control! types/Vec4 [_ _ property-fn]
  (create-multi-textfield! ["X" "Y" "Z" "W"] property-fn))

(defn- create-multi-keyed-textfield! [fields property-fn]
  (let [text-fields  (mapv (fn [_] (TextField.)) fields)
        box          (doto (GridPane.)
                       (.setPrefWidth Double/MAX_VALUE))
        get-fns (map (fn [f] (or (:get-fn f) #(get-in % (:path f)))) fields)
        update-ui-fn (partial update-multi-text-fn text-fields get-fns)]
    (doseq [[t f] (map (fn [f t]
                         (let [set-fn (or (:set-fn f)
                                          (fn [e v] (assoc-in e (:path f) v)))]
                           [t (fn [_]
                                (let [v            (field-expression/to-double (.getText ^TextField t))
                                      current-vals (properties/values (property-fn))]
                                  (if v
                                    (properties/set-values! (property-fn) (mapv #(set-fn % v) current-vals))
                                    (update-ui-fn current-vals (properties/validation-message (property-fn))
                                                  (properties/read-only? (property-fn))))))]))
                       fields text-fields)]
      (customize! t f))
    (doall (map-indexed (fn [idx [^TextField t f]]
                          (let [children (cond-> []
                                           (:label f)   (conj (doto (Label. (:label f))
                                                                (.setMinWidth Region/USE_PREF_SIZE)))
                                           (:control f) (conj (doto ^Control (:control f)
                                                                (.setMinWidth Region/USE_PREF_SIZE)))
                                           true         (conj t))
                                ^GridPane comp (doto (create-grid-pane children)
                                                 (GridPane/setConstraints idx 0)
                                                 (GridPane/setHgrow Priority/ALWAYS))]
                            (ui/add-child! box comp)))
             (map vector text-fields fields)))
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
        fields [{:get-fn (fn [c] (second (first (properties/curve-vals c))))
                 :set-fn (fn [c v] (properties/->curve-spread [[0 v 1 0]] (:spread c)))
                 :control toggle-button}
                {:label "+/-" :path [:spread]}]
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

(defmethod create-property-control! types/Color [edit-type _ property-fn]
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
                                              v        [(.getRed c) (.getGreen c) (.getBlue c) (.getOpacity c)]
                                              values (if (:ignore-alpha? edit-type)
                                                       (map #(assoc %1 3 %2) (repeat v) (map last (properties/values (property-fn))))
                                                       (repeat v))]
                                          (properties/set-values! (property-fn) values))))
    [color-picker update-ui-fn]))

(defmethod create-property-control! :choicebox [{:keys [options]} _ property-fn]
  (let [combo-box (fuzzy-combo-box/make options)
        update-ui-fn (fn [values message read-only?]
                       (binding [*programmatic-setting* true]
                         (fuzzy-combo-box/set-value! combo-box (properties/unify-values values))
                         (update-field-message [combo-box] message)
                         (ui/disable! combo-box read-only?)))
        listen-fn (fn [_old-val new-val]
                    (when-not *programmatic-setting*
                      (properties/set-values! (property-fn) (repeat new-val))))]
    (fuzzy-combo-box/observe! combo-box listen-fn)
    [combo-box update-ui-fn]))

(defmethod create-property-control! resource/Resource [edit-type {:keys [workspace project]} property-fn]
  (let [box           (GridPane.)
        browse-button (doto (Button. "\u2026") ; "..." (HORIZONTAL ELLIPSIS)
                        (.setPrefWidth 26)
                        (ui/add-style! "button-small"))
        open-button   (doto (Button. "" (jfx/get-image-view "icons/32/Icons_S_14_linkarrow.png" 16))
                        (.setMaxWidth 26)
                        (ui/add-style! "button-small"))
        text          (TextField.)
        dialog-opts   (if (:ext edit-type) {:ext (:ext edit-type)} {})
        update-ui-fn  (fn [values message read-only?]
                        (update-text-fn text (fn [v] (when v (resource/proj-path v))) values message read-only?)
                        (let [val (properties/unify-values values)]
                          (ui/editable! browse-button (not read-only?))
                          (ui/editable! open-button (boolean (and val (resource/proj-path val) (resource/exists? val))))))
        commit-fn     (fn [_]
                        (let [path     (ui/text text)
                              resource (workspace/resolve-workspace-resource workspace path)]
                          (properties/set-values! (property-fn) (repeat resource))))]
    (ui/add-style! box "composite-property-control-container")
    (ui/on-action! browse-button (fn [_] (when-let [resource (first (dialogs/make-resource-dialog workspace project dialog-opts))]
                                           (properties/set-values! (property-fn) (repeat resource)))))
    (ui/on-action! open-button (fn [_]  (when-let [resource (-> (property-fn)
                                                              properties/values
                                                              properties/unify-values)]
                                          (ui/run-command open-button :open {:resources [resource]}))))
    (customize! text commit-fn)
    (ui/children! box [text browse-button open-button])
    (GridPane/setConstraints text 0 0)
    (GridPane/setConstraints open-button 1 0)
    (GridPane/setConstraints browse-button 2 0)

    ; Merge the facing borders of the open and browse buttons.
    (GridPane/setMargin open-button (Insets. 0 -1 0 0))
    (.setOnMousePressed open-button (ui/event-handler _ (.toFront open-button)))
    (.setOnMousePressed browse-button (ui/event-handler _ (.toFront browse-button)))

    (doto (.. box getColumnConstraints)
      (.add (doto (ColumnConstraints.)
              (.setPrefWidth all-available)
              (.setHgrow Priority/ALWAYS)))
      (.add (doto (ColumnConstraints.)
              (.setMinWidth ColumnConstraints/CONSTRAIN_TO_PREF)
              (.setHgrow Priority/NEVER)))
      (.add (doto (ColumnConstraints.)
              (.setMinWidth ColumnConstraints/CONSTRAIN_TO_PREF)
              (.setHgrow Priority/NEVER))))
    [box update-ui-fn]))

(defmethod create-property-control! :slider [edit-type context property-fn]
  (let [box (doto (GridPane.)
              (.setHgap grid-hgap))
        [^TextField textfield tf-update-ui-fn] (create-property-control! {:type g/Num} context property-fn)
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
    (.addEventFilter slider MouseEvent/MOUSE_PRESSED (ui/event-handler event (ui/user-data! slider ::op-seq (gensym))))
    (ui/observe (.valueProperty slider) (fn [observable old-val new-val]
                                          (when-not *programmatic-setting*
                                            (let [val (if precision
                                                        (math/round-with-precision new-val precision)
                                                        new-val)]
                                              (properties/set-values! (property-fn) (repeat val) (ui/user-data slider ::op-seq))))))
    (ui/children! box [textfield slider])
    (GridPane/setConstraints textfield 0 0)
    (GridPane/setConstraints slider 1 0)
    (.. box getColumnConstraints (add (doto (ColumnConstraints.)
                                        (.setPercentWidth 20))))
    (.. box getColumnConstraints (add (doto (ColumnConstraints.)
                                        (.setPercentWidth 80))))
    [box update-ui-fn]))

(defmethod create-property-control! :multi-line-text [_ _ property-fn]
  (let [text         (doto (TextArea.)
                       (ui/add-style! "property")
                       (.setMinHeight 68))
        update-ui-fn (partial update-text-fn text)
        update-fn    #(properties/set-values! (property-fn) (repeat (.getText text)))]
    (ui/bind-key! text "Shortcut+Enter" update-fn)
    (customize! text (fn [_] (update-fn)))
    [text update-ui-fn]))

(defmethod create-property-control! :default [_ _ _]
  (let [text         (TextField.)
        wrapper      (doto (HBox.)
                       (.setPrefWidth Double/MAX_VALUE))
        update-ui-fn (partial update-text-fn text)]
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
  {:fatal "tooltip-error"
   :warning "tooltip-warning"
   :info "tooltip-info"})

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
  {:fatal "field-error"
   :warning "field-warning"
   :info "field-info"})

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
  (doto (Label. label)
    (ui/add-style! "property-label")
    (.setMinWidth Label/USE_PREF_SIZE)
    (.setMinHeight 28.0)))

(defn- create-properties-row [context ^GridPane grid key property row property-fn]
  (let [^Label label (create-property-label (properties/label property))
        [^Node control update-ctrl-fn] (create-property-control! (:edit-type property) context
                                                                 (fn [] (property-fn key)))
        reset-btn (doto (Button. nil (jfx/get-image-view "icons/32/Icons_S_02_Reset.png"))
                    (ui/add-styles! ["clear-button" "button-small"])
                    (ui/on-action! (fn [_]
                                     (properties/clear-override! (property-fn key))
                                     (ui/suppress-auto-commit! control)
                                     (.requestFocus control))))

        label-box (let [box (GridPane.)]
                    (GridPane/setFillWidth label true)
                    (.. box getColumnConstraints (add (doto (ColumnConstraints.)
                                                        (.setHgrow Priority/ALWAYS))))
                    (.. box getColumnConstraints (add (doto (ColumnConstraints.)
                                                        (.setHgrow Priority/NEVER))))
                    box)

        update-label-box (fn [overridden?]
                           (if overridden?
                             (do
                               (ui/children! label-box [label reset-btn])
                               (GridPane/setConstraints label 0 0)
                               (GridPane/setConstraints reset-btn 1 0))
                             (do
                               (ui/children! label-box [label])
                               (GridPane/setConstraints label 0 0))))

        update-ui-fn (fn [property]
                       (let [overridden? (properties/overridden? property)
                             f (if overridden? ui/add-style! ui/remove-style!)]
                         (doseq [c [label control]]
                           (f c "overridden"))
                         (update-label-box overridden?)
                         (update-ctrl-fn (properties/values property)
                                         (properties/validation-message property)
                                         (properties/read-only? property))))]

    (update-label-box (properties/overridden? property))

    (GridPane/setConstraints label-box 0 row)
    (GridPane/setConstraints control 1 row)

    (.add (.getChildren grid) label-box )
    (.add (.getChildren grid) control)

    (GridPane/setFillWidth label-box true)
    (GridPane/setFillWidth control true)

    [key update-ui-fn]))

(defn- create-properties [context grid properties property-fn]
  ; TODO - add multi-selection support for properties view
  (doall (map-indexed (fn [row [key property]]
                        (create-properties-row context grid key property row property-fn))
                      properties)))

(defn- make-grid [parent context properties property-fn]
  (let [grid (doto (GridPane.)
               (.setHgap grid-hgap)
               (.setVgap grid-vgap))
        cc1  (doto (ColumnConstraints.) (.setHgrow Priority/NEVER))
        cc2  (doto (ColumnConstraints.) (.setHgrow Priority/ALWAYS) (.setPrefWidth all-available))]
    (.. grid getColumnConstraints (add cc1))
    (.. grid getColumnConstraints (add cc2))

    (ui/add-child! parent grid)
    (ui/add-style! grid "form")
    (create-properties context grid properties property-fn)))

(defn- create-category-label [label]
  (doto (Label. label) (ui/add-style! "property-category")))

(defn- make-pane! [parent context properties]
  (let [vbox (doto (VBox. (double 10.0))
               (.setId "properties-view-pane")
               (.setPadding (Insets. 10 10 10 10))
               (.setFillWidth true)
               (AnchorPane/setBottomAnchor 0.0)
               (AnchorPane/setLeftAnchor 0.0)
               (AnchorPane/setRightAnchor 0.0)
               (AnchorPane/setTopAnchor 0.0))]
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
                                                   (make-grid vbox context properties property-fn)))]
                                (recur (rest sections) (into result update-fns)))
                              result))]
        ; NOTE: Note update-fns is a sequence of [[property-key update-ui-fn] ...]
        (ui/user-data! parent ::update-fns (into {} update-fns)))
      (ui/children! parent [vbox])
      vbox))

(defn- refresh-pane! [^Parent parent properties]
  (let [pane (.lookup parent "#properties-view-pane")
        update-fns (ui/user-data parent ::update-fns)
        prev-properties (:properties (ui/user-data pane ::properties))]
    (ui/user-data! pane ::properties properties)
    (doseq [[key property] (:properties properties)
            ;; Only update the UI when the props actually differ
            ;; Differing :edit-type's would have recreated the UI so no need to compare them here
            :when (not= (dissoc property :edit-type) (dissoc (get prev-properties key) :edit-type))]
      (when-let [update-ui-fn (get update-fns key)]
        (update-ui-fn property)))))

(def ^:private ephemeral-edit-type-fields [:from-type :to-type :set-fn])

(defn- edit-type->template [edit-type]
  (apply dissoc edit-type ephemeral-edit-type-fields))

(defn- properties->template [properties]
  (mapv (fn [[k v]] [k (edit-type->template (:edit-type v))]) (:properties properties)))

(defn- update-pane! [parent context properties]
  ; NOTE: We cache the ui based on the ::template and ::properties user-data
  (profiler/profile "properties" "update-pane"
    (let [properties (properties/coalesce properties)
          template (properties->template properties)
          prev-template (ui/user-data parent ::template)]
      (when (not= template prev-template)
        (make-pane! parent context properties)
        (ui/user-data! parent ::template template))
      (refresh-pane! parent properties))))

(g/defnode PropertiesView
  (property parent-view Parent)
  (property workspace g/Any)
  (property project g/Any)

  (input selected-node-properties g/Any)

  (output pane Pane :cached (g/fnk [parent-view workspace project selected-node-properties]
                                   (let [context {:workspace workspace :project project}]
                                     ;; Collecting the properties and then updating the view takes some time, but has no immediacy
                                     ;; This is effectively time-slicing it over two "frames" (or whenever JavaFX decides to run the second part)
                                     (ui/run-later
                                       (update-pane! parent-view context selected-node-properties))))))

(defn make-properties-view [workspace project app-view view-graph ^Node parent]
  (let [view-id       (g/make-node! view-graph PropertiesView :parent-view parent :workspace workspace :project project)
        stage         (.. parent getScene getWindow)]
    (g/connect! app-view :selected-node-properties view-id :selected-node-properties)
    view-id))
