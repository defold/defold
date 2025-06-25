;; Copyright 2020-2025 The Defold Foundation
;; Copyright 2014-2020 King
;; Copyright 2009-2014 Ragnar Svensson, Christian Murray
;; Licensed under the Defold License version 1.0 (the "License"); you may not use
;; this file except in compliance with the License.
;;
;; You may obtain a copy of the License, together with FAQs at
;; https://www.defold.com/license
;;
;; Unless required by applicable law or agreed to in writing, software distributed
;; under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
;; CONDITIONS OF ANY KIND, either express or implied. See the License for the
;; specific language governing permissions and limitations under the License.

(ns editor.properties-view
  (:require [clojure.string :as string]
            [dynamo.graph :as g]
            [editor.app-view :as app-view]
            [editor.color-dropper :as color-dropper]
            [editor.field-expression :as field-expression]
            [editor.handler :as handler]
            [editor.jfx :as jfx]
            [editor.math :as math]
            [editor.prefs :as prefs]
            [editor.properties :as properties]
            [editor.resource :as resource]
            [editor.resource-dialog :as resource-dialog]
            [editor.types :as types]
            [editor.ui :as ui]
            [editor.ui.fuzzy-combo-box :as fuzzy-combo-box]
            [editor.workspace :as workspace]
            [util.coll :as coll :refer [pair]]
            [util.id-vec :as iv]
            [util.profiler :as profiler])
  (:import [editor.properties Curve CurveSpread]
           [java.util Collection]
           [javafx.geometry Insets Point2D]
           [javafx.scene Node Parent]
           [javafx.scene.control Button CheckBox ColorPicker Control Label Slider TextArea TextField TextInputControl ToggleButton Tooltip]
           [javafx.scene.input MouseEvent MouseDragEvent]
           [javafx.scene.layout AnchorPane ColumnConstraints GridPane HBox Pane Priority Region StackPane VBox]
           [javafx.scene.paint Color]
           [javafx.util Duration]))

(set! *warn-on-reflection* true)

(def ^{:private true :const true} grid-hgap 4)
(def ^{:private true :const true} grid-vgap 6)
(def ^{:private true :const true} all-available 5000)

(defonce ^:private saved-colors-prefs-path [:workflow :saved-colors])

(declare update-field-message)

(defn- update-multi-text-fn [texts format-fn get-fns values message read-only?]
  (update-field-message texts message)
  (doseq [[^TextInputControl text get-fn] (map vector texts get-fns)]
    (doto text
      (ui/text! (format-fn (properties/unify-values (map get-fn values))))
      (ui/editable! (not read-only?)))
    (if (ui/focus? text)
      (.selectAll text)
      (.home text))))

(defn- update-text-fn
  ([^TextInputControl text format-fn values message read-only?]
   (update-text-fn text format-fn identity values message read-only?))
  ([^TextInputControl text format-fn get-fn values message read-only?]
   (update-multi-text-fn [text] format-fn [get-fn] values message read-only?)))

(defn edit-type->type [edit-type]
  (or (some-> edit-type :type g/value-type-dispatch-value)
      (:type edit-type)))

(defmulti customize! (fn [control _ _] (class control)))

(defmethod customize! TextField [^TextField t update-fn cancel-fn]
  (doto t
    (GridPane/setHgrow Priority/ALWAYS)
    (ui/on-action! update-fn)
    (ui/on-cancel! cancel-fn)
    (ui/auto-commit! update-fn)
    (ui/select-all-on-click!)))

(defmethod customize! TextArea [^TextArea t update-fn cancel-fn]
  (doto t
    (GridPane/setHgrow Priority/ALWAYS)
    (ui/on-action! update-fn)
    (ui/on-cancel! cancel-fn)
    (ui/auto-commit! update-fn)
    (ui/select-all-on-click!)))

(defn- old-num->parse-num-fn [old-num]
  {:pre [(or (number? old-num) (nil? old-num))]}
  (if (math/float32? old-num)
    field-expression/to-float
    field-expression/to-double))

(defn- coalesced-property->any-value [{:keys [values] :as coalesced-property}]
  {:pre [(vector? values)]}
  (or (first values)
      (first (:original-values coalesced-property))))

(defn- coalesced-property->old-num [coalesced-property]
  (let [old-value (coalesced-property->any-value coalesced-property)]
    (if (number? old-value)
      old-value
      (old-value 0))))

(defn- parse-num [^String text old-num]
  (let [parse-num-fn (old-num->parse-num-fn old-num)]
    (parse-num-fn text)))

(defn add-style-class!
  [^Node node style-class]
  (.add (.getStyleClass node) style-class))

