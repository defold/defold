;; Copyright 2020-2026 The Defold Foundation
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
  (:require [cljfx.api :as fx]
            [cljfx.fx.column-constraints :as fx.column-constraints]
            [cljfx.fx.slider :as fx.slider]
            [cljfx.lifecycle :as fx.lifecycle]
            [cljfx.mutator :as fx.mutator]
            [cljfx.prop :as fx.prop]
            [clojure.string :as string]
            [dynamo.graph :as g]
            [editor.app-view :as app-view]
            [editor.error-reporting :as error-reporting]
            [editor.field-expression :as field-expression]
            [editor.fxui :as fxui]
            [editor.fxui.combo-box :as fxui.combo-box]
            [editor.handler :as handler]
            [editor.localization :as localization]
            [editor.math :as math]
            [editor.menu-items :as menu-items]
            [editor.properties :as properties]
            [editor.resource :as resource]
            [editor.resource-dialog :as resource-dialog]
            [editor.types :as types]
            [editor.ui :as ui]
            [editor.workspace :as workspace]
            [util.coll :as coll :refer [pair]]
            [util.defonce :as defonce]
            [util.eduction :as e]
            [util.id-vec :as iv])
  (:import [editor.properties Curve CurveSpread]
           [javafx.beans.value ChangeListener]
           [javafx.css PseudoClass]
           [javafx.event Event EventHandler]
           [javafx.scene Node Parent]
           [javafx.scene.control Slider]
           [javafx.scene.input DragEvent KeyCode KeyEvent MouseEvent TransferMode]
           [javafx.scene.paint Color]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(defn edit-type->type [edit-type]
  (or (some-> edit-type :type g/value-type-dispatch-value)
      (:type edit-type)))

(defn- old-num->round-num-fn [old-num]
  {:pre [(or (number? old-num) (nil? old-num))]}
  (if (math/float32? old-num)
    properties/round-scalar-float
    properties/round-scalar))

(defn- coalesced-property->any-value [{:keys [values] :as coalesced-property}]
  {:pre [(vector? values)]}
  (or (first values)
      (first (:original-values coalesced-property))))

(defn- coalesced-property->old-num [coalesced-property]
  (let [old-value (coalesced-property->any-value coalesced-property)]
    (cond
      (number? old-value) old-value
      (nil? old-value) 0.0
      :else (old-value 0))))

(defn script-property-type->style-class [script-property-type]
  (case script-property-type
    :script-property-type-number "script-property-text-field-icon-number"
    :script-property-type-hash "script-property-text-field-icon-hash"
    :script-property-type-url "script-property-text-field-icon-url"
    nil))

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

(handler/defhandler :edit.show-overrides :property
  (active? [evaluation-context selection]
    (when-let [node-id (handler/selection->node-id selection evaluation-context)]
      (pos? (count (g/overrides (:basis evaluation-context) node-id)))))
  (run [property selection search-results-view app-view workspace]
    (g/let-ec [localization (workspace/localization workspace evaluation-context)
               node-id (handler/selection->node-id selection evaluation-context)]
      (app-view/show-override-inspector!
        app-view
        search-results-view
        node-id
        [(:key property)]
        localization))))

(handler/defhandler :edit.pull-up-overrides :property
  (label [property user-data]
    (when (nil? user-data)
      (localization/message "command.edit.pull-up-overrides.variant.property" {"property" (properties/label property)})))
  (active? [property user-data]
    (or (some? user-data)
        (= 1 (count (:original-values property)))))
  (enabled? [user-data]
    (if user-data
      (properties/can-transfer-overrides? (:transfer-overrides-plan user-data))
      true))
  (options [property selection user-data evaluation-context]
    (when (nil? user-data)
      (when-let [node-id (handler/selection->node-id selection evaluation-context)]
        (let [prop-kws [(:key property)]
              source-prop-infos-by-prop-kw (properties/transferred-properties node-id prop-kws evaluation-context)]
          (when source-prop-infos-by-prop-kw
            (mapv (fn [transfer-overrides-plan]
                    {:label (properties/transfer-overrides-description transfer-overrides-plan evaluation-context)
                     :command :edit.pull-up-overrides
                     :user-data {:transfer-overrides-plan transfer-overrides-plan}})
                  (properties/pull-up-overrides-plan-alternatives node-id source-prop-infos-by-prop-kw evaluation-context)))))))
  (run [user-data]
    (properties/transfer-overrides! (:transfer-overrides-plan user-data))))

(handler/defhandler :edit.push-down-overrides :property
  (label [property user-data]
    (when (nil? user-data)
      (localization/message "command.edit.push-down-overrides.variant.property" {"property" (properties/label property)})))
  (active? [property selection user-data evaluation-context]
    (or (some? user-data)
        (and (= 1 (count (:original-values property)))
             (if-let [node-id (handler/selection->node-id selection evaluation-context)]
               (not (coll/empty? (g/overrides (:basis evaluation-context) node-id)))
               false))))
  (enabled? [user-data]
    (if user-data
      (properties/can-transfer-overrides? (:transfer-overrides-plan user-data))
      true))
  (options [property selection user-data evaluation-context]
    (when (nil? user-data)
      (when-let [node-id (handler/selection->node-id selection evaluation-context)]
        (let [prop-kws [(:key property)]
              source-prop-infos-by-prop-kw (properties/transferred-properties node-id prop-kws evaluation-context)]
          (when source-prop-infos-by-prop-kw
            (mapv (fn [transfer-overrides-plan]
                    {:label (properties/transfer-overrides-description transfer-overrides-plan evaluation-context)
                     :command :edit.push-down-overrides
                     :user-data {:transfer-overrides-plan transfer-overrides-plan}})
                  (properties/push-down-overrides-plan-alternatives node-id source-prop-infos-by-prop-kw evaluation-context)))))))
  (run [user-data]
    (properties/transfer-overrides! (:transfer-overrides-plan user-data))))

(handler/defhandler :private/clear-override :property
  (label [property]
    (localization/message "command.private.clear-override" {"property" (properties/label property)}))
  (active? [property]
    (not (coll/empty? (:original-values property))))
  (run [property]
    (properties/clear-override! property)))

(handler/register-menu! ::property-menu
  [menu-items/show-overrides
   menu-items/pull-up-overrides
   menu-items/push-down-overrides
   {:icon "icons/32/Icons_S_02_Reset.png"
    :command :private/clear-override}])

(defonce/record SelectionProvider [original-node-ids]
  handler/SelectionProvider
  (selection [_this _evaluation-context] original-node-ids)
  (succeeding-selection [_this _evaluation-context])
  (alt-selection [_this _evaluation-context]))

(defn- category-property-edit-types [{:keys [display-order properties]}]
  (into [(pair nil
               (into []
                     (comp (remove properties/category?)
                           (map (fn [property-keyword]
                                  (pair property-keyword
                                        (properties property-keyword)))))
                     display-order))]
        (comp (filter properties/category?)
              (map (fn [[category-title & property-keywords]]
                     (pair category-title
                           (mapv (fn [property-keyword]
                                   (pair property-keyword
                                         (properties property-keyword)))
                                 property-keywords)))))
        display-order))

(defn- set-values!
  ([property values]
   (set-values! property values (gensym)))
  ([property values opseq]
   (properties/set-values! property values opseq)
   ;; may be nil in tests
   (some-> (ui/main-stage) (-> .getScene (ui/user-data! ::ui/refresh-requested? true)))))

(defmulti make-control-view
  (fn [property _context _localization-state]
    (edit-type->type (:edit-type property))))

(defn control-view [{:keys [property context localization-state]}]
  (make-control-view property context localization-state))

(defn- property-validation-tooltip [property localization-state]
  (when-let [{:keys [severity message]} (properties/validation-message property)]
    {:severity (case severity :fatal :error :warning :warning :info)
     :message (localization-state message)}))

(defn- resolve-validation
  "If property has validation message, set :color and :tooltip input-desc props"
  [input-desc property localization-state]
  (if-let [{:keys [severity] :as tooltip} (property-validation-tooltip property localization-state)]
    (-> input-desc
        (fxui/apply-tooltip tooltip)
        (cond-> (not= severity :info) (assoc :color severity)))
    input-desc))

(defn- resolve-script-property-style-class [input-desc property]
  (if-let [style-class (script-property-type->style-class (:script-property-type (:edit-type property)))]
    (update input-desc :style-class coll/conj-vector style-class)
    input-desc))

(defn- resolve-value [input-desc property]
  (assoc input-desc :value (properties/unify-values (properties/values property))))

(defmethod make-control-view g/Str [property _context localization-state]
  (-> {:fx/type fxui/value-field
       :on-value-changed #(set-values! property (repeat %))
       :editable (not (properties/read-only? property))}
      (resolve-value property)
      (resolve-validation property localization-state)
      (resolve-script-property-style-class property)))

(defn- resolve-scrubber
  "Adds a scrubber to a text field

  Args:
    input-desc    cljfx view description
    property      coalesced property
    alignment     hover overlay alignment
    ->value       2-arg function that receives a value of a single uncoalesced
                  property and a new scrubbed value (double), should return the
                  new value
    ->number      1-arg function that converts a value of a single uncoalesced
                  property and extracts a numeric value from it"
  ([input-desc property alignment ->value]
   (resolve-scrubber input-desc property alignment ->value identity))
  ([input-desc property alignment ->value ->number]
   (cond-> input-desc
           (not (properties/read-only? property))
           (assoc
             :hover-overlay
             {:fx/type fxui/hover-overlay
              :padding 1
              :alignment alignment
              :content
              {:fx/type fxui/scrubber
               :on-scrubbed
               (fn []
                 (let [{min-value :min
                        max-value :max
                        :keys [precision]
                        :or {precision 1.0}} (:edit-type property)
                       precision (double precision)
                       opseq (gensym)
                       initial-values (properties/values property)
                       doubles-vol (volatile! (mapv (comp double ->number) initial-values))]
                   (fn [^double delta]
                     (let [new-doubles (vswap!
                                         doubles-vol
                                         coll/mapv>
                                         #(cond-> (+ ^double % (* delta precision))
                                                  min-value (max (double min-value))
                                                  max-value (min (double max-value))))
                           new-values (mapv #(->value (initial-values %) (new-doubles %))
                                            (range (count new-doubles)))]
                       (set-values! property new-values opseq)))))}}))))