(defn script-property-type->style-class [script-property-type]
  (case script-property-type
    :script-property-type-number "script-property-text-field-icon-number"
    :script-property-type-hash "script-property-text-field-icon-hash"
    :script-property-type-url "script-property-text-field-icon-url"
    nil))

(defmulti create-property-control! (fn [edit-type _ _]
                                     (edit-type->type edit-type)))

(defmethod create-property-control! g/Str [edit-type _ property-fn]
  (let [text (TextField.)
        update-ui-fn (partial update-text-fn text str)
        cancel-fn (fn [_]
                    (let [property (property-fn)]
                      (update-ui-fn (properties/values property)
                                    (properties/validation-message property)
                                    (properties/read-only? property))))
        update-fn (fn [_]
                    (properties/set-values! (property-fn) (repeat (.getText text))))]
    (customize! text update-fn cancel-fn)
    (when-let [style-class (script-property-type->style-class (:script-property-type edit-type))]
      (add-style-class! text style-class))
    [text update-ui-fn]))

(defmethod create-property-control! g/Int [edit-type _ property-fn]
  (let [text (TextField.)
        update-ui-fn (partial update-text-fn text field-expression/format-int)
        update-prop-fn (fn [_]
                         (let [property (property-fn)]
                           (update-ui-fn (properties/values property)
                                         (properties/validation-message property)
                                         (properties/read-only? property))))
        cancel-fn update-prop-fn
        update-fn (fn [_]
                    (when-let [v (field-expression/to-int (.getText text))]
                      (let [property (property-fn)]
                        (properties/set-values! property (repeat v))
                        (update-prop-fn nil))))
        drag-update-fn (fn [v update-val] (int (+ v update-val)))]
    (customize! text update-fn cancel-fn)
    (when-let [style-class (script-property-type->style-class (:script-property-type edit-type))]
      (add-style-class! text style-class))
    [text update-ui-fn drag-update-fn]))

(defmethod create-property-control! g/Num [edit-type _ property-fn]
  (let [text-field (TextField.)
        update-ui-fn (partial update-text-fn text-field field-expression/format-number)
        cancel-fn (fn [_]
                    (let [property (property-fn)]
                      (update-ui-fn (properties/values property)
                                    (properties/validation-message property)
                                    (properties/read-only? property))))
        update-fn (fn update-fn [_]
                    (let [property (property-fn)
                          old-num (coalesced-property->old-num property)
                          num (parse-num (.getText text-field) old-num)]
                      (if (and num (not= num old-num))
                        (properties/set-values! property (repeat num))
                        (cancel-fn nil))))
        drag-update-fn (fn [v update-val] (properties/round-scalar (+ v update-val)))]
    (customize! text-field update-fn cancel-fn)
    (when-let [style-class (script-property-type->style-class (:script-property-type edit-type))]
      (add-style-class! text-field style-class))
    [text-field update-ui-fn drag-update-fn]))

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

(defn- handle-control-drag-event! [property-fn drag-update-fn update-ui-fn ^MouseDragEvent event]
  (.consume event)
  (let [property (property-fn)
        target (.getTarget event)
        position (ui/user-data target ::position)
        op-seq (ui/user-data target ::op-seq)
        is-read-only (properties/read-only? property)]
    (when (and position op-seq (not is-read-only))
      (let [edit-type (:edit-type property)
            to-fn (:to-type edit-type identity)
            from-fn (:from-type edit-type identity)
            min-val (:min edit-type)
            max-val (:max edit-type)
            x (.getX event)
            y (.getY event)
            [prev-x prev-y] (ui/user-data target ::position)
            delta-x (- x prev-x)
            delta-y (- prev-y y)
            max-delta (if (> (abs delta-x) (abs delta-y)) delta-x delta-y)
            update-val (cond-> (or (:precision edit-type) 1.0)
                         (.isShiftDown event) (* 10.0)
                         (.isControlDown event) (* 0.1)
                         (neg? max-delta) -)]
        (when (> (abs max-delta) 1)
          (let [values (or (ui/user-data target ::values)
                           (properties/values property))
                new-values (mapv (fn [value]
                                   (cond-> (drag-update-fn value update-val)
                                     min-val (max min-val)
                                     max-val (min max-val))) values)]
            (properties/set-values! property values op-seq)
            (ui/user-data! target ::position [x y])
            (ui/user-data! target ::values new-values)
            (update-ui-fn (mapv (comp to-fn from-fn) new-values)
                          (properties/validation-message property)
                          (properties/read-only? property))))))))

(defn handle-label-press-event!
  [^MouseEvent event]
  (.consume event)
  (doto (.getTarget event)
    (ui/add-style! "active")
    (ui/user-data! ::op-seq (gensym))
    (ui/user-data! ::position [(.getX event) (.getY event)])
    (ui/user-data! ::values nil)))

(defn handle-label-release-event!
  [^MouseEvent event ^Node control]
  (.consume event)
  (let [target (.getTarget event)]
    (when-not (ui/user-data target ::values)
      (.requestFocus control))
    (doto target
      (ui/remove-style! "active")
      (ui/user-data! ::op-seq nil)
      (ui/user-data! ::position nil))))

(defn- make-control-draggable
  ([^Node control drag-event-handler]
   (make-control-draggable control drag-event-handler true))
  ([^Node control drag-event-handler is-left-aligned]
   (let [drag-icon (doto (Button. "" (jfx/get-image-view "icons/32/Icons_X_11_scaleupdown.png" 14))
                     (ui/add-styles! ["action-button" "drag-handle"])
                     (.setFocusTraversable false)
                     (.addEventHandler MouseEvent/MOUSE_DRAGGED (ui/event-handler event (drag-event-handler event)))
                     (.addEventHandler MouseEvent/MOUSE_PRESSED (ui/event-handler event (handle-label-press-event! event)))
                     (.addEventHandler MouseEvent/MOUSE_RELEASED (ui/event-handler event (handle-label-release-event! event control))))]
     (if is-left-aligned
       (AnchorPane/setRightAnchor drag-icon 0.0)
       (AnchorPane/setLeftAnchor drag-icon 0.0))
     (doto control
       (AnchorPane/setRightAnchor 0.0)
       (AnchorPane/setLeftAnchor 0.0))
     (ui/observe (.focusedProperty control)
                 (fn [_ _ is-focused]
                   (if is-focused
                     (ui/add-style! drag-icon "hidden")
                     (ui/remove-style! drag-icon "hidden"))))
     (doto (AnchorPane. (ui/node-array [control drag-icon]))
       (GridPane/setHgrow Priority/ALWAYS)
       (ui/add-style! "overlay-action-pane")))))