(defn- scrubbed-int [_ ^double n]
  (int (Math/round n)))

(defmethod make-control-view g/Int [property _context localization-state]
  (-> {:fx/type fxui/value-field
       :to-string field-expression/format-int
       :to-value field-expression/to-int
       :on-value-changed #(set-values! property (repeat %))
       :editable (not (properties/read-only? property))}
      (resolve-value property)
      (resolve-scrubber property :right scrubbed-int)
      (resolve-validation property localization-state)
      (resolve-script-property-style-class property)))

(defmethod make-control-view g/Num [property _context localization-state]
  (let [old-num (coalesced-property->old-num property)
        is-float (math/float32? old-num)]
    (-> {:fx/type fxui/value-field
         :to-string field-expression/format-number
         :to-value field-expression/to-double
         :on-value-changed #(set-values! property (repeat (cond-> % is-float float)))
         :editable (not (properties/read-only? property))}
        (resolve-value property)
        (resolve-scrubber property :right #((old-num->round-num-fn old-num) %2))
        (resolve-validation property localization-state)
        (resolve-script-property-style-class property))))

(defmethod make-control-view g/Bool [property _context localization-state]
  (let [value (properties/unify-values (properties/values property))]
    ;; Wrap in h-box so it's not extended right with a hoverable empty space
    {:fx/type fxui/horizontal
     :children
     [(-> {:fx/type fxui/check-box
           :indeterminate (nil? value)
           :selected (boolean value)
           :on-selected-changed #(set-values! property (repeat %))
           :disable (properties/read-only? property)}
          (resolve-validation property localization-state))]}))

(defn- make-number-tuple-view [labels property localization-state]
  (let [old-num (coalesced-property->old-num property)
        is-float (math/float32? old-num)
        values (properties/values property)]
    {:fx/type fxui/grid
     :spacing :medium
     :column-constraints (vec
                           (repeat
                             (count labels)
                             {:fx/type fx.column-constraints/lifecycle
                              :percent-width (* 100 (double (/ 1 (count labels))))}))
     :children
     (coll/transfer labels []
       (map-indexed
         (fn [i label]
           {:fx/type fxui/horizontal
            :grid-pane/column i
            :spacing :small
            :children
            [{:fx/type fxui/label
              :color :hint
              :text label}
             (-> {:fx/type fxui/value-field
                  :alignment :right
                  :h-box/hgrow :always
                  :to-string field-expression/format-number
                  :to-value field-expression/to-double
                  :value (properties/unify-values (mapv #(nth % i) values))
                  :on-value-changed (fn [new-num]
                                      (set-values! property (mapv #(assoc % i (cond-> new-num is-float float)) values)))
                  :editable (not (properties/read-only? property))}
                 (resolve-validation property localization-state)
                 (resolve-scrubber
                   property
                   :left
                   #(assoc %1 i ((old-num->round-num-fn old-num) %2))
                   #(% i)))]})))}))

(defmethod make-control-view types/Vec2 [property _context localization-state]
  (make-number-tuple-view (:labels (:edit-type property) ["X" "Y"]) property localization-state))

(defmethod make-control-view types/Vec3 [property _context localization-state]
  (make-number-tuple-view (:labels (:edit-type property) ["X" "Y" "Z"]) property localization-state))

(defmethod make-control-view types/Vec4 [property _context localization-state]
  (make-number-tuple-view (:labels (:edit-type property) ["X" "Y" "Z" "W"]) property localization-state))

(defn curved-values? [property-values]
  (< 1 (properties/curve-point-count (first property-values))))

(defn- curve-toggler [{:keys [property]}]
  (let [values (properties/values property)]
    {:fx/type fxui/toggle-button
     :selected (curved-values? values)
     :disable (or (properties/read-only? property) (nil? (first values)))
     :focus-traversable false
     :graphic {:fx/type ui/image-icon
               :path "icons/32/Icons_X_03_Bezier.png"
               :size 12.0}
     :on-selected-changed
     (fn [selected]
       (set-values!
         property
         (mapv
           (fn [curve]
             (let [v (-> curve properties/curve-vals first)
                   points (iv/iv-vec (cond-> [v] selected (conj (update v 0 #(if (math/float32? %) (float 1.0) 1.0)))))]
               (assoc curve :points points)))
           values)))}))

(defn- curve-component-view [{:keys [property localization-state set-fn]}]
  (let [values (properties/values property)]
    {:fx/type fxui/horizontal
     :spacing :small
     :children
     [{:fx/type curve-toggler
       :property property}
      (-> {:fx/type fxui/value-field
           :alignment :right
           :h-box/hgrow :always
           :to-string field-expression/format-number
           :to-value field-expression/to-double
           :value (properties/unify-values (mapv curve-get-fn values))
           :editable (not (properties/read-only? property))
           :disable (curved-values? values)
           :on-value-changed (fn [new-num]
                               (set-values! property (mapv #(set-fn % new-num) (properties/values property))))}
          (resolve-validation property localization-state)
          (resolve-scrubber property :left #(set-fn %1 (properties/round-scalar %2)) curve-get-fn))]}))

(defmethod make-control-view Curve [property _context localization-state]
  {:fx/type curve-component-view
   :property property
   :localization-state localization-state
   :set-fn curve-set-fn})

(defmethod make-control-view CurveSpread [property _context localization-state]
  (let [values (properties/values property)]
    {:fx/type fxui/grid
     :spacing :small
     :column-constraints [{:fx/type fx.column-constraints/lifecycle
                           :percent-width 55}
                          {:fx/type fx.column-constraints/lifecycle
                           :percent-width 45}]
     :children
     [{:fx/type curve-component-view
       :grid-pane/column 0
       :property property
       :localization-state localization-state
       :set-fn curve-spread-set-fn}
      {:fx/type fxui/horizontal
       :grid-pane/column 1
       :spacing :small
       :children
       [{:fx/type fxui/label :text "+/-" :color :hint}
        (-> {:fx/type fxui/value-field
             :alignment :right
             :h-box/hgrow :always
             :to-string field-expression/format-number
             :to-value field-expression/to-float
             :value (properties/unify-values (map :spread values))
             :editable (not (properties/read-only? property))
             :on-value-changed (fn [spread]
                                 (set-values! property (mapv #(assoc % :spread spread) (properties/values property))))}
            (resolve-validation property localization-state)
            (resolve-scrubber property :left #(assoc %1 :spread (properties/round-scalar-float %2)) :spread))]}]}))

(defn- vec->color [[r g b a]]
  (Color. (float r) (float g) (float b) (float a)))

(defmethod make-control-view types/Color [property {:keys [color-dropper-view prefs]} localization-state]
  (let [values (properties/values property)
        value (properties/unify-values values)
        ignore-alpha (:ignore-alpha (:edit-type property))
        old-num (coalesced-property->old-num property)
        coerce (if (math/float32? old-num) float double)]
    (-> {:fx/type fxui/color-picker
         :value (some-> value vec->color)
         :on-value-changed
         (fn [^Color new-color]
           (set-values! property
                        (mapv (fn [old-value]
                                (-> old-value
                                    (assoc 0 (coerce (.getRed new-color))
                                           1 (coerce (.getGreen new-color))
                                           2 (coerce (.getBlue new-color)))
                                    (cond-> (not ignore-alpha)
                                            (assoc 3 (coerce (.getOpacity new-color))))))
                              values)))
         :ignore-alpha ignore-alpha
         :color-dropper-view color-dropper-view
         :prefs prefs
         :editable (not (properties/read-only? property))}
        (resolve-validation property localization-state))))

(defn- make-choicebox-to-string [options]
  (let [options (into {} options)]
    #(or (options %) (str %))))

(defmethod make-control-view :choicebox [property _context localization-state]
  (let [options (:options (:edit-type property))]
    {:fx/type fxui/ext-memo
     :fn make-choicebox-to-string
     :args [options]
     :key :to-string
     :desc (-> {:fx/type fxui.combo-box/view
                :disable (properties/read-only? property)
                :value (properties/unify-values (properties/values property))
                :on-value-changed #(set-values! property (repeat %))
                :items (mapv first options)
                :filter-prompt-text (localization-state (localization/message "ui.combo-box.filter-prompt"))
                :no-items-text (localization-state (localization/message "ui.combo-box.no-items"))
                :not-found-text (localization-state (localization/message "ui.combo-box.not-found"))}
               (resolve-validation property localization-state))}))

(defn- single-drag-resource [^DragEvent e valid-extensions workspace]
  {:pre [(set? valid-extensions)]}
  (let [files (.getFiles (.getDragboard e))]
    (when-some [file (when (= 1 (count files))
                       (first files))]
      (when-some [proj-path (workspace/as-proj-path workspace file)]
        (let [resource (workspace/resolve-workspace-resource workspace proj-path)
              resource-ext (resource/type-ext resource)]
          (when (and (resource/exists? resource)
                     (contains? valid-extensions resource-ext))
            resource))))))

(defmethod make-control-view resource/Resource [property {:keys [workspace project]} localization-state]
  (let [value (properties/unify-values (properties/values property))
        {:keys [ext dialog-accept-fn]} (:edit-type property)
        ext-set (if (string? ext) #{ext} (set ext))
        dialog-opts (cond-> {}
                            ext (assoc :ext ext)
                            dialog-accept-fn (assoc :accept-fn dialog-accept-fn))
        read-only (properties/read-only? property)
        openable (and (resource/openable-resource? value) (resource/exists? value))
        tooltip (property-validation-tooltip property localization-state)]
    (cond->
      {:fx/type fxui/horizontal
       :style-class "ext-resource-picker"
       :pseudo-classes (case (:severity (properties/validation-message property))
                         :fatal #{:error}
                         :warning #{:warning}
                         #{})
       :on-drag-over
       (fn [^DragEvent e]
         (when (single-drag-resource e ext-set workspace)
           (.acceptTransferModes e TransferMode/COPY_OR_MOVE)
           (.consume e)))
       :on-drag-dropped
       (fn [^DragEvent e]
         (when-let [resource (single-drag-resource e ext-set workspace)]
           (set-values! property (repeat resource))
           (.setDropCompleted e true)
           (.consume e)))
       :on-drag-entered
       (fn [^DragEvent e]
         (.pseudoClassStateChanged ^Node (.getTarget e) (PseudoClass/getPseudoClass "drag-over") true)
         (.pseudoClassStateChanged ^Node (.getTarget e)
                                   (PseudoClass/getPseudoClass "drag-invalid")
                                   (not (boolean (single-drag-resource e ext-set workspace)))))
       :on-drag-exited
       (fn [^DragEvent e]
         (.pseudoClassStateChanged ^Node (.getTarget e) (PseudoClass/getPseudoClass "drag-over") false))
       :children
       [{:fx/type fxui/value-field
         :h-box/hgrow :always
         :value (some-> value resource/proj-path)
         :on-value-changed #(set-values! property (repeat (workspace/resolve-workspace-resource workspace %)))
         :editable (not read-only)
         :style-class "ext-resource-picker-field"}
        {:fx/type fxui/button
         :graphic {:fx/type ui/image-icon
                   :path "icons/32/Icons_S_14_linkarrow.png"
                   :size 16.0}
         :style-class "ext-resource-picker-open-button"
         :disable (not openable)
         :on-action #(when openable (ui/run-command (.getSource ^Event %) :file.open value))}
        {:fx/type fxui/button
         :text "..."
         :style-class "ext-resource-picker-browse-button"
         :disable read-only
         :on-action (fn [_]
                      (when-let [resource (first (resource-dialog/make workspace project dialog-opts))]
                        (set-values! property (repeat resource))))}]}
      tooltip (fxui/apply-tooltip tooltip))))

(def ^:private prop-slider-on-value-changed
  (fx/make-binding-prop
    (fn [^Slider slider on-value-changed]
      (let [value-changed-fn (volatile! nil)
            ^EventHandler on-mouse-pressed (fn on-mouse-pressed [_]
                                             (let [f (on-value-changed)]
                                               (when-not (fn? f)
                                                 (throw (IllegalStateException. "The on-value-changed callback must return a function")))
                                               (vreset! value-changed-fn f)))
            ^EventHandler on-mouse-released (fn on-mouse-released [_]
                                              (vreset! value-changed-fn nil))
            ^ChangeListener value-changed-listener (fn value-changed-listener [_ _ new-value]
                                                     (when-not fx.lifecycle/*in-progress?*
                                                       (let [f @value-changed-fn]
                                                         (if f
                                                           (f new-value)
                                                           (do (.handle on-mouse-pressed nil)
                                                               (@value-changed-fn new-value))))))]
        (.addEventFilter slider MouseEvent/MOUSE_PRESSED on-mouse-pressed)
        (.addEventHandler slider MouseEvent/MOUSE_RELEASED on-mouse-released)
        (.addListener (.valueProperty slider) value-changed-listener)
        #(do
           (.removeEventFilter slider MouseEvent/MOUSE_PRESSED on-mouse-pressed)
           (.removeEventHandler slider MouseEvent/MOUSE_RELEASED on-mouse-released)
           (.removeListener (.valueProperty slider) value-changed-listener)
           (vreset! value-changed-fn nil))))
    fx.lifecycle/callback))

(defmethod make-control-view :slider [property _context localization-state]
  (let [old-num (coalesced-property->old-num property)
        is-float (math/float32? old-num)
        {:keys [min max precision]
         :or {min 0.0 max 1.0}} (:edit-type property)
        value (properties/unify-values (properties/values property))
        read-only (properties/read-only? property)]
    {:fx/type fxui/grid
     :spacing :small
     :column-constraints [{:fx/type fx.column-constraints/lifecycle :percent-width 20}
                          {:fx/type fx.column-constraints/lifecycle :percent-width 80}]
     :children
     [(-> {:fx/type fxui/value-field
           :grid-pane/column 0
           :pref-column-count (if precision (count (str precision)) 5)
           :to-string field-expression/format-number
           :to-value field-expression/to-double
           :value value
           :on-value-changed #(set-values! property (repeat (cond-> % is-float float)))
           :editable (not read-only)}
          (resolve-validation property localization-state))
      {:fx/type fx.slider/lifecycle
       :grid-pane/column 1
       :focus-traversable false
       :min min
       :max max
       :disable read-only
       :value (or value max)
       prop-slider-on-value-changed
       (fn make-on-value-changed []
         (let [op-seq (gensym)]
           (fn on-value-changed [new-value]
             (let [new-value (if precision
                               (math/round-with-precision new-value precision)
                               new-value)]
               (set-values! property (repeat (if is-float (float new-value) new-value)) op-seq)))))}]}))

(defmethod make-control-view :multi-line-text [property _context localization-state]
  (-> {:fx/type fxui/value-area
       :min-height 68.0
       :pref-height 68.0
       :on-value-changed #(set-values! property (repeat %))
       :editable (not (properties/read-only? property))}
      (resolve-value property)
      (resolve-validation property localization-state)))

(defmethod make-control-view :default [property _context localization-state]
  (-> {:fx/type fxui/value-field
       :to-string str
       :editable false}
      (resolve-value property)
      (resolve-validation property localization-state)))

(defn- focus-mouse-event-source! [^MouseEvent e]
  (.requestFocus ^Node (.getSource e)))

(def ^:private prop-mouse-pressed-handler
  (fx.prop/make
    (fx.mutator/adder-remover
      #(.addEventHandler ^Node %1 MouseEvent/MOUSE_PRESSED %2)
      #(.removeEventHandler ^Node %1 MouseEvent/MOUSE_PRESSED %2))
    fx.lifecycle/event-handler))

(def ^:private prop-property-context
  (fx.prop/make
    (fx.mutator/setter
      (fn [label [context selection-provider]]
        (ui/context! label :property context selection-provider)))
    fx.lifecycle/scalar))

(def ^:private prop-button-menu
  (fx.prop/make (fx.mutator/setter ui/register-button-menu) fx.lifecycle/scalar))

(defn- error-view [{:keys [^Throwable exception caught]}]
  (when caught (error-reporting/report-exception! exception))
  {:fx/type fxui/paragraph
   :color :error
   :text (or (.getMessage exception) (.getSimpleName (class exception)))})

(defn- handle-grid-key-pressed [^KeyEvent e]
  (when (= KeyCode/ESCAPE (.getCode e))
    (.requestFocus ^Node (.getSource e))
    (.consume e)))

(fxui/defc grid-view
  {:compose [{:fx/type fx/ext-watcher
              :ref (:localization (:context props))
              :key :localization-state}]}
  [{:keys [localization-state properties context]}]
  (let [properties (properties/coalesce properties)
        selection-provider (->SelectionProvider (:original-node-ids properties))]
    {:fx/type fxui/vertical
     :padding :small
     :spacing :small
     :children
     (->> properties
          category-property-edit-types
          (e/mapcat
            (fn [[category-title properties]]
              (when-not (coll/empty? properties)
                (-> []
                    (cond->
                      category-title
                      (conj {:fx/type fxui/label
                             :text (localization-state category-title)
                             :style-class "property-category"}))
                    (conj
                      {:fx/type fxui/grid
                       :on-key-pressed handle-grid-key-pressed
                       :column-constraints [{:fx/type fx.column-constraints/lifecycle}
                                            {:fx/type fx.column-constraints/lifecycle
                                             :hgrow :always}]
                       :spacing :small
                       :children
                       (coll/transfer properties []
                         (coll/mapcat-indexed
                           (fn [i [property-keyword property]]
                             (let [overridden (properties/overridden? property)]
                               (coll/pair
                                 {:fx/type fxui/menu-button
                                  :min-width :use-pref-size
                                  :style-class (cond-> ["property-label"] overridden (conj "overridden"))
                                  :grid-pane/column 0
                                  :grid-pane/row i
                                  :grid-pane/fill-width true
                                  :grid-pane/valignment :top
                                  :mnemonic-parsing false
                                  :focus-traversable false
                                  :text (localization-state (properties/label property))
                                  :tooltip (localization-state
                                             (localization/message
                                               "property.tooltip"
                                               {"help" (properties/tooltip property)
                                                "id" (string/replace (name property-keyword) \- \_)}))
                                  prop-button-menu ::property-menu
                                  prop-mouse-pressed-handler focus-mouse-event-source!
                                  prop-property-context [(assoc context :property property) selection-provider]}
                                 {:fx/type fxui/ext-error-boundary
                                  :fx/key property-keyword
                                  :grid-pane/column 1
                                  :grid-pane/row i
                                  :grid-pane/fill-width true
                                  :desc {:fx/type control-view
                                         :property property
                                         :context context
                                         :localization-state localization-state}
                                  :catch {:fx/type error-view}})))))})))))
          (into []))}))

(defn pane-view [{:keys [parent context selected-node-properties]}]
  {:fx/type fxui/ext-with-anchor-pane-props
   :desc {:fx/type fxui/ext-value :value parent}
   :props {:children [{:fx/type fxui/scroll
                       :anchor-pane/top 0
                       :anchor-pane/bottom 0
                       :anchor-pane/left 0
                       :anchor-pane/right 0
                       :content {:fx/type grid-view
                                 :context context
                                 :properties selected-node-properties}}]}})

(g/defnode PropertiesView
  (property parent-view Parent)
  (property prefs g/Any)

  (input workspace g/Any)
  (input localization g/Any)
  (input project g/Any)
  (input app-view g/NodeID)
  (input search-results-view g/NodeID)
  (input color-dropper-view g/NodeID)
  (input selected-node-properties g/Any)

  (output description g/Any :cached
          (g/fnk [parent-view workspace project app-view search-results-view selected-node-properties color-dropper-view prefs localization]
            {:fx/type fxui/ext-dedupe-identical-desc
             :desc {:fx/type pane-view
                    :parent parent-view
                    :context {:workspace workspace
                              :project project
                              :app-view app-view
                              :prefs prefs
                              :localization localization
                              :search-results-view search-results-view
                              :color-dropper-view color-dropper-view}
                    :selected-node-properties selected-node-properties}})))

(defn make-properties-view [workspace project app-view search-results-view view-graph color-dropper-view prefs ^Node parent]
  (let [properties-view (first
                          (g/tx-nodes-added
                            (g/transact
                              (g/make-nodes view-graph [view [PropertiesView :parent-view parent :prefs prefs]]
                                (g/connect workspace :_node-id view :workspace)
                                (g/connect workspace :localization view :localization)
                                (g/connect project :_node-id view :project)
                                (g/connect app-view :_node-id view :app-view)
                                (g/connect app-view :selected-node-properties view :selected-node-properties)
                                (g/connect search-results-view :_node-id view :search-results-view)
                                (g/connect color-dropper-view :_node-id view :color-dropper-view)))))]
    (ui/node-timer!
      parent 30 "refresh-properties-view"
      #(fxui/advance-ui-user-data-component! parent ::properties-view (g/node-value properties-view :description)))
    properties-view))