(defn- create-multi-text-field! [labels property-fn]
  (let [text-fields (mapv (fn [_] (TextField.)) labels)
        box (doto (GridPane.)
              (.setHgap grid-hgap))
        get-fns (mapv (fn [index]
                        #(nth % index))
                      (range (count text-fields)))
        update-ui-fn (partial update-multi-text-fn text-fields field-expression/format-number get-fns)]
    (dorun
      (map
        (fn [index ^TextField text-field ^String label-text]
          (let [drag-update-fn (fn [v update-val]
                                 (update v index #(properties/round-scalar (+ % update-val))))
                cancel-fn (fn [_]
                            (let [property (property-fn)
                                  current-vals (properties/values property)]
                              (update-ui-fn current-vals
                                            (properties/validation-message property)
                                            (properties/read-only? property))))
                update-fn (fn update-fn [_]
                            (let [property (property-fn)
                                  current-vals (properties/values property)
                                  old-num (get (get current-vals 0) index)
                                  num (parse-num (.getText text-field) old-num)]
                              (if (and num (not= num old-num))
                                (properties/set-values! property (mapv #(assoc % index num) current-vals))
                                (cancel-fn nil))))
                _ (customize! text-field update-fn cancel-fn)
                text-field (make-control-draggable text-field (partial handle-control-drag-event! property-fn drag-update-fn update-ui-fn) false) 
                children (if (seq label-text)
                           (let [label (doto (Label. label-text)
                                         (.setMinWidth Region/USE_PREF_SIZE))]
                             [label text-field])
                           [text-field])
                comp (doto (create-grid-pane children)
                       (GridPane/setConstraints index 0)
                       (GridPane/setHgrow Priority/ALWAYS))]
            (ui/add-child! box comp)))
        (range)
        text-fields
        labels))
    [box update-ui-fn]))

(def ^:private matrix-field-label-texts
  ;; These correspond to the semantic meaning of the linear values in the array
  ;; of doubles. We might choose to present the fields in a different order in
  ;; the property editor.
  ["X" "X" "X" "X"
   "Y" "Y" "Y" "Y"
   "Z" "Z" "Z" "Z"
   "T" "T" "T" "W"])

(defn- create-matrix-field-grid [^long row-column-count property-fn]
  (let [labels
        (for [^long row (range row-column-count)
              ^long column (range row-column-count)]
          (let [label-text (matrix-field-label-texts (+ column (* row 4)))
                presentation-row column
                presentation-column (* row 2)]
            (doto (Label. label-text)
              (.setMinWidth Region/USE_PREF_SIZE)
              (GridPane/setConstraints presentation-column presentation-row)
              (GridPane/setHgrow Priority/NEVER)
              (cond-> (zero? presentation-column)
                      (ui/add-style! "leftmost")))))

        text-fields
        (vec
          (for [^long row (range row-column-count)
                ^long column (range row-column-count)]
            (let [presentation-row column
                  presentation-column (inc (* row 2))]
              (doto (TextField.)
                (GridPane/setConstraints presentation-column presentation-row)
                (GridPane/setHgrow Priority/ALWAYS)))))

        grid-pane
        (doto (GridPane.)
          (.setHgap grid-hgap)
          (ui/add-style! "property-component-matrix")
          (ui/children! (interleave labels text-fields)))

        get-fns
        (mapv (fn [^long value-index]
                #(nth % value-index))
              (range (* row-column-count row-column-count)))

        update-ui-fn
        (partial update-multi-text-fn text-fields field-expression/format-number get-fns)

        cancel-fn
        (fn cancel-fn [_]
          (let [property (property-fn)
                current-vals (properties/values property)]
            (update-ui-fn current-vals
                          (properties/validation-message property)
                          (properties/read-only? property))))]

    (doseq [^long row (range row-column-count)
            ^long column (range row-column-count)
            :let [value-index (+ column (* row row-column-count))
                  text-field (text-fields value-index)]]
      (letfn [(update-fn [_]
                (let [property (property-fn)
                      old-vals (properties/values property)
                      old-num (get (get old-vals 0) value-index)
                      num (parse-num (ui/text text-field) old-num)]
                  (if (and num (not= num old-num))
                    (let [new-vals (mapv #(assoc % value-index num) old-vals)]
                      (properties/set-values! property new-vals))
                    (cancel-fn nil))))]
        (customize! text-field update-fn cancel-fn)))

    (pair grid-pane update-ui-fn)))

(defmethod create-property-control! types/Vec2 [edit-type _ property-fn]
  (let [{:keys [labels]
         :or {labels ["X" "Y"]}} edit-type]
    (create-multi-text-field! labels property-fn)))

(defmethod create-property-control! types/Vec3 [edit-type _ property-fn]
  (let [{:keys [labels]
         :or {labels ["X" "Y" "Z"]}} edit-type]
    (create-multi-text-field! labels property-fn)))

(defmethod create-property-control! types/Vec4 [edit-type _ property-fn]
  (let [{:keys [labels]
         :or {labels ["X" "Y" "Z" "W"]}} edit-type]
    (create-multi-text-field! labels property-fn)))

(defmethod create-property-control! types/Mat2 [_edit-type _ property-fn]
  (create-matrix-field-grid 2 property-fn))

(defmethod create-property-control! types/Mat3 [_edit-type _ property-fn]
  (create-matrix-field-grid 3 property-fn))

(defmethod create-property-control! types/Mat4 [_edit-type _ property-fn]
  (create-matrix-field-grid 4 property-fn))

(defn- create-multi-keyed-text-field! [fields property-fn]
  (let [text-fields (mapv (fn [_] (TextField.)) fields)
        box (doto (GridPane.)
              (.setPrefWidth Double/MAX_VALUE))
        get-fns (mapv (fn [f] (or (:get-fn f) #(get-in % (:path f)))) fields)
        update-ui-fn (partial update-multi-text-fn text-fields field-expression/format-number get-fns)]
    (dorun
      (map
        (fn [index ^TextField text-field field get-fn]
          (let [^String label-text (:label field)
                ^Control control (:control field)
                children (cond-> []

                                 label-text
                                 (conj (doto (Label. label-text)
                                         (.setMinWidth Region/USE_PREF_SIZE)))

                                 control
                                 (conj (doto control
                                         (.setMinWidth Region/USE_PREF_SIZE)))

                                 :always
                                 (conj text-field))
                comp (doto (create-grid-pane children)
                       (GridPane/setConstraints index 0)
                       (GridPane/setHgrow Priority/ALWAYS))
                set-fn (or (:set-fn field)
                           (let [path (:path field)]
                             (fn [e v]
                               (assoc-in e path v))))
                cancel-fn (fn [_]
                            (let [property (property-fn)
                                  current-vals (properties/values property)]
                              (update-ui-fn current-vals
                                            (properties/validation-message property)
                                            (properties/read-only? property))))
                update-fn (fn update-fn [_]
                            (let [property (property-fn)
                                  current-vals (properties/values property)
                                  old-num (get-fn (first current-vals))
                                  num (parse-num (.getText text-field) old-num)]
                              (if num
                                (properties/set-values! property (mapv #(set-fn % num) current-vals))
                                (cancel-fn nil))))]
            (customize! text-field update-fn cancel-fn)
            (ui/add-child! box comp)))
        (range)
        text-fields
        fields
        get-fns))
    [box update-ui-fn]))

(def ^:private ^:dynamic *programmatic-setting* nil)

(defn- make-curve-toggler
  ^ToggleButton [property-fn]
  (let [^ToggleButton toggle-button (doto (ToggleButton. nil (jfx/get-image-view "icons/32/Icons_X_03_Bezier.png" 12.0))
                                      (.setFocusTraversable false)
                                      (ui/add-styles! ["embedded-properties-button"]))]
    (doto toggle-button
      (ui/on-action! (fn [_]
                       (let [property (property-fn)
                             selected (.isSelected toggle-button)
                             vals (mapv (fn [curve] (let [v (-> curve
                                                                properties/curve-vals
                                                                first)]
                                                      (assoc curve :points (iv/iv-vec (cond-> [v]
                                                                                              selected
                                                                                              (conj (update v 0 #(if (math/float32? %) (float 1.0) 1.0))))))))
                                        (properties/values property))]
                         (properties/set-values! property vals)))))))

(defn- make-control-points [curve num]
  (let [old-points (properties/curve-vals curve)
        old-point (first old-points)
        old-num (second old-point)
        is-float (math/float32? old-num)
        zero (if is-float (float 0.0) 0.0)
        one (if is-float (float 1.0) 1.0)]
    (conj (empty old-points)
          (-> (empty old-point)
              (conj zero)
              (conj (cond-> num is-float float))
              (conj one)
              (conj zero)))))

(defn- curve-get-fn [curve]
  (second (first (properties/curve-vals curve))))

(defn- curve-set-fn [curve num]
  (properties/->curve (make-control-points curve num)))

(defn- curve-spread-set-fn [curve-spread num]
  (properties/->curve-spread (make-control-points curve-spread num)
                             (:spread curve-spread)))

(defn- make-curve-update-ui-fn [^ToggleButton editor-toggle-button ^TextField value-text-field update-ui-fn]
  (fn curve-update-ui-fn [values message is-read-only]
    (update-ui-fn values message is-read-only)
    (let [is-curved (< 1 (properties/curve-point-count (first values)))]
      (.setSelected editor-toggle-button is-curved)
      (ui/editable! editor-toggle-button (some? (first values)))
      (ui/disable! value-text-field is-curved))))

(defmethod create-property-control! CurveSpread [_ _ property-fn]
  (let [^ToggleButton editor-toggle-button (make-curve-toggler property-fn)
        fields [{:get-fn curve-get-fn
                 :set-fn curve-spread-set-fn
                 :control editor-toggle-button}
                {:label "+/-" :path [:spread]}]
        [^HBox box update-ui-fn] (create-multi-keyed-text-field! fields property-fn)
        ^TextField value-text-field (some #(and (instance? TextField %) %) (.getChildren ^HBox (first (.getChildren box))))
        update-ui-fn (make-curve-update-ui-fn editor-toggle-button value-text-field update-ui-fn)]
    [box update-ui-fn]))

(defmethod create-property-control! Curve [_ _ property-fn]
  (let [^ToggleButton editor-toggle-button (make-curve-toggler property-fn)
        fields [{:get-fn curve-get-fn
                 :set-fn curve-set-fn
                 :control editor-toggle-button}]
        [^HBox box update-ui-fn] (create-multi-keyed-text-field! fields property-fn)
        ^TextField value-text-field (some #(and (instance? TextField %) %) (.getChildren ^HBox (first (.getChildren box))))
        update-ui-fn (make-curve-update-ui-fn editor-toggle-button value-text-field update-ui-fn)]
    [box update-ui-fn]))

(defn- set-color-value! [property-fn ignore-alpha ^Color c]
  (let [property (property-fn)
        old-value (coalesced-property->any-value property)
        num-fn (if (math/float32? (first old-value))
                 properties/round-scalar-coarse-float
                 properties/round-scalar-coarse)
        new-value (-> (coll/empty-with-meta old-value)
                      (conj (num-fn (.getRed c)))
                      (conj (num-fn (.getGreen c)))
                      (conj (num-fn (.getBlue c)))
                      (conj (num-fn (.getOpacity c))))
        values (if ignore-alpha
                 (let [old-values (properties/values property)
                       old-alphas (map #(nth % 3) old-values)]
                   (mapv #(assoc new-value 3 %) old-alphas))
                 (repeat new-value))]
    (properties/set-values! property values)))

(defn- value->color [v]
  (let [[r g b a] v]
    (Color. r g b a)))

(defn- color->web-string [^Color c ignore-alpha]
  (cond->> (nnext (.toString c))
    ignore-alpha (drop-last 2)
    :always (apply str "#")))

(defn- save-colors!
  [colors prefs]
  (->> (mapv #(color->web-string % false) colors)
       (prefs/set! prefs saved-colors-prefs-path)))

(defn- get-saved-colors
  [prefs]
  (->> (prefs/get prefs saved-colors-prefs-path)
       (mapv #(Color/valueOf ^String %))))

(defmethod create-property-control! types/Color [edit-type {:keys [color-dropper-view prefs]} property-fn]
  (let [wrapper (doto (HBox.)
                  (.setPrefWidth Double/MAX_VALUE))
        pick-fn (fn [c] (set-color-value! property-fn (:ignore-alpha? edit-type) c))
        saved-colors ^Collection (get-saved-colors prefs)
        color-dropper (doto (Button. "" (jfx/get-image-view "icons/32/Icons_M_03_colorpicker.png" 16))
                        (ui/add-styles! ["action-button" "color-dropper"])
                        (AnchorPane/setRightAnchor 0.0)
                        (ui/on-click! (fn [^MouseEvent event] (color-dropper/activate! color-dropper-view pick-fn event))))
        text (TextField.)
        color-picker (ColorPicker.)
        ignore-alpha (:ignore-alpha? edit-type)
        value->display-color #(some-> % value->color (color->web-string ignore-alpha))
        get-overlay #(.lookup (ui/main-root) "#overlay")
        pane (doto (AnchorPane. (ui/node-array [text color-dropper]))
               (HBox/setHgrow Priority/ALWAYS)
               (ui/add-style! "overlay-action-pane"))
        update-ui-fn (fn [values message read-only?]
                       (update-text-fn text value->display-color values message read-only?)
                       (.setValue color-picker (some-> (properties/unify-values values) value->color))
                       (update-field-message [color-picker] message)
                       (ui/editable! color-picker (not read-only?)))
        cancel-fn (fn [_]
                    (let [property (property-fn)
                          current-vals (properties/values property)]
                      (update-ui-fn current-vals
                                    (properties/validation-message property)
                                    (properties/read-only? property))))
        commit-fn (fn [_]
                    (when-let [c (try (Color/valueOf (ui/text text))
                                      (catch Exception _e (cancel-fn nil)))]
                      (set-color-value! property-fn ignore-alpha c)))]
    (when (seq saved-colors)
      (.setAll (.getCustomColors color-picker) saved-colors))
    (doto text
      (AnchorPane/setRightAnchor 0.0)
      (AnchorPane/setLeftAnchor 0.0)
      (ui/add-style! "color-input")
      (customize! commit-fn cancel-fn))
    (doto color-picker
      (ui/on-action! (fn [_]
                       (let [c (.getValue color-picker)]
                         (set-color-value! property-fn ignore-alpha c)
                         (ui/user-data! (ui/main-scene) ::ui/refresh-requested? true))))
      (.setOnShown (ui/event-handler event (.setVisible ^StackPane (get-overlay) true)))
      (.setOnHidden (ui/event-handler event (.setVisible ^StackPane (get-overlay) false))))
    (ui/observe-list (.getCustomColors color-picker) (fn [_ values] (save-colors! values prefs)))
    (ui/children! wrapper [pane color-picker])
    [wrapper update-ui-fn]))

(defmethod create-property-control! :choicebox [{:keys [options]} _ property-fn]
  (let [combo-box (fuzzy-combo-box/make options)
        update-ui-fn (fn [values message read-only?]
                       (binding [*programmatic-setting* true]
                         (fuzzy-combo-box/set-value! combo-box (properties/unify-values values))
                         (update-field-message [combo-box] message)
                         (ui/disable! combo-box read-only?)))
        listen-fn (fn [_old-val new-val]
                    (when-not *programmatic-setting*
                      (properties/set-values! (property-fn) (repeat new-val))
                      (ui/user-data! (ui/main-scene) ::ui/refresh-requested? true)))]
    (fuzzy-combo-box/observe! combo-box listen-fn)
    [combo-box update-ui-fn]))

(defmethod create-property-control! resource/Resource [edit-type {:keys [workspace project]} property-fn]
  (let [box           (GridPane.)
        browse-button (doto (Button. "\u2026") ; "..." (HORIZONTAL ELLIPSIS)
                        (.setPrefWidth 26)
                        (.setFocusTraversable false)
                        (ui/add-style! "button-small"))
        open-button   (doto (Button. "" (jfx/get-image-view "icons/32/Icons_S_14_linkarrow.png" 16))
                        (.setMaxWidth 26)
                        (.setFocusTraversable false)
                        (ui/add-style! "button-small"))
        text          (TextField.)
        dialog-opts (merge
                      (if (:ext edit-type)
                        {:ext (:ext edit-type)}
                        {})
                      (if (:dialog-accept-fn edit-type)
                        {:accept-fn (:dialog-accept-fn edit-type)}
                        {}))
        update-ui-fn  (fn [values message read-only?]
                        (update-text-fn text str (fn [v] (when v (resource/proj-path v))) values message read-only?)
                        (let [val (properties/unify-values values)]
                          (ui/editable! browse-button (not read-only?))
                          (ui/editable! open-button (and (resource/openable-resource? val) (resource/exists? val)))))
        cancel-fn (fn [_]
                    (let [property (property-fn)
                          current-vals (properties/values property)]
                      (update-ui-fn current-vals
                                    (properties/validation-message property)
                                    (properties/read-only? property))))
        commit-fn (fn [_]
                    (let [path (ui/text text)
                          resource (workspace/resolve-workspace-resource workspace path)]
                      (properties/set-values! (property-fn) (repeat resource))))]
    (ui/add-style! box "composite-property-control-container")
    (ui/on-action! browse-button (fn [_] (when-let [resource (first (resource-dialog/make workspace project dialog-opts))]
                                           (properties/set-values! (property-fn) (repeat resource)))))
    (ui/on-action! open-button (fn [_]  (when-let [resource (-> (property-fn)
                                                              properties/values
                                                              properties/unify-values)]
                                          (ui/run-command open-button :file.open resource))))
    (customize! text commit-fn cancel-fn)
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
        [^TextField text-field tf-update-ui-fn] (create-property-control! {:type g/Num} context property-fn)
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
    (.setPrefColumnCount text-field (if precision (count (str precision)) 5))
    (.addEventFilter slider MouseEvent/MOUSE_PRESSED (ui/event-handler event (ui/user-data! slider ::op-seq (gensym))))
    (ui/observe
      (.valueProperty slider)
      (fn [_observable _old-val new-val]
        (when-not *programmatic-setting*
          (let [property (property-fn)
                val (if precision
                      (math/round-with-precision new-val precision)
                      new-val)
                old-num (coalesced-property->old-num property)
                num (if (math/float32? old-num)
                      (float val)
                      val)]
            (properties/set-values! property (repeat num) (ui/user-data slider ::op-seq))))))
    (ui/children! box [text-field slider])
    (GridPane/setConstraints text-field 0 0)
    (GridPane/setConstraints slider 1 0)
    (.. box getColumnConstraints (add (doto (ColumnConstraints.)
                                        (.setPercentWidth 20))))
    (.. box getColumnConstraints (add (doto (ColumnConstraints.)
                                        (.setPercentWidth 80))))
    [box update-ui-fn]))

(defmethod create-property-control! :multi-line-text [_ _ property-fn]
  (let [text (doto (TextArea.)
               (ui/add-style! "property")
               (.setMinHeight 68))
        update-ui-fn (partial update-text-fn text str)
        cancel-fn (fn [_]
                    (let [property (property-fn)]
                      (update-ui-fn (properties/values property)
                                    (properties/validation-message property)
                                    (properties/read-only? property))))
        update-fn (fn [_]
                    (properties/set-values! (property-fn) (repeat (.getText text))))]
    (customize! text update-fn cancel-fn)
    [text update-ui-fn]))

(defmethod create-property-control! :default [_ _ _]
  (let [text         (TextField.)
        wrapper      (doto (HBox.)
                       (.setPrefWidth Double/MAX_VALUE))
        update-ui-fn (partial update-text-fn text str)]
    (HBox/setHgrow text Priority/ALWAYS)
    (ui/children! wrapper [text])
    (.setDisable text true)
    [wrapper update-ui-fn]))

(defn- node-screen-coords ^Point2D [^Node node ^Point2D offset]
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
      (fn [verb _]
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

(defn- create-property-label [label key tooltip]
  (doto (Label. label)
    (.setTooltip (doto (Tooltip.)
                   (.setText (cond->> (format "Available as `%s` in editor scripts"
                                              (string/replace (name key) \- \_))
                                      tooltip
                                      (str tooltip "\n\n")))
                   (.setHideDelay Duration/ZERO)
                   (.setShowDuration (Duration/seconds 30))))
    (ui/add-style! "property-label")
    (.setMinWidth Label/USE_PREF_SIZE)
    (.setMinHeight 28.0)))

(handler/defhandler :edit.show-overrides :property
  (active? [evaluation-context selection]
    (when-let [node-id (handler/selection->node-id selection)]
      (pos? (count (g/overrides (:basis evaluation-context) node-id)))))
  (run [property selection search-results-view app-view]
    (app-view/show-override-inspector! app-view search-results-view (handler/selection->node-id selection) [(:key property)])))

(handler/register-menu! ::properties-menu
  [{:label "Show Overrides"
    :command :edit.show-overrides}])

(defrecord SelectionProvider [original-node-ids]
  handler/SelectionProvider
  (selection [_] original-node-ids)
  (succeeding-selection [_])
  (alt-selection [_]))

(defn- create-properties-row [context ^GridPane grid key property row original-node-ids property-fn]
  (let [^Label label (doto (create-property-label (properties/label property) key (properties/tooltip property))
                       (ui/context! :property (assoc context :property property) (->SelectionProvider original-node-ids))
                       (ui/register-context-menu ::properties-menu true))
        [^Node control update-ctrl-fn drag-update-fn] (create-property-control! (:edit-type property) context
                                                                 (fn [] (property-fn key)))
        reset-btn (doto (Button. nil (jfx/get-image-view "icons/32/Icons_S_02_Reset.png"))
                    (.setFocusTraversable false)
                    (ui/add-styles! ["clear-button" "button-small"])
                    (ui/on-action! (fn [_]
                                     (properties/clear-override! (property-fn key))
                                     (ui/clear-auto-commit! control)
                                     (.requestFocus label))))

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
                                         (properties/read-only? property))))
        control (cond-> control
                  drag-update-fn
                  (make-control-draggable (partial handle-control-drag-event! (fn [] property) drag-update-fn update-ctrl-fn)))]

    (update-label-box (properties/overridden? property))

    (GridPane/setConstraints label-box 0 row)
    (GridPane/setConstraints control 1 row)

    (.add (.getChildren grid) label-box)
    (.add (.getChildren grid) control)

    (GridPane/setFillWidth label-box true)
    (GridPane/setFillWidth control true)

    [key update-ui-fn]))

(defn- create-properties [context grid properties original-node-ids property-fn]
  ; TODO - add multi-selection support for properties view
  (doall (map-indexed (fn [row [key property]]
                        (create-properties-row context grid key property row original-node-ids property-fn))
                      properties)))

(defn- make-grid [parent context properties original-node-ids property-fn]
  (let [grid (doto (GridPane.)
               (.setHgap grid-hgap)
               (.setVgap grid-vgap))
        cc1  (doto (ColumnConstraints.) (.setHgrow Priority/NEVER))
        cc2  (doto (ColumnConstraints.) (.setHgrow Priority/ALWAYS) (.setPrefWidth all-available))]
    (.. grid getColumnConstraints (add cc1))
    (.. grid getColumnConstraints (add cc2))

    (ui/add-child! parent grid)
    (ui/add-style! grid "form")
    (create-properties context grid properties original-node-ids property-fn)))

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
            {:keys [display-order properties original-node-ids]} properties
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
                                                   (make-grid vbox context properties original-node-ids property-fn)))]
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

(def ^:private ephemeral-edit-type-fields [:from-type :to-type :set-fn :clear-fn :dialog-accept-fn])

(defn- edit-type->template [edit-type]
  (apply dissoc edit-type ephemeral-edit-type-fields))

(defn- properties->template [properties]
  (into {}
        (map (fn [[prop-kw {:keys [edit-type]}]]
               (let [template (edit-type->template edit-type)]
                 (pair prop-kw template))))
        (:properties properties)))

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
  (property prefs g/Any)

  (input workspace g/Any)
  (input project g/Any)
  (input app-view g/NodeID)
  (input search-results-view g/NodeID)
  (input color-dropper-view g/NodeID)
  (input selected-node-properties g/Any)

  (output pane Pane :cached (g/fnk [parent-view workspace project app-view search-results-view selected-node-properties color-dropper-view prefs]
                                   (let [context {:workspace workspace
                                                  :project project
                                                  :app-view app-view
                                                  :prefs prefs
                                                  :search-results-view search-results-view
                                                  :color-dropper-view color-dropper-view}]
                                     ;; Collecting the properties and then updating the view takes some time, but has no immediacy
                                     ;; This is effectively time-slicing it over two "frames" (or whenever JavaFX decides to run the second part)
                                     (ui/run-later
                                       (update-pane! parent-view context selected-node-properties))))))

(defn make-properties-view [workspace project app-view search-results-view view-graph color-dropper-view prefs ^Node parent]
  (first
    (g/tx-nodes-added
      (g/transact
        (g/make-nodes view-graph [view [PropertiesView :parent-view parent :prefs prefs]]
          (g/connect workspace :_node-id view :workspace)
          (g/connect project :_node-id view :project)
          (g/connect app-view :_node-id view :app-view)
          (g/connect app-view :selected-node-properties view :selected-node-properties)
          (g/connect search-results-view :_node-id view :search-results-view)
          (g/connect color-dropper-view :_node-id view :color-dropper-view))))))
