;; Copyright 2020-2022 The Defold Foundation
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

(ns editor.fxui
  (:refer-clojure :exclude [partial])
  (:require [cljfx.api :as fx]
            [cljfx.coerce :as fx.coerce]
            [cljfx.component :as fx.component]
            [cljfx.fx.button :as fx.button]
            [cljfx.fx.column-constraints :as fx.column-constraints]
            [cljfx.fx.grid-pane :as fx.grid-pane]
            [cljfx.fx.label :as fx.label]
            [cljfx.fx.list-cell :as fx.list-cell]
            [cljfx.fx.stage :as fx.stage]
            [cljfx.fx.svg-path :as fx.svg-path]
            [cljfx.fx.text-area :as fx.text-area]
            [cljfx.fx.text-field :as fx.text-field]
            [cljfx.lifecycle :as fx.lifecycle]
            [cljfx.mutator :as fx.mutator]
            [cljfx.prop :as fx.prop]
            [editor.error-reporting :as error-reporting]
            [editor.ui :as ui]
            [editor.util :as eutil])
  (:import [clojure.lang Fn IFn IHashEq MultiFn]
           [com.defold.control ListCell]
           [java.util Collection]
           [javafx.application Platform]
           [javafx.collections ObservableList]
           [javafx.scene Node]
           [javafx.beans.property ReadOnlyProperty]
           [javafx.beans.value ChangeListener]
           [javafx.scene.control TextInputControl ListView ScrollPane]
           [javafx.util Callback]))

(set! *warn-on-reflection* true)

(def ext-value
  "Extension lifecycle that returns value on `:value` key"
  (reify fx.lifecycle/Lifecycle
    (create [_ desc _]
      (:value desc))
    (advance [_ _ desc _]
      (:value desc))
    (delete [_ _ _])))

(defn identity-aware-observable-list-mutator [get-list-fn]
  (let [set-all! #(.setAll ^ObservableList (get-list-fn %1) ^Collection %2)]
    (reify fx.mutator/Mutator
      (assign! [_ instance coerce value]
        (set-all! instance (coerce value)))
      (replace! [_ instance coerce old-value new-value]
        (when-not (identical? old-value new-value)
          (set-all! instance (coerce new-value))))
      (retract! [_ instance _ _]
        (set-all! instance [])))))

(extend-protocol fx.lifecycle/Lifecycle
  MultiFn
  (create [_ desc opts]
    (fx.lifecycle/create fx.lifecycle/dynamic-fn->dynamic desc opts))
  (advance [_ component desc opts]
    (fx.lifecycle/advance fx.lifecycle/dynamic-fn->dynamic component desc opts))
  (delete [_ component opts]
    (fx.lifecycle/delete fx.lifecycle/dynamic-fn->dynamic component opts)))

(defn focus-when-on-scene! [^Node node]
  (if (some? (.getScene node))
    (.requestFocus node)
    (.addListener (.sceneProperty node)
                  (reify ChangeListener
                    (changed [this _ _ new-scene]
                      (when (some? new-scene)
                        (.removeListener (.sceneProperty node) this)
                        (.requestFocus node)))))))

(defn ext-focused-by-default
  "Function component that mimics extension lifecycle. Focuses node specified by
   `:desc` key when it gets added to scene graph"
  [{:keys [desc]}]
  {:fx/type fx/ext-on-instance-lifecycle
   :on-created focus-when-on-scene!
   :desc desc})

(def ext-with-advance-events
  "Extension lifecycle that notifies all listeners even during advancing

  Expected keys:
  - `:desc` (required) - description of underlying component"
  (reify fx.lifecycle/Lifecycle
    (create [_ {:keys [desc]} opts]
      (binding [fx.lifecycle/*in-progress?* false]
        (fx.lifecycle/create fx.lifecycle/dynamic desc opts)))
    (advance [_ component {:keys [desc]} opts]
      (binding [fx.lifecycle/*in-progress?* false]
        (fx.lifecycle/advance fx.lifecycle/dynamic component desc opts)))
    (delete [_ component opts]
      (binding [fx.lifecycle/*in-progress?* false]
        (fx.lifecycle/delete fx.lifecycle/dynamic component opts)))))

(def ^:private ext-ensure-scroll-pane-child-visible-impl
  (fx/make-ext-with-props
    {:ensure-visible
     (fx.prop/make
       (fx.mutator/setter
         (fn [^ScrollPane pane ^Node child]
           (when child
             (let [content-height (-> pane .getContent .getBoundsInLocal .getHeight)
                   viewport-bounds (.getViewportBounds pane)
                   viewport-height (.getHeight viewport-bounds)
                   viewport-bottom (- (.getHeight viewport-bounds) (.getMinY viewport-bounds))
                   child-bounds (-> pane .getContent (.sceneToLocal (.localToScene child (.getBoundsInLocal child))))]
               (when (< viewport-height content-height)
                 (cond
                   ;; when child view is below viewport, scroll down
                   (< viewport-bottom (.getMaxY child-bounds))
                   (.setVvalue pane (/ (- (.getMaxY child-bounds) (.getHeight viewport-bounds))
                                       (- content-height viewport-height)))
                   ;; when child view is above viewport, scroll down
                   (< (.getMinY child-bounds) (- (.getMinY viewport-bounds)))
                   (.setVvalue pane (/ (.getMinY child-bounds)
                                       (- content-height viewport-height)))))))))
       fx.lifecycle/dynamic)}))

(defn ext-ensure-scroll-pane-child-visible
  "Extension lifecycle that ensures ScrollPane's child node is visible

  This extension will perform scroll whenever the child node is changed

  Expected props:

    :scroll-pane-desc    cljfx description (required) that resolves to
                         a ScrollPane instance
    :child-desc          cljfx description (optional) that resolves to a Node
                         that is also present in the ScrollPane's content. This
                         can be achieved using fx/ext-let-refs + fx/ext-get-ref"
  [{:keys [scroll-pane-desc child-desc]}]
  {:fx/type ext-ensure-scroll-pane-child-visible-impl
   :props (if (some? child-desc)
            {:ensure-visible child-desc}
            {})
   :desc scroll-pane-desc})

(defn make-event-filter-prop
  "Creates a prop-config that will add event filter for specified `event-type`

  Value for such prop in component description is expected to be an event
  handler (either EventHandler, function or event map)"
  [event-type]
  (fx.prop/make
    (fx.mutator/adder-remover #(.addEventFilter ^Node %1 event-type %2)
                              #(.removeEventFilter ^Node %1 event-type %2))
    (fx.lifecycle/wrap-coerce fx.lifecycle/event-handler
                              fx.coerce/event-handler)))

(defn make-event-handler-prop
  "Creates a prop-config that will add event handler for specified `event-type`

  Value for such prop in component description is expected to be an event
  handler (either EventHandler, function or event map)"
  [event-type]
  (fx.prop/make
    (fx.mutator/adder-remover #(.addEventHandler ^Node %1 event-type %2)
                              #(.removeEventHandler ^Node %1 event-type %2))
    (fx.lifecycle/wrap-coerce fx.lifecycle/event-handler
                              fx.coerce/event-handler)))

(def text-input-selection-prop
  "Prop-config that will set selection of a TextInputControl component

  Value for such prop in component description is expected to be a 2-element
  vector of anchor and caret indices"
  (fx.prop/make
    (reify fx.mutator/Mutator
      (assign! [_ instance coerce value]
        (let [[anchor caret] (coerce value)]
          (.selectRange ^TextInputControl instance anchor caret)))
      (replace! [_ instance coerce _ new-value]
        (let [[anchor caret] (coerce new-value)]
          (.selectRange ^TextInputControl instance anchor caret)))
      (retract! [_ _ _ _]))
    fx.lifecycle/scalar))

(def list-cell-factory-prop
  "Prop-config that provides simple list cell instead of text-field based one

  Value for such prop is a function of list view item that returns cell's prop
  map (without `:fx/type`)"
  (fx.prop/make
    (fx.mutator/setter #(.setCellFactory ^ListView %1 %2))
    (fx.lifecycle/detached-prop-map fx.list-cell/props)
    :coerce
    #(let [props-vol (volatile! {})]
       (reify Callback
         (call [_ _]
           (proxy [ListCell] []
             (updateItem [item empty]
               (let [^ListCell this this
                     props @props-vol]
                 (proxy-super updateItem item empty)
                 (vreset! props-vol (% props this item empty))))))))))

(def on-text-input-selection-changed-prop
  "Prop-config that will observe changes for selection in TextInputControl

  Value for such prop in component description is expected to be an event
  handler (either function or event map)"
  (fx.prop/make
    (reify fx.mutator/Mutator
      (assign! [_ instance coerce value]
        (let [[caret-listener anchor-listener] (coerce value)]
          (doto ^TextInputControl instance
            (-> .caretPositionProperty (.addListener ^ChangeListener caret-listener))
            (-> .anchorProperty (.addListener ^ChangeListener anchor-listener)))))
      (replace! [this instance coerce old-value new-value]
        (when-not (= old-value new-value)
          (fx.mutator/retract! this instance coerce old-value)
          (fx.mutator/assign! this instance coerce new-value)))
      (retract! [_ instance coerce value]
        (let [[caret-listener anchor-listener] (coerce value)]
          (doto ^TextInputControl instance
            (-> .caretPositionProperty (.removeListener ^ChangeListener caret-listener))
            (-> .anchorProperty (.removeListener ^ChangeListener anchor-listener))))))
    (fx.lifecycle/wrap-coerce
      fx.lifecycle/event-handler
      (fn [f]
        [(reify ChangeListener
           (changed [_ o _ new-caret]
             (f [(.getAnchor ^TextInputControl (.getBean ^ReadOnlyProperty o)) new-caret])))
         (reify ChangeListener
           (changed [_ o _ new-anchor]
             (f [new-anchor (.getCaretPosition ^TextInputControl (.getBean ^ReadOnlyProperty o))])))]))))

(defn wrap-dedupe-desc
  "Renderer middleware that skips advancing if new description is the same"
  [lifecycle]
  (reify fx.lifecycle/Lifecycle
    (create [_ desc opts]
      (with-meta
        {:desc desc
         :child (fx.lifecycle/create lifecycle desc opts)}
        {`fx.component/instance #(-> % :child fx.component/instance)}))
    (advance [_ component desc opts]
      (if (= desc (:desc component))
        component
        (-> component
            (assoc :desc desc)
            (update :child #(fx.lifecycle/advance lifecycle % desc opts)))))
    (delete [_ component opts]
      (fx.lifecycle/delete lifecycle (:child component) opts))))

(defmacro provide-single-default [m k v]
  `(let [m# ~m
         k# ~k]
     (if (contains? m# k#)
       m#
       (assoc m# k# ~v))))

(defmacro provide-defaults
  "Like assoc, but does nothing if key is already in this map. Evaluates values
  only when key is not present"
  [m & kvs]
  `(-> ~m
       ~@(map #(cons `provide-single-default %) (partition 2 kvs))))

(defn mount-renderer-and-await-result!
  "Mounts `renderer` and blocks current thread until `state-atom`'s value
  receives a `::result` key"
  [state-atom renderer]
  (let [event-loop-key (Object.)
        result-promise (promise)]
    (future
      (error-reporting/catch-all!
        (let [result @result-promise]
          (fx/on-fx-thread
            (Platform/exitNestedEventLoop event-loop-key result)))))
    (add-watch state-atom event-loop-key
               (fn [k r _ n]
                 (let [result (::result n ::no-result)]
                   (when-not (= result ::no-result)
                     (deliver result-promise result)
                     (remove-watch r k)))))
    (fx/mount-renderer state-atom renderer)
    (Platform/enterNestedEventLoop event-loop-key)))

(defn dialog-showing? [props]
  (not (contains? props ::result)))

(defn show-dialog-and-await-result!
  "Creates a dialog, shows it and block current thread until dialog has a result
  (which is checked by presence of a `:result` key in state map)

  Options:
  - `:initial-state` (optional, default `{}`) - map containing initial state of
    a dialog, should not contain `::result` key to be shown
  - `:event-handler` (required) - 2-argument event handler, receives current
    state as first argument and event map as second, returns new state. Once
    state of a dialog has `::result` key in it, dialog interaction is considered
    complete and dialog will close
  - `:description` (required) - fx description used for this dialog, gets merged
    into current state map, meaning that state map contents, including
    eventually a `::result` key, will also be present in description props. You
    can use `editor.fxui/dialog-showing?` and pass it resulting props to check
    if dialog stage's `:showing` property should be set to true"
  [& {:keys [initial-state event-handler description]
      :or {initial-state {}}}]
  (let [state-atom (atom initial-state)
        renderer (fx/create-renderer
                   :error-handler error-reporting/report-exception!
                   :opts {:fx.opt/map-event-handler #(swap! state-atom event-handler %)}
                   :middleware (fx/wrap-map-desc merge description))]
    (mount-renderer-and-await-result! state-atom renderer)))

(defn stage
  "Generic `:stage` that mirrors behavior of `editor.ui/make-stage`"
  [props]
  (assoc props
    :fx/type fx.stage/lifecycle
    :on-focused-changed ui/focus-change-listener
    :icons (if (eutil/is-mac-os?) [] [ui/application-icon-image])))

(defn dialog-stage
  "Generic dialog `:stage` that mirrors behavior of `editor.ui/make-dialog-stage`"
  [props]
  (let [owner (:owner props ::no-owner)]
    (-> props
        (assoc :fx/type stage)
        (assoc :owner (cond
                        (= owner ::no-owner)
                        {:fx/type fx/ext-instance-factory
                         :create ui/main-stage}

                        (:fx/type owner)
                        owner

                        :else
                        {:fx/type ext-value
                         :value owner}))
        (provide-defaults
          :resizable false
          :style :decorated
          :modality (if (nil? owner) :application-modal :window-modal)))))

(defn add-style-classes [props & classes]
  (update props :style-class (fn [style-class]
                               (let [existing-classes (cond
                                                        (nil? style-class) []
                                                        (string? style-class) [style-class]
                                                        :else style-class)]
                                 (into existing-classes classes)))))

(defn label
  "Generic `:label` with sensible defaults (`:wrap-text` is true)

  Additional keys:
  - `:variant` (optional, default `:label`) - a styling variant, either `:label`
     or `:header`"
  [{:keys [variant]
    :or {variant :label}
    :as props}]
  (-> props
      (assoc :fx/type fx.label/lifecycle)
      (dissoc :variant)
      (provide-defaults :wrap-text true)
      (add-style-classes (case variant
                           :label "label"
                           :header "header"))))

(defn button
  "Generic button

  Additional keys:
  - `:variant` (optional, default `:secondary`) - a styling variant, either
    `:secondary`, `:primary`, `:icon` or `:danger`"
  [{:keys [variant]
    :or {variant :secondary}
    :as props}]
  (-> props
      (assoc :fx/type fx.button/lifecycle)
      (dissoc :variant)
      (add-style-classes "button" (case variant
                                    :primary "button-primary"
                                    :secondary "button-secondary"
                                    :danger "button-danger"
                                    :icon "button-icon"))))

(defn two-col-input-grid-pane
  "Grid pane whose children are partitioned into pairs and displayed in 2
  columns, useful for multiple label + input fields"
  [props]
  (-> props
      (assoc :fx/type fx.grid-pane/lifecycle
             :column-constraints [{:fx/type fx.column-constraints/lifecycle}
                                  {:fx/type fx.column-constraints/lifecycle
                                   :hgrow :always}])
      (add-style-classes "input-grid-pane")
      (update :children (fn [children]
                          (into []
                                (comp
                                  (partition-all 2)
                                  (map-indexed
                                    (fn [row [label input]]
                                      [(assoc label :grid-pane/column 0
                                                    :grid-pane/row row
                                                    :grid-pane/halignment :right)
                                       (assoc input :grid-pane/column 1
                                                    :grid-pane/row row)]))
                                  cat)
                                children)))))

(defn text-field
  "Generic `:text-field`

  Additional keys:
  - `:variant` (optional, default `:default`) - a styling variant, either
    `:default` or `:error`"
  [{:keys [variant]
    :or {variant :default}
    :as props}]
  (-> props
      (assoc :fx/type fx.text-field/lifecycle)
      (dissoc :variant)
      (add-style-classes "text-field" (case variant
                                        :default "text-field-default"
                                        :error "text-field-error"))))

(defn text-area
  "Generic `:text-area`

  Additional keys:
  - `:variant` (optional, default `:default`) - a styling variant, either
    `:default`, `:error` or `:borderless`"
  [{:keys [variant]
    :or {variant :default}
    :as props}]
  (-> props
      (assoc :fx/type fx.text-area/lifecycle)
      (dissoc :variant)
      (add-style-classes "text-area" (case variant
                                       :default "text-area-default"
                                       :error "text-area-error"
                                       :borderless "text-area-borderless"))))

(defn icon
  "An `:svg-path` with content being managed by `:type` key

  Additional keys:
  - `:type` (required) - icon type, see :icon/* keywords"
  [{:keys [type] :as props}]
  (-> props
      (assoc :fx/type fx.svg-path/lifecycle
             :content (case type
                        :icon/android "M28.5,11c-0.6,0-1.1,0.2-1.5,0.5v0c0-3.4-1.8-6.3-4.4-8l1.1-2.2c0.1-0.2,0-0.5-0.2-0.7c-0.2-0.1-0.5,0-0.7,0.2L21.7,3c-1.3-0.6-2.7-1-4.2-1s-2.9,0.4-4.2,1l-1.1-2.1c-0.1-0.2-0.4-0.3-0.7-0.2c-0.2,0.1-0.3,0.4-0.2,0.7l1.1,2.2C9.8,5.2,8,8.1,8,11.5v0C7.6,11.2,7.1,11,6.5,11C5.1,11,4,12.1,4,13.5v8C4,22.9,5.1,24,6.5,24c0.6,0,1.1-0.2,1.5-0.5v1c0,1.4,1.1,2.5,2.5,2.5H11v4.5c0,1.4,1.1,2.5,2.5,2.5s2.5-1.1,2.5-2.5V27h3v4.6c0,1.3,1.1,2.4,2.4,2.4h0.3c1.3,0,2.4-1.1,2.4-2.4V27h0.5c1.4,0,2.5-1.1,2.5-2.5v-1c0.4,0.3,0.9,0.5,1.5,0.5c1.4,0,2.5-1.1,2.5-2.5v-8C31,12.1,29.9,11,28.5,11zM17.5,3c4.5,0,8.2,3.5,8.5,8H9C9.3,6.5,13,3,17.5,3zM6.5,23C5.7,23,5,22.3,5,21.5v-8C5,12.7,5.7,12,6.5,12S8,12.7,8,13.5v8C8,22.3,7.3,23,6.5,23zM26,24.5c0,0.8-0.7,1.5-1.5,1.5H23v5.6c0,0.8-0.6,1.4-1.4,1.4h-0.3c-0.8,0-1.4-0.6-1.4-1.4V26h-5v5.5c0,0.8-0.7,1.5-1.5,1.5S12,32.3,12,31.5V26h-1.5C9.7,26,9,25.3,9,24.5v-3v-8V12h17v1.5v8V24.5zM30,21.5c0,0.8-0.7,1.5-1.5,1.5S27,22.3,27,21.5v-8c0-0.8,0.7-1.5,1.5-1.5s1.5,0.7,1.5,1.5V21.5zM12.5,7.3c0-0.5,0.4-0.8,0.8-0.8c0.5,0,0.9,0.4,0.9,0.8s-0.4,0.9-0.9,0.9C12.9,8.2,12.5,7.8,12.5,7.3zM20.8,7.3c0-0.5,0.4-0.8,0.9-0.8c0.5,0,0.8,0.4,0.8,0.8s-0.4,0.9-0.8,0.9C21.2,8.2,20.8,7.8,20.8,7.3z"
                        :icon/apple "M16.5,8.6l0.4,0c0.1,0,0.2,0,0.4,0c1.4,0,2.6-0.5,3.8-1.5c1.6-1.3,2.5-3,2.9-5C24,1.6,24,1.1,24,0.5l0-0.5h-0.5c-0.1,0-0.4,0-0.5,0.1c-1.1,0.2-2.2,0.6-3.1,1.2c-1.8,1.2-2.9,2.9-3.4,5c-0.1,0.6-0.2,1.2-0.1,1.9L16.5,8.6zM17.4,6.5c0.4-1.9,1.4-3.4,3-4.4c0.8-0.5,1.6-0.9,2.6-1c0,0.3,0,0.6-0.1,0.9c-0.3,1.8-1.1,3.3-2.5,4.4c-1,0.8-2,1.2-3.1,1.2C17.3,7.2,17.4,6.9,17.4,6.5zM30.6,24.3c-2.6-1.3-4-3.2-4.2-5.8c-0.2-2.6,0.9-4.6,3.3-6.3l0.3-0.3l-0.2-0.4c0-0.1-0.1-0.1-0.1-0.2c-0.8-1-1.8-1.8-2.9-2.4c-1.6-0.8-3.2-1-4.9-0.8c-1.2,0.2-2.3,0.6-3.4,1.1c-1,0.4-1.7,0.4-2.4,0.1c-0.6-0.3-1.2-0.5-1.8-0.7c-0.7-0.3-1.5-0.5-2.3-0.5c-2.7,0-4.9,1-6.7,3.2c-1.2,1.5-2,3.3-2.2,5.5c-0.3,2.5,0.1,5.1,1,8c0.6,1.7,1.3,3.2,2.2,4.6c0.8,1.2,1.7,2.6,3,3.6c0.9,0.7,1.8,1.1,2.7,1.1c0.1,0,0.2,0,0.4,0c0.7-0.1,1.5-0.3,2.4-0.7c1.8-0.8,3.4-0.8,5.1-0.1l0.2,0.1c0.3,0.1,0.7,0.3,1,0.4c1.5,0.5,2.8,0.4,4-0.3c0.7-0.4,1.3-1,1.9-1.7c0.8-1,1.9-2.5,2.7-4.1c0.3-0.6,0.5-1.2,0.8-1.8l0.5-1.2L30.6,24.3zM29.6,25.4c-0.2,0.6-0.5,1.2-0.8,1.7c-0.7,1.5-1.7,2.9-2.5,3.9c-0.5,0.7-1.1,1.2-1.7,1.5c-0.9,0.5-1.9,0.6-3.2,0.2c-0.3-0.1-0.6-0.2-0.9-0.4l-0.2-0.1c-1.9-0.8-3.9-0.8-5.9,0.1c-0.8,0.4-1.5,0.6-2.1,0.6c-0.8,0.1-1.6-0.2-2.4-0.9c-1.2-1-2-2.3-2.8-3.4c-0.8-1.3-1.5-2.7-2.1-4.3c-0.9-2.7-1.2-5.1-1-7.5c0.2-2,0.9-3.6,2-5c1.5-1.9,3.5-2.8,5.8-2.8c0,0,0,0,0.1,0c0.7,0,1.4,0.2,2,0.4c0.6,0.2,1.2,0.4,1.8,0.7c1.3,0.5,2.4,0.2,3.3-0.1c1.1-0.4,2-0.8,3.1-1c1.5-0.2,2.9,0,4.4,0.7c0.9,0.4,1.7,1,2.4,1.9c-2.4,1.8-3.5,4.2-3.3,6.9c0.2,2.8,1.6,5,4.3,6.4L29.6,25.4z"
                        :icon/archive "M32,9.2c0-0.6-0.2-1.2-0.7-1.6l-5.9-5.9C25,1.2,24.4,1,23.8,1L4.3,1C3.6,1,3,1.6,3,2.3l0,30.4C3,33.4,3.6,34,4.3,34l26.4,0c0.7,0,1.3-0.6,1.3-1.3L32,9.2zM30.7,33L4.3,33C4.1,33,4,32.9,4,32.7L4,2.3C4,2.1,4.1,2,4.3,2l19.5,0c0.3,0,0.7,0.1,0.9,0.4l5.9,5.9C30.8,8.5,31,8.9,31,9.2l0,23.5C31,32.9,30.9,33,30.7,33zM29.5,9.5c0,0.3-0.2,0.5-0.5,0.5l-4.9,0C23.5,10,23,9.5,23,8.9L23,4c0-0.3,0.2-0.5,0.5-0.5S24,3.7,24,4l0,4.9C24,9,24,9,24.1,9L29,9C29.3,9,29.5,9.2,29.5,9.5zM18,13.5c0,0.3-0.2,0.5-0.5,0.5S17,13.8,17,13.5s0.2-0.5,0.5-0.5S18,13.2,18,13.5zM18,11.5c0,0.3-0.2,0.5-0.5,0.5S17,11.8,17,11.5s0.2-0.5,0.5-0.5S18,11.2,18,11.5zM18,9.5c0,0.3-0.2,0.5-0.5,0.5S17,9.8,17,9.5C17,9.2,17.2,9,17.5,9S18,9.2,18,9.5zM18,7.5c0,0.3-0.2,0.5-0.5,0.5S17,7.8,17,7.5S17.2,7,17.5,7S18,7.2,18,7.5zM18,5.5c0,0.3-0.2,0.5-0.5,0.5S17,5.8,17,5.5C17,5.2,17.2,5,17.5,5S18,5.2,18,5.5zM18,3.5c0,0.3-0.2,0.5-0.5,0.5S17,3.8,17,3.5S17.2,3,17.5,3S18,3.2,18,3.5zM15.9,12.6c0-0.3,0.2-0.5,0.5-0.5s0.5,0.2,0.5,0.5s-0.2,0.5-0.5,0.5S15.9,12.9,15.9,12.6zM15.9,10.6c0-0.3,0.2-0.5,0.5-0.5s0.5,0.2,0.5,0.5s-0.2,0.5-0.5,0.5S15.9,10.9,15.9,10.6zM15.9,8.6c0-0.3,0.2-0.5,0.5-0.5S17,8.2,17,8.6c0,0.3-0.2,0.5-0.5,0.5S15.9,8.9,15.9,8.6zM18,17.5c0,0.3-0.2,0.5-0.5,0.5S17,17.8,17,17.5s0.2-0.5,0.5-0.5S18,17.2,18,17.5zM18,15.5c0,0.3-0.2,0.5-0.5,0.5S17,15.8,17,15.5c0-0.3,0.2-0.5,0.5-0.5S18,15.2,18,15.5zM15.9,16.6c0-0.3,0.2-0.5,0.5-0.5s0.5,0.2,0.5,0.5s-0.2,0.5-0.5,0.5S15.9,16.9,15.9,16.6zM15.9,14.5c0-0.3,0.2-0.5,0.5-0.5s0.5,0.2,0.5,0.5c0,0.3-0.2,0.5-0.5,0.5S15.9,14.9,15.9,14.5zM15.9,6.6c0-0.3,0.2-0.5,0.5-0.5S17,6.2,17,6.6s-0.2,0.5-0.5,0.5S15.9,6.9,15.9,6.6zM15.9,4.6c0-0.3,0.2-0.5,0.5-0.5S17,4.2,17,4.6c0,0.3-0.2,0.5-0.5,0.5S15.9,4.9,15.9,4.6zM18,19.5v2.9c0,0.3-0.2,0.5-0.5,0.5h-0.9c-0.3,0-0.5-0.2-0.5-0.5v-2.9c0-0.3,0.2-0.5,0.5-0.5h0.9C17.8,19,18,19.2,18,19.5z"
                        :icon/circle-check "M17.5,0C7.8,0,0,7.8,0,17.5S7.8,35,17.5,35S35,27.2,35,17.5S27.2,0,17.5,0zM17.5,34C8.4,34,1,26.6,1,17.5S8.4,1,17.5,1S34,8.4,34,17.5S26.6,34,17.5,34zM26,13c0.2,0.2,0.2,0.5,0,0.7L15.8,23.9C15.7,24,15.6,24,15.5,24s-0.3-0.1-0.4-0.1l-6-6c-0.2-0.2-0.2-0.5,0-0.7s0.5-0.2,0.7,0l5.6,5.6l9.8-9.8C25.5,12.8,25.8,12.8,26,13z"
                        :icon/circle-happy "M17.5,0C7.8,0,0,7.8,0,17.5S7.8,35,17.5,35S35,27.2,35,17.5S27.2,0,17.5,0zM17.5,34C8.4,34,1,26.6,1,17.5S8.4,1,17.5,1S34,8.4,34,17.5S26.6,34,17.5,34zM22,13c-0.9,0-1.7,0.6-2,1.4c-0.1,0.3,0.1,0.5,0.3,0.6c0.3,0.1,0.5-0.1,0.6-0.3c0.1-0.4,0.5-0.6,1-0.6s0.9,0.3,1,0.6c0.1,0.2,0.3,0.4,0.5,0.4c0,0,0.1,0,0.1,0c0.3-0.1,0.4-0.4,0.3-0.6C23.7,13.6,22.9,13,22,13zM13,13c-0.9,0-1.7,0.6-2,1.4c-0.1,0.3,0.1,0.5,0.3,0.6c0.3,0.1,0.5-0.1,0.6-0.3c0.1-0.4,0.5-0.6,1-0.6s0.9,0.3,1,0.6c0.1,0.2,0.3,0.4,0.5,0.4c0,0,0.1,0,0.1,0c0.3-0.1,0.4-0.4,0.3-0.6C14.7,13.6,13.9,13,13,13zM19.5,18c-0.5,0-3.5,0-4,0c-0.3,0-0.6,0.5-0.5,0.8c0.3,1.3,1.3,2.2,2.5,2.2s2.2-0.9,2.5-2.2C20.1,18.5,19.8,18,19.5,18z"
                        :icon/circle-info "M17.5,0C7.8,0,0,7.8,0,17.5S7.8,35,17.5,35S35,27.2,35,17.5S27.2,0,17.5,0zM17.5,34C8.4,34,1,26.6,1,17.5S8.4,1,17.5,1S34,8.4,34,17.5S26.6,34,17.5,34zM18.4,10.5c0,0.5-0.4,0.9-0.9,0.9c-0.5,0-0.9-0.4-0.9-0.9c0-0.5,0.4-0.9,0.9-0.9C18,9.6,18.4,10,18.4,10.5zM18,14.5v11c0,0.3-0.2,0.5-0.5,0.5c-0.3,0-0.5-0.2-0.5-0.5v-11c0-0.3,0.2-0.5,0.5-0.5C17.8,14,18,14.2,18,14.5z"
                        :icon/circle-plus "M17.5,0C7.8,0,0,7.8,0,17.5S7.8,35,17.5,35S35,27.2,35,17.5S27.2,0,17.5,0zM17.5,34C8.4,34,1,26.6,1,17.5S8.4,1,17.5,1S34,8.4,34,17.5S26.6,34,17.5,34zM26.5,17.5c0,0.3-0.2,0.5-0.5,0.5h-8v8c0,0.3-0.2,0.5-0.5,0.5S17,26.3,17,26v-8H9c-0.3,0-0.5-0.2-0.5-0.5S8.7,17,9,17h8V9c0-0.3,0.2-0.5,0.5-0.5S18,8.7,18,9v8h8C26.3,17,26.5,17.2,26.5,17.5z"
                        :icon/circle-question "M17.5,0C7.8,0,0,7.8,0,17.5S7.8,35,17.5,35S35,27.2,35,17.5S27.2,0,17.5,0zM17.5,34C8.4,34,1,26.6,1,17.5S8.4,1,17.5,1S34,8.4,34,17.5S26.6,34,17.5,34zM18,24.5c0,0.3-0.2,0.5-0.5,0.5S17,24.8,17,24.5c0-0.3,0.2-0.5,0.5-0.5S18,24.2,18,24.5zM21.9,13.4c0.3,1.2,0.1,2.5-0.6,3.5c-0.7,1-1.8,1.7-3,1.9c-0.2,0-0.3,0.2-0.3,0.3v2.3c0,0.3-0.2,0.5-0.5,0.5S17,21.8,17,21.5v-2.3c0-0.6,0.5-1.2,1.1-1.3c0.9-0.2,1.8-0.7,2.3-1.5c0.5-0.8,0.7-1.8,0.5-2.8c-0.3-1.3-1.3-2.3-2.6-2.6c-1.1-0.2-2.2,0-3,0.7c-0.8,0.7-1.3,1.7-1.3,2.7c0,0.3-0.2,0.5-0.5,0.5S13,14.8,13,14.5c0-1.4,0.6-2.7,1.7-3.5c1.1-0.9,2.5-1.2,3.9-0.9C20.2,10.5,21.5,11.8,21.9,13.4z"
                        :icon/circle-sad "M17.5,0C7.8,0,0,7.8,0,17.5S7.8,35,17.5,35S35,27.2,35,17.5S27.2,0,17.5,0zM17.5,34C8.4,34,1,26.6,1,17.5S8.4,1,17.5,1S34,8.4,34,17.5S26.6,34,17.5,34zM19.6,26.5c-0.3,0.1-0.6-0.1-0.6-0.4c-0.2-0.7-0.8-1.1-1.5-1.1s-1.3,0.5-1.5,1.1c-0.1,0.3-0.3,0.4-0.6,0.4c-0.3-0.1-0.4-0.3-0.4-0.6c0.3-1.1,1.3-1.9,2.5-1.9s2.2,0.8,2.5,1.9C20.1,26.1,19.9,26.4,19.6,26.5zM27,19.1c-0.3,1.1-1.3,1.9-2.5,1.9s-2.2-0.8-2.5-1.9c-0.1-0.3,0.1-0.5,0.4-0.6c0.3-0.1,0.6,0.1,0.6,0.4c0.2,0.7,0.8,1.1,1.5,1.1c0.7,0,1.3-0.5,1.5-1.1c0.1-0.2,0.3-0.4,0.5-0.4c0,0,0.1,0,0.1,0C26.9,18.6,27.1,18.9,27,19.1zM13,19.1c-0.3,1.1-1.3,1.9-2.5,1.9S8.3,20.2,8,19.1c-0.1-0.3,0.1-0.5,0.4-0.6c0.3-0.1,0.6,0.1,0.6,0.4c0.2,0.7,0.8,1.1,1.5,1.1c0.7,0,1.3-0.5,1.5-1.1c0.1-0.2,0.3-0.4,0.5-0.4c0,0,0.1,0,0.1,0C12.9,18.6,13.1,18.9,13,19.1z"
                        :icon/circle-x "M17.5,0C7.8,0,0,7.8,0,17.5S7.8,35,17.5,35S35,27.2,35,17.5S27.2,0,17.5,0zM17.5,34C8.4,34,1,26.6,1,17.5S8.4,1,17.5,1S34,8.4,34,17.5S26.6,34,17.5,34zM24.1,11.6l-5.9,5.9l5.9,5.9c0.2,0.2,0.2,0.5,0,0.7c-0.1,0.1-0.2,0.1-0.4,0.1s-0.3,0-0.4-0.1l-5.9-5.9l-5.9,5.9c-0.1,0.1-0.2,0.1-0.4,0.1s-0.3,0-0.4-0.1c-0.2-0.2-0.2-0.5,0-0.7l5.9-5.9l-5.9-5.9c-0.2-0.2-0.2-0.5,0-0.7s0.5-0.2,0.7,0l5.9,5.9l5.9-5.9c0.2-0.2,0.5-0.2,0.7,0S24.3,11.4,24.1,11.6z"
                        :icon/clone "M29.5,9.5c0,0.3-0.2,0.5-0.5,0.5l-4.9,0C23.5,10,23,9.5,23,8.9L23,4c0-0.3,0.2-0.5,0.5-0.5S24,3.7,24,4l0,4.9C24,9,24,9,24.1,9L29,9C29.3,9,29.5,9.2,29.5,9.5zM32,28.7c0,0.7-0.6,1.3-1.3,1.3L8.3,30C7.6,30,7,29.4,7,28.7L7,2.3C7,1.6,7.6,1,8.3,1l15.5,0c0.6,0,1.2,0.2,1.6,0.7l5.9,5.9C31.7,8,32,8.6,32,9.2L32,28.7zM31,28.7l0-19.5c0-0.3-0.1-0.7-0.4-0.9l-5.9-5.9C24.5,2.1,24.1,2,23.8,2L8.3,2C8.1,2,8,2.1,8,2.3l0,26.4C8,28.9,8.1,29,8.3,29l22.4,0C30.9,29,31,28.9,31,28.7zM27.5,31c-0.3,0-0.5,0.2-0.5,0.5l0,1.2c0,0.2-0.1,0.3-0.3,0.3L4.3,33C4.1,33,4,32.9,4,32.7L4,6.3C4,6.1,4.1,6,4.3,6l1.2,0C5.8,6,6,5.8,6,5.5C6,5.2,5.8,5,5.5,5L4.3,5C3.6,5,3,5.6,3,6.3l0,26.4C3,33.4,3.6,34,4.3,34l22.4,0c0.7,0,1.3-0.6,1.3-1.3l0-1.2C28,31.2,27.8,31,27.5,31zM22.5,17H20v-2.5c0-0.3-0.2-0.5-0.5-0.5S19,14.2,19,14.5V17h-2.5c-0.3,0-0.5,0.2-0.5,0.5s0.2,0.5,0.5,0.5H19v2.5c0,0.3,0.2,0.5,0.5,0.5s0.5-0.2,0.5-0.5V18h2.5c0.3,0,0.5-0.2,0.5-0.5S22.8,17,22.5,17z"
                        :icon/cross "M30.1,29.6c0.2,0.2,0.2,0.5,0,0.7c-0.1,0.1-0.2,0.1-0.4,0.1s-0.3,0-0.4-0.1L17.2,18.2L5.1,30.4c-0.1,0.1-0.2,0.1-0.4,0.1s-0.3,0-0.4-0.1c-0.2-0.2-0.2-0.5,0-0.7l12.1-12.1L4.4,5.4c-0.2-0.2-0.2-0.5,0-0.7s0.5-0.2,0.7,0l12.1,12.1L29.4,4.6c0.2-0.2,0.5-0.2,0.7,0s0.2,0.5,0,0.7L18,17.5L30.1,29.6z"
                        :icon/download "M34,25v8.5c0,0.3-0.2,0.5-0.5,0.5h-32C1.2,34,1,33.8,1,33.5V25c0-0.3,0.2-0.5,0.5-0.5S2,24.7,2,25v8h31v-8c0-0.3,0.2-0.5,0.5-0.5S34,24.7,34,25zM27,17.7l-9,9V2c0-0.3-0.2-0.5-0.5-0.5S17,1.7,17,2v24.8l-9-9c-0.1-0.1-0.2-0.1-0.4-0.1s-0.3,0-0.4,0.1c-0.2,0.2-0.2,0.5,0,0.7l9.9,9.9c0,0,0.1,0.1,0.2,0.1c0.1,0.1,0.3,0.1,0.4,0c0.1,0,0.1-0.1,0.2-0.1l9.9-9.9c0.2-0.2,0.2-0.5,0-0.7S27.2,17.6,27,17.7z"
                        :icon/facebook "M26.1,0.2C25.7,0.2,23.8,0,21.5,0c-2.5,0-4.6,0.8-6.1,2.3C13.8,3.9,13,6.2,13,8.9V12H9H8v1v6v1h1h4v14v1h1h6h1v-1V20h4.1h0.9l0.1-0.9l0.8-6l0.1-1.1h-1.1H21V9.4c0-2,0.8-2.4,1.9-2.4L26,7l1,0V6V1.2V0.4L26.1,0.2zM26,6l-3.1,0C20.5,6,20,7.8,20,9.4V13h5.8l-0.8,6H20v15h-6V19H9v-6h5V8.9c0-5,3-7.9,7.5-7.9c2.1,0,4,0.2,4.5,0.2V6z"
                        :icon/file-check "M28,31.5l0,1.2c0,0.7-0.6,1.3-1.3,1.3L4.3,34C3.6,34,3,33.4,3,32.7L3,6.3C3,5.6,3.6,5,4.3,5l1.2,0C5.7,5,6,5.2,6,5.5C6,5.8,5.8,6,5.5,6L4.3,6C4.1,6,4,6.1,4,6.3l0,26.4C4,32.9,4.1,33,4.3,33l22.4,0c0.2,0,0.3-0.1,0.3-0.3l0-1.2c0-0.3,0.2-0.5,0.5-0.5C27.8,31,28,31.2,28,31.5zM23.1,15.1l-4.6,4.6l-2.6-2.6c-0.2-0.2-0.5-0.2-0.7,0s-0.2,0.5,0,0.7l3,3c0.1,0.1,0.2,0.1,0.4,0.1s0.3,0,0.4-0.1l5-5c0.2-0.2,0.2-0.5,0-0.7S23.3,15,23.1,15.1zM32,28.7c0,0.7-0.6,1.3-1.3,1.3L8.3,30C7.6,30,7,29.4,7,28.7L7,2.3C7,1.6,7.6,1,8.3,1l15.5,0c0.6,0,1.2,0.2,1.6,0.7l5.9,5.9C31.7,8,32,8.6,32,9.2L32,28.7zM31,28.7l0-19.5c0-0.3-0.1-0.7-0.4-0.9l-5.9-5.9C24.5,2.1,24.1,2,23.8,2L8.3,2C8.1,2,8,2.1,8,2.3l0,26.4C8,28.9,8.1,29,8.3,29l22.4,0C30.9,29,31,28.9,31,28.7zM29,9l-4.9,0C24,9,24,9,24,8.9L24,4c0-0.3-0.2-0.5-0.5-0.5S23,3.7,23,4l0,4.9c0,0.6,0.5,1.1,1.1,1.1l4.9,0c0.3,0,0.5-0.2,0.5-0.5C29.5,9.2,29.3,9,29,9z"
                        :icon/file-edit "M8,8.5C8,8.2,8.2,8,8.5,8h11C19.8,8,20,8.2,20,8.5S19.8,9,19.5,9h-11C8.2,9,8,8.8,8,8.5zM8.5,14h7c0.3,0,0.5-0.2,0.5-0.5S15.8,13,15.5,13h-7C8.2,13,8,13.2,8,13.5S8.2,14,8.5,14zM27,13.5c0-0.3-0.2-0.5-0.5-0.5h-8c-0.3,0-0.5,0.2-0.5,0.5s0.2,0.5,0.5,0.5h8C26.8,14,27,13.8,27,13.5zM24,18.5c0-0.3-0.2-0.5-0.5-0.5h-15C8.2,18,8,18.2,8,18.5S8.2,19,8.5,19h15C23.8,19,24,18.8,24,18.5zM8.5,24h2c0.3,0,0.5-0.2,0.5-0.5S10.8,23,10.5,23h-2C8.2,23,8,23.2,8,23.5S8.2,24,8.5,24zM18.5,24c0.3,0,0.5-0.2,0.5-0.5S18.8,23,18.5,23h-5c-0.3,0-0.5,0.2-0.5,0.5s0.2,0.5,0.5,0.5H18.5zM8.5,29h11c0.3,0,0.5-0.2,0.5-0.5S19.8,28,19.5,28h-11C8.2,28,8,28.2,8,28.5S8.2,29,8.5,29zM33.9,20.9l-8,8c-0.1,0.1-0.2,0.1-0.4,0.1h-3c-0.3,0-0.5-0.2-0.5-0.5v-3c0-0.1,0.1-0.3,0.1-0.4l8-8c0.2-0.2,0.5-0.2,0.7,0l3,3C34,20.3,34,20.7,33.9,20.9zM30.8,22.5l-2.3-2.3l-5,5l2.3,2.3L30.8,22.5zM24.8,28L23,26.2v1.1l0.7,0.7H24.8zM32.8,20.5l-2.3-2.3l-1.3,1.3l2.3,2.3L32.8,20.5zM29.5,9.5C29.5,9.2,29.3,9,29,9l-4.9,0C24,9,24,9,24,8.9L24,4c0-0.3-0.2-0.5-0.5-0.5S23,3.7,23,4l0,4.9c0,0.6,0.5,1.1,1.1,1.1l4.9,0C29.3,10,29.5,9.8,29.5,9.5zM32,25.5c0-0.3-0.2-0.5-0.5-0.5S31,25.2,31,25.5v5l0,0v2.2c0,0.1-0.1,0.2-0.2,0.2H4.3C4.1,33,4,32.9,4,32.8V2.2C4,2.1,4.1,2,4.3,2h19.6c0.3,0,0.5,0.1,0.7,0.3l6.1,6.1C30.9,8.6,31,8.9,31,9.1v3.4l0,0v3c0,0.3,0.2,0.5,0.5,0.5s0.5-0.2,0.5-0.5V14h0V9.1c0-0.5-0.2-1-0.6-1.4l-6.1-6.1C24.9,1.2,24.4,1,23.9,1H4.3C3.6,1,3,1.6,3,2.2v30.5C3,33.4,3.6,34,4.3,34h26.5c0.7,0,1.2-0.6,1.2-1.2v-3.2h0V25.5z"
                        :icon/file-generic "M8,8.5C8,8.2,8.2,8,8.5,8h11C19.8,8,20,8.2,20,8.5S19.8,9,19.5,9h-11C8.2,9,8,8.8,8,8.5zM8.5,14h7c0.3,0,0.5-0.2,0.5-0.5S15.8,13,15.5,13h-7C8.2,13,8,13.2,8,13.5S8.2,14,8.5,14zM32,27.7v1.8h0v3.2c0,0.7-0.6,1.2-1.2,1.2H4.3C3.6,34,3,33.4,3,32.8V2.2C3,1.6,3.6,1,4.3,1h19.6c0.5,0,1,0.2,1.4,0.6l6.1,6.1C31.8,8.1,32,8.6,32,9.1V14h0v11.5V27.7zM31,9.1c0-0.3-0.1-0.5-0.3-0.7l-6.1-6.1C24.4,2.1,24.1,2,23.9,2H4.3C4.1,2,4,2.1,4,2.2v30.5C4,32.9,4.1,33,4.3,33h26.5c0.1,0,0.2-0.1,0.2-0.2v-2.2l0,0v-2.8v-2.2v-13l0,0V9.1zM26.5,28h-4c-0.3,0-0.5,0.2-0.5,0.5s0.2,0.5,0.5,0.5h4c0.3,0,0.5-0.2,0.5-0.5S26.8,28,26.5,28zM19.5,28h-11C8.2,28,8,28.2,8,28.5S8.2,29,8.5,29h11c0.3,0,0.5-0.2,0.5-0.5S19.8,28,19.5,28zM8.5,24h4c0.3,0,0.5-0.2,0.5-0.5S12.8,23,12.5,23h-4C8.2,23,8,23.2,8,23.5S8.2,24,8.5,24zM26.5,23h-11c-0.3,0-0.5,0.2-0.5,0.5s0.2,0.5,0.5,0.5h11c0.3,0,0.5-0.2,0.5-0.5S26.8,23,26.5,23zM26.5,18h-18C8.2,18,8,18.2,8,18.5S8.2,19,8.5,19h18c0.3,0,0.5-0.2,0.5-0.5S26.8,18,26.5,18zM29,9l-4.9,0C24,9,24,9,24,8.9L24,4c0-0.3-0.2-0.5-0.5-0.5S23,3.7,23,4l0,4.9c0,0.6,0.5,1.1,1.1,1.1l4.9,0c0.3,0,0.5-0.2,0.5-0.5C29.5,9.2,29.3,9,29,9zM26.5,13h-8c-0.3,0-0.5,0.2-0.5,0.5s0.2,0.5,0.5,0.5h8c0.3,0,0.5-0.2,0.5-0.5S26.8,13,26.5,13z"
                        :icon/file-happy "M29.5,9.5c0,0.3-0.2,0.5-0.5,0.5l-4.9,0C23.5,10,23,9.5,23,8.9L23,4c0-0.3,0.2-0.5,0.5-0.5S24,3.7,24,4l0,4.9C24,9,24,9,24.1,9L29,9C29.3,9,29.5,9.2,29.5,9.5zM32,28.7c0,0.7-0.6,1.3-1.3,1.3L8.3,30C7.6,30,7,29.4,7,28.7L7,2.3C7,1.6,7.6,1,8.3,1l15.5,0c0.6,0,1.2,0.2,1.6,0.7l5.9,5.9C31.7,8,32,8.6,32,9.2L32,28.7zM31,28.7l0-19.5c0-0.3-0.1-0.7-0.4-0.9l-5.9-5.9C24.5,2.1,24.1,2,23.8,2L8.3,2C8.1,2,8,2.1,8,2.3l0,26.4C8,28.9,8.1,29,8.3,29l22.4,0C30.9,29,31,28.9,31,28.7zM27.5,31c-0.3,0-0.5,0.2-0.5,0.5l0,1.2c0,0.2-0.1,0.3-0.3,0.3L4.3,33C4.1,33,4,32.9,4,32.7L4,6.3C4,6.1,4.1,6,4.3,6l1.2,0C5.8,6,6,5.8,6,5.5C6,5.2,5.8,5,5.5,5L4.3,5C3.6,5,3,5.6,3,6.3l0,26.4C3,33.4,3.6,34,4.3,34l22.4,0c0.7,0,1.3-0.6,1.3-1.3l0-1.2C28,31.2,27.8,31,27.5,31zM24,14c-0.9,0-1.7,0.6-2,1.4c-0.1,0.3,0.1,0.5,0.3,0.6c0.3,0.1,0.5-0.1,0.6-0.3c0.1-0.4,0.5-0.6,1-0.6s0.9,0.3,1,0.6c0.1,0.2,0.3,0.4,0.5,0.4c0,0,0.1,0,0.1,0c0.3-0.1,0.4-0.4,0.3-0.6C25.7,14.6,24.9,14,24,14zM15,14c-0.9,0-1.7,0.6-2,1.4c-0.1,0.3,0.1,0.5,0.3,0.6c0.3,0.1,0.5-0.1,0.6-0.3c0.1-0.4,0.5-0.6,1-0.6s0.9,0.3,1,0.6c0.1,0.2,0.3,0.4,0.5,0.4c0,0,0.1,0,0.1,0c0.3-0.1,0.4-0.4,0.3-0.6C16.7,14.6,15.9,14,15,14zM21.5,19c-0.5,0-3.5,0-4,0c-0.3,0-0.6,0.5-0.5,0.8c0.3,1.3,1.3,2.2,2.5,2.2s2.2-0.9,2.5-2.2C22.1,19.5,21.8,19,21.5,19z"
                        :icon/file-happy-2 "M29.5,9.5c0,0.3-0.2,0.5-0.5,0.5l-4.9,0C23.5,10,23,9.5,23,8.9L23,4c0-0.3,0.2-0.5,0.5-0.5S24,3.7,24,4l0,4.9C24,9,24,9,24.1,9L29,9C29.3,9,29.5,9.2,29.5,9.5zM32,9.1v23.6c0,0.7-0.6,1.2-1.2,1.2H4.2C3.6,34,3,33.4,3,32.8V2.2C3,1.6,3.6,1,4.2,1h19.6c0.5,0,1,0.2,1.4,0.6l6.1,6.1C31.8,8.1,32,8.6,32,9.1zM31,9.1c0-0.3-0.1-0.5-0.3-0.7l-6.1-6.1C24.4,2.1,24.1,2,23.9,2H4.2C4.1,2,4,2.1,4,2.2v30.5C4,32.9,4.1,33,4.2,33h26.5c0.1,0,0.2-0.1,0.2-0.2V9.1zM22,15c-0.9,0-1.7,0.6-2,1.4c-0.1,0.3,0.1,0.5,0.3,0.6c0.3,0.1,0.5-0.1,0.6-0.3c0.1-0.4,0.5-0.6,1-0.6s0.9,0.3,1,0.6c0.1,0.2,0.3,0.4,0.5,0.4c0,0,0.1,0,0.1,0c0.3-0.1,0.4-0.4,0.3-0.6C23.7,15.6,22.9,15,22,15zM13,15c-0.9,0-1.7,0.6-2,1.4c-0.1,0.3,0.1,0.5,0.3,0.6c0.3,0.1,0.5-0.1,0.6-0.3c0.1-0.4,0.5-0.6,1-0.6s0.9,0.3,1,0.6c0.1,0.2,0.3,0.4,0.5,0.4c0,0,0.1,0,0.1,0c0.3-0.1,0.4-0.4,0.3-0.6C14.7,15.6,13.9,15,13,15zM19.5,20c-0.5,0-3.5,0-4,0c-0.3,0-0.6,0.5-0.5,0.8c0.3,1.3,1.3,2.2,2.5,2.2s2.2-0.9,2.5-2.2C20.1,20.5,19.8,20,19.5,20z"
                        :icon/file-link "M14.6,11.6C14.2,12,14,12.4,14,13s0.2,1,0.6,1.4l1.1,1.1c0.2,0.2,0.2,0.5,0,0.7c-0.2,0.2-0.5,0.2-0.7,0l-1.1-1.1C13.3,14.5,13,13.8,13,13s0.3-1.5,0.9-2.1c1.1-1.1,3.1-1.1,4.2,0l3.1,3.1c0.6,0.6,0.9,1.3,0.9,2.1s-0.3,1.5-0.9,2.1c0,0-0.2,0.2-0.6,0.4c-0.1,0-0.1,0-0.2,0c-0.2,0-0.4-0.1-0.5-0.3c-0.1-0.3,0-0.5,0.3-0.6c0.1,0,0.2-0.1,0.2-0.1c0.4-0.4,0.6-0.9,0.6-1.4s-0.2-1-0.6-1.4l-3.1-3.1C16.6,10.8,15.3,10.8,14.6,11.6zM27.5,31c-0.3,0-0.5,0.2-0.5,0.5l0,1.2c0,0.2-0.1,0.3-0.3,0.3L4.3,33C4.1,33,4,32.9,4,32.7L4,6.3C4,6.1,4.1,6,4.3,6l1.2,0C5.8,6,6,5.8,6,5.5C6,5.2,5.7,5,5.5,5L4.3,5C3.6,5,3,5.6,3,6.3l0,26.4C3,33.4,3.6,34,4.3,34l22.4,0c0.7,0,1.3-0.6,1.3-1.3l0-1.2C28,31.2,27.8,31,27.5,31zM32,28.7c0,0.7-0.6,1.3-1.3,1.3L8.3,30C7.6,30,7,29.4,7,28.7L7,2.3C7,1.6,7.6,1,8.3,1l15.5,0c0.6,0,1.2,0.2,1.6,0.7l5.9,5.9C31.7,8,32,8.6,32,9.2L32,28.7zM31,28.7l0-19.5c0-0.3-0.1-0.7-0.4-0.9l-5.9-5.9C24.5,2.1,24.1,2,23.8,2L8.3,2C8.1,2,8,2.1,8,2.3l0,26.4C8,28.9,8.1,29,8.3,29l22.4,0C30.9,29,31,28.9,31,28.7zM29,9l-4.9,0C24,9,24,9,24,8.9L24,4c0-0.3-0.2-0.5-0.5-0.5S23,3.7,23,4l0,4.9c0,0.6,0.5,1.1,1.1,1.1l4.9,0c0.3,0,0.5-0.2,0.5-0.5C29.5,9.2,29.3,9,29,9zM24.4,21.5c0.4-0.4,0.6-0.9,0.6-1.4s-0.2-1-0.6-1.4l-1.1-1.1c-0.2-0.2-0.2-0.5,0-0.7c0.2-0.2,0.5-0.2,0.7,0l1.1,1.1c0.6,0.6,0.9,1.3,0.9,2.1s-0.3,1.5-0.9,2.1c-1.1,1.1-3.1,1.1-4.2,0l-3.1-3.1C17.3,18.6,17,17.8,17,17s0.3-1.5,0.9-2.1c0,0,0.2-0.2,0.6-0.4c0.1,0,0.1,0,0.2,0c0.2,0,0.4,0.1,0.5,0.3c0.1,0.3,0,0.5-0.3,0.6c-0.1,0-0.2,0.1-0.2,0.1C18.2,16,18,16.5,18,17s0.2,1,0.6,1.4l3.1,3.1C22.4,22.2,23.7,22.2,24.4,21.5z"
                        :icon/file-loading "M17.8,17.1c0.1,0.1,0.2,0.2,0.2,0.4c0,0.1,0,0.1,0,0.2c0,0.1-0.1,0.1-0.1,0.2c0,0-0.1,0.1-0.2,0.1c-0.1,0-0.1,0-0.2,0c-0.1,0-0.1,0-0.2,0c-0.1,0-0.1-0.1-0.2-0.1c0,0-0.1-0.1-0.1-0.2c0-0.1,0-0.1,0-0.2c0-0.1,0-0.3,0.1-0.4c0.1-0.1,0.4-0.2,0.5-0.1C17.8,17.1,17.8,17.1,17.8,17.1zM19.1,17.1C19,17.2,19,17.4,19,17.5c0,0.1,0,0.1,0,0.2c0,0.1,0.1,0.1,0.1,0.2c0.1,0.1,0.2,0.2,0.4,0.2c0.1,0,0.1,0,0.2,0c0.1,0,0.1-0.1,0.2-0.1c0,0,0.1-0.1,0.1-0.2c0-0.1,0-0.1,0-0.2c0-0.1,0-0.3-0.2-0.4C19.7,17,19.3,17,19.1,17.1zM27.5,31c-0.3,0-0.5,0.2-0.5,0.5l0,1.2c0,0.2-0.1,0.3-0.3,0.3H4.3C4.1,33,4,32.9,4,32.7V6.3C4,6.1,4.1,6,4.3,6l1.2,0C5.8,6,6,5.8,6,5.5C6,5.2,5.7,5,5.5,5L4.3,5C3.6,5,3,5.6,3,6.3v26.4C3,33.4,3.6,34,4.3,34h22.4c0.7,0,1.3-0.6,1.3-1.3l0-1.2C28,31.2,27.8,31,27.5,31zM29,9l-4.9,0C24,9,24,9,24,8.9L24,4c0-0.3-0.2-0.5-0.5-0.5S23,3.7,23,4l0,4.9c0,0.6,0.5,1.1,1.1,1.1l4.9,0c0.3,0,0.5-0.2,0.5-0.5C29.5,9.2,29.3,9,29,9zM21.1,17.1C21,17.2,21,17.4,21,17.5s0,0.3,0.1,0.3c0,0,0.1,0.1,0.2,0.1c0.1,0,0.1,0,0.2,0c0.1,0,0.1,0,0.2,0c0.1,0,0.1-0.1,0.2-0.1c0.1-0.1,0.2-0.2,0.2-0.3s0-0.3-0.2-0.4C21.7,17,21.3,17,21.1,17.1zM32,28.7c0,0.7-0.6,1.3-1.3,1.3L8.3,30C7.6,30,7,29.4,7,28.7V2.3C7,1.6,7.6,1,8.3,1l15.5,0c0.6,0,1.2,0.2,1.6,0.7l5.9,5.9C31.7,8,32,8.6,32,9.2L32,28.7zM31,28.7l0-19.5c0-0.3-0.1-0.7-0.4-0.9l-5.9-5.9C24.5,2.1,24.1,2,23.8,2L8.3,2C8.1,2,8,2.1,8,2.3v26.4C8,28.9,8.1,29,8.3,29l22.4,0C30.9,29,31,28.9,31,28.7z"
                        :icon/file-loading2 "M32,9.2c0-0.6-0.2-1.2-0.7-1.6l-5.9-5.9C25,1.2,24.4,1,23.8,1L8.3,1C7.6,1,7,1.6,7,2.3l0,26.4C7,29.4,7.6,30,8.3,30l22.4,0c0.7,0,1.3-0.6,1.3-1.3L32,9.2zM30.7,29L8.3,29C8.1,29,8,28.9,8,28.7L8,2.3C8,2.1,8.1,2,8.3,2l15.5,0c0.3,0,0.7,0.1,0.9,0.4l5.9,5.9C30.8,8.5,31,8.9,31,9.2l0,19.5C31,28.9,30.9,29,30.7,29zM28,31.5l0,1.2c0,0.7-0.6,1.3-1.3,1.3L4.3,34C3.6,34,3,33.4,3,32.7L3,6.3C3,5.6,3.6,5,4.3,5l1.2,0C5.7,5,6,5.2,6,5.5C6,5.8,5.8,6,5.5,6L4.3,6C4.1,6,4,6.1,4,6.3l0,26.4C4,32.9,4.1,33,4.3,33l22.4,0c0.2,0,0.3-0.1,0.3-0.3l0-1.2c0-0.3,0.2-0.5,0.5-0.5C27.8,31,28,31.2,28,31.5zM29.5,9.5c0,0.3-0.2,0.5-0.5,0.5l-4.9,0C23.5,10,23,9.5,23,8.9L23,4c0-0.3,0.2-0.5,0.5-0.5S24,3.7,24,4l0,4.9C24,9,24,9,24.1,9L29,9C29.3,9,29.5,9.2,29.5,9.5zM23,16.5c0,1.9-1.6,3.5-3.5,3.5c-0.2,0-0.4,0-0.6,0c-0.3,0-0.5-0.3-0.4-0.6c0-0.3,0.3-0.4,0.6-0.4c1.5,0.3,2.9-0.9,2.9-2.5c0-0.3,0.2-0.5,0.5-0.5S23,16.2,23,16.5zM17.6,18.2c0.2,0.2,0.2,0.5,0,0.7C17.5,19,17.4,19,17.3,19c-0.1,0-0.3-0.1-0.4-0.2c-0.6-0.6-0.9-1.5-0.9-2.3c0-0.4,0.1-0.8,0.2-1.2c0.1-0.3,0.4-0.4,0.6-0.3c0.3,0.1,0.4,0.4,0.3,0.6C17,15.9,17,16.2,17,16.5C17,17.1,17.2,17.7,17.6,18.2zM19.5,14c-0.4,0-0.9,0.1-1.3,0.3c-0.1,0-0.2,0.1-0.2,0.1c-0.2,0-0.3-0.1-0.4-0.3c-0.1-0.2-0.1-0.5,0.2-0.7c0.5-0.3,1.1-0.5,1.7-0.5c0.3,0,0.5,0.2,0.5,0.5S19.8,14,19.5,14z"
                        :icon/file-medal "M32,32.5c0,0.8-0.7,1.5-1.5,1.5h-26C3.7,34,3,33.3,3,32.5v-30C3,1.7,3.7,1,4.5,1h19.4c0.5,0,1,0.2,1.4,0.6l6.1,6.1C31.8,8.1,32,8.6,32,9.1v3.4c0,0.3-0.2,0.5-0.5,0.5S31,12.8,31,12.5V9.1c0-0.3-0.1-0.5-0.3-0.7l-6.1-6.1C24.4,2.1,24.1,2,23.9,2H4.5C4.2,2,4,2.2,4,2.5v30C4,32.8,4.2,33,4.5,33h26c0.3,0,0.5-0.2,0.5-0.5c0-0.3,0.2-0.5,0.5-0.5S32,32.2,32,32.5zM8.5,9h11C19.8,9,20,8.8,20,8.5S19.8,8,19.5,8h-11C8.2,8,8,8.2,8,8.5S8.2,9,8.5,9zM8.5,14h7c0.3,0,0.5-0.2,0.5-0.5S15.8,13,15.5,13h-7C8.2,13,8,13.2,8,13.5S8.2,14,8.5,14zM21.5,13.5c0-0.3-0.2-0.5-0.5-0.5h-2.5c-0.3,0-0.5,0.2-0.5,0.5s0.2,0.5,0.5,0.5H21C21.3,14,21.5,13.8,21.5,13.5zM8.5,19h10c0.3,0,0.5-0.2,0.5-0.5S18.8,18,18.5,18h-10C8.2,18,8,18.2,8,18.5S8.2,19,8.5,19zM8.5,24h4c0.3,0,0.5-0.2,0.5-0.5S12.8,23,12.5,23h-4C8.2,23,8,23.2,8,23.5S8.2,24,8.5,24zM18.5,23h-3c-0.3,0-0.5,0.2-0.5,0.5s0.2,0.5,0.5,0.5h3c0.3,0,0.5-0.2,0.5-0.5S18.8,23,18.5,23zM8.5,29H20c0.3,0,0.5-0.2,0.5-0.5S20.3,28,20,28H8.5C8.2,28,8,28.2,8,28.5S8.2,29,8.5,29zM23.5,3.5C23.2,3.5,23,3.7,23,4v5c0,0.6,0.5,1,1,1h5c0.3,0,0.5-0.2,0.5-0.5S29.3,9,29,9l-5,0V4C24,3.7,23.8,3.5,23.5,3.5zM34.3,20l0.6,2c0.1,0.2,0,0.4-0.2,0.6l-1.7,1.2L33,24.1v6.4c0,0.2-0.1,0.3-0.2,0.4C32.7,31,32.6,31,32.5,31c-0.1,0-0.1,0-0.2,0L28,29L23.7,31c-0.2,0.1-0.3,0.1-0.5,0S23,30.7,23,30.5v-6.4l-0.1-0.4l-1.7-1.2C21,22.4,21,22.2,21,22l0.6-2L21,18c-0.1-0.2,0-0.4,0.2-0.6l1.7-1.2l0.6-1.9c0.1-0.2,0.3-0.3,0.5-0.3H26l1.7-1.2c0.2-0.1,0.4-0.1,0.6,0L30,14H32c0.2,0,0.4,0.1,0.5,0.3l0.6,1.9l1.7,1.2C35,17.6,35,17.8,35,18L34.3,20zM32,29.7V26h-2l-1.7,1.2c-0.1,0.1-0.2,0.1-0.3,0.1s-0.2,0-0.3-0.1L26,26h-2v3.7l3.8-1.7c0.1-0.1,0.3-0.1,0.4,0L32,29.7zM33.3,20.2c0-0.1,0-0.2,0-0.3l0.6-1.8L32.4,17c-0.1-0.1-0.1-0.1-0.2-0.2L31.7,15h-1.9c-0.1,0-0.2,0-0.3-0.1L28,13.8l-1.5,1.1C26.4,15,26.3,15,26.2,15h-1.9l-0.6,1.7c0,0.1-0.1,0.2-0.2,0.2l-1.5,1.1l0.6,1.8c0,0.1,0,0.2,0,0.3l-0.6,1.8l1.5,1.1c0.1,0.1,0.1,0.1,0.2,0.2l0.2,0.5c0,0,0,0.1,0.1,0.2l0.4,1.1h1.9c0.1,0,0.2,0,0.3,0.1l1.5,1.1l1.5-1.1c0.1-0.1,0.2-0.1,0.3-0.1h1.9l0.4-1.1c0-0.1,0-0.1,0.1-0.2l0.2-0.5c0-0.1,0.1-0.2,0.2-0.2l1.5-1.1L33.3,20.2zM32,20c0,2.2-1.8,4-4,4s-4-1.8-4-4s1.8-4,4-4S32,17.8,32,20zM31,20c0-1.7-1.3-3-3-3s-3,1.3-3,3s1.3,3,3,3S31,21.7,31,20z"
                        :icon/file-question "M31.4,7.7l-6.1-6.1C24.9,1.2,24.4,1,23.9,1H4.3C3.6,1,3,1.6,3,2.2v30.5C3,33.4,3.6,34,4.3,34h26.5c0.7,0,1.3-0.6,1.3-1.2V9.1C32,8.6,31.8,8.1,31.4,7.7zM31,32.8c0,0.1-0.1,0.2-0.2,0.2H4.3C4.1,33,4,32.9,4,32.8V2.2C4,2.1,4.1,2,4.3,2h19.6c0.3,0,0.5,0.1,0.7,0.3l6.1,6.1C30.9,8.6,31,8.9,31,9.1V32.8zM29.5,9.5c0,0.3-0.2,0.5-0.5,0.5l-4.9,0C23.5,10,23,9.5,23,8.9L23,4c0-0.3,0.2-0.5,0.5-0.5S24,3.7,24,4l0,4.9C24,9,24,9,24.1,9L29,9C29.3,9,29.5,9.2,29.5,9.5zM17,26.5c0,0.3-0.2,0.5-0.5,0.5S16,26.8,16,26.5c0-0.3,0.2-0.5,0.5-0.5S17,26.2,17,26.5zM20.9,15.4c0.3,1.2,0.1,2.5-0.6,3.5c-0.7,1-1.8,1.7-3,1.9c-0.2,0-0.3,0.2-0.3,0.3v2.3c0,0.3-0.2,0.5-0.5,0.5S16,23.8,16,23.5v-2.3c0-0.6,0.5-1.2,1.1-1.3c0.9-0.2,1.8-0.7,2.3-1.5c0.5-0.8,0.7-1.8,0.5-2.8c-0.3-1.3-1.3-2.3-2.6-2.6c-1.1-0.2-2.2,0-3,0.7c-0.8,0.7-1.3,1.7-1.3,2.7c0,0.3-0.2,0.5-0.5,0.5S12,16.8,12,16.5c0-1.4,0.6-2.7,1.7-3.5c1.1-0.9,2.5-1.2,3.9-0.9C19.2,12.5,20.5,13.8,20.9,15.4z"
                        :icon/file-sad "M29.5,9.5c0,0.3-0.2,0.5-0.5,0.5l-4.9,0C23.5,10,23,9.5,23,8.9L23,4c0-0.3,0.2-0.5,0.5-0.5S24,3.7,24,4l0,4.9C24,9,24,9,24.1,9L29,9C29.3,9,29.5,9.2,29.5,9.5zM32,28.7c0,0.7-0.6,1.3-1.3,1.3L8.3,30C7.6,30,7,29.4,7,28.7L7,2.3C7,1.6,7.6,1,8.3,1l15.5,0c0.6,0,1.2,0.2,1.6,0.7l5.9,5.9C31.7,8,32,8.6,32,9.2L32,28.7zM31,28.7l0-19.5c0-0.3-0.1-0.7-0.4-0.9l-5.9-5.9C24.5,2.1,24.1,2,23.8,2L8.3,2C8.1,2,8,2.1,8,2.3l0,26.4C8,28.9,8.1,29,8.3,29l22.4,0C30.9,29,31,28.9,31,28.7zM27.5,31C27.5,31,27.5,31,27.5,31c-0.3,0-0.5,0.2-0.5,0.5l0,1.2c0,0.2-0.1,0.3-0.3,0.3L4.3,33C4.1,33,4,32.9,4,32.7L4,6.3C4,6.1,4.1,6,4.3,6l1.2,0C5.8,6,6,5.8,6,5.5C6,5.2,5.8,5,5.5,5c0,0,0,0,0,0L4.3,5C3.6,5,3,5.6,3,6.3l0,26.4C3,33.4,3.6,34,4.3,34l22.4,0c0.7,0,1.3-0.6,1.3-1.3l0-1.2C28,31.2,27.8,31,27.5,31zM25.6,14c-0.3-0.1-0.5,0.1-0.6,0.3c-0.1,0.4-0.5,0.6-1,0.6s-0.9-0.3-1-0.6c-0.1-0.3-0.4-0.4-0.6-0.3c-0.3,0.1-0.4,0.4-0.3,0.6c0.2,0.8,1.1,1.4,2,1.4s1.7-0.6,2-1.4C26.1,14.4,25.9,14.1,25.6,14zM16.6,14c-0.3-0.1-0.5,0.1-0.6,0.3c-0.1,0.4-0.5,0.6-1,0.6s-0.9-0.3-1-0.6c-0.1-0.3-0.4-0.4-0.6-0.3c-0.3,0.1-0.4,0.4-0.3,0.6c0.2,0.8,1.1,1.4,2,1.4s1.7-0.6,2-1.4C17.1,14.4,16.9,14.1,16.6,14zM19.5,19c-1.2,0-2.2,0.5-2.5,1.3c-0.1,0.3,0,0.5,0.3,0.6c0.3,0.1,0.5,0,0.6-0.3c0.1-0.3,0.7-0.7,1.5-0.7s1.4,0.4,1.5,0.7c0.1,0.2,0.3,0.3,0.5,0.3c0.1,0,0.1,0,0.2,0c0.3-0.1,0.4-0.4,0.3-0.6C21.7,19.5,20.7,19,19.5,19z"
                        :icon/file-sad-2 "M29.5,9.5c0,0.3-0.2,0.5-0.5,0.5l-4.9,0C23.5,10,23,9.5,23,8.9L23,4c0-0.3,0.2-0.5,0.5-0.5S24,3.7,24,4l0,4.9C24,9,24,9,24.1,9L29,9C29.3,9,29.5,9.2,29.5,9.5zM32,9.1v23.6c0,0.7-0.6,1.2-1.2,1.2H4.3C3.6,34,3,33.4,3,32.8V2.2C3,1.6,3.6,1,4.3,1h19.6c0.5,0,1,0.2,1.4,0.6l6.1,6.1C31.8,8.1,32,8.6,32,9.1zM31,9.1c0-0.3-0.1-0.5-0.3-0.7l-6.1-6.1C24.4,2.1,24.1,2,23.9,2H4.3C4.1,2,4,2.1,4,2.2v30.5C4,32.9,4.1,33,4.3,33h26.5c0.1,0,0.2-0.1,0.2-0.2V9.1zM23.6,15c-0.3-0.1-0.5,0.1-0.6,0.3c-0.1,0.4-0.5,0.6-1,0.6s-0.9-0.3-1-0.6c-0.1-0.3-0.4-0.4-0.6-0.3c-0.3,0.1-0.4,0.4-0.3,0.6c0.2,0.8,1.1,1.4,2,1.4s1.7-0.6,2-1.4C24.1,15.4,23.9,15.1,23.6,15zM14.6,15c-0.3-0.1-0.5,0.1-0.6,0.3c-0.1,0.4-0.5,0.6-1,0.6s-0.9-0.3-1-0.6c-0.1-0.3-0.4-0.4-0.6-0.3c-0.3,0.1-0.4,0.4-0.3,0.6c0.2,0.8,1.1,1.4,2,1.4s1.7-0.6,2-1.4C15.1,15.4,14.9,15.1,14.6,15zM17.5,20c-1.2,0-2.2,0.5-2.5,1.3c-0.1,0.3,0,0.5,0.3,0.6c0.3,0.1,0.5,0,0.6-0.3c0.1-0.3,0.7-0.7,1.5-0.7s1.4,0.4,1.5,0.7c0.1,0.2,0.3,0.3,0.5,0.3c0.1,0,0.1,0,0.2,0c0.3-0.1,0.4-0.4,0.3-0.6C19.7,20.5,18.7,20,17.5,20z"
                        :icon/file-search "M18,5.5c0,0.3-0.2,0.5-0.5,0.5S17,5.8,17,5.5C17,5.2,17.2,5,17.5,5S18,5.2,18,5.5zM17.5,7C17.2,7,17,7.2,17,7.5s0.2,0.5,0.5,0.5S18,7.8,18,7.5S17.8,7,17.5,7zM17.5,13c-0.3,0-0.5,0.2-0.5,0.5s0.2,0.5,0.5,0.5s0.5-0.2,0.5-0.5S17.8,13,17.5,13zM17.5,9C17.2,9,17,9.2,17,9.5c0,0.3,0.2,0.5,0.5,0.5S18,9.8,18,9.5C18,9.2,17.8,9,17.5,9zM17.5,11c-0.3,0-0.5,0.2-0.5,0.5s0.2,0.5,0.5,0.5s0.5-0.2,0.5-0.5S17.8,11,17.5,11zM17.5,3C17.2,3,17,3.2,17,3.5s0.2,0.5,0.5,0.5S18,3.8,18,3.5S17.8,3,17.5,3zM16.5,13.1c0.3,0,0.5-0.2,0.5-0.5S16.8,12,16.5,12s-0.5,0.2-0.5,0.5S16.2,13.1,16.5,13.1zM16.5,11.1c0.3,0,0.5-0.2,0.5-0.5S16.8,10,16.5,10s-0.5,0.2-0.5,0.5S16.2,11.1,16.5,11.1zM24.1,10l4.9,0c0.3,0,0.5-0.2,0.5-0.5C29.5,9.2,29.3,9,29,9l-4.9,0C24,9,24,9,24,8.9L24,4c0-0.3-0.2-0.5-0.5-0.5S23,3.7,23,4l0,4.9C23,9.5,23.5,10,24.1,10zM16.5,4c-0.3,0-0.5,0.2-0.5,0.5c0,0.3,0.2,0.5,0.5,0.5S17,4.9,17,4.6C17,4.2,16.8,4,16.5,4zM16.5,7.1c0.3,0,0.5-0.2,0.5-0.5S16.8,6,16.5,6s-0.5,0.2-0.5,0.5S16.2,7.1,16.5,7.1zM16.5,9.1c0.3,0,0.5-0.2,0.5-0.5C17,8.2,16.8,8,16.5,8s-0.5,0.2-0.5,0.5C15.9,8.9,16.2,9.1,16.5,9.1zM31.4,7.7l-6.1-6.1C24.9,1.2,24.4,1,23.9,1H4.2C3.6,1,3,1.6,3,2.2v26.2C3,28.8,3.2,29,3.5,29S4,28.8,4,28.5V2.2C4,2.1,4.1,2,4.2,2h19.6c0.3,0,0.5,0.1,0.7,0.3l6.1,6.1C30.9,8.6,31,8.9,31,9.1v23.6c0,0.1-0.1,0.2-0.2,0.2H8.5C8.2,33,8,33.2,8,33.5S8.2,34,8.5,34h22.2c0.7,0,1.2-0.6,1.2-1.2V9.1C32,8.6,31.8,8.1,31.4,7.7zM9.6,28.1l-5.7,5.7C3.8,34,3.6,34,3.5,34s-0.3,0-0.4-0.1c-0.2-0.2-0.2-0.5,0-0.7l5.7-5.7c-1.2-1.3-1.9-3-1.9-4.9c0-4.1,3.4-7.5,7.5-7.5s7.5,3.4,7.5,7.5S18.6,30,14.5,30C12.6,30,10.9,29.3,9.6,28.1zM8,22.5c0,3.6,2.9,6.5,6.5,6.5s6.5-2.9,6.5-6.5S18.1,16,14.5,16S8,18.9,8,22.5zM10.5,23c0.3,0,0.5-0.2,0.5-0.5c0-1.9,1.6-3.5,3.5-3.5c0.3,0,0.5-0.2,0.5-0.5S14.8,18,14.5,18C12,18,10,20,10,22.5C10,22.8,10.2,23,10.5,23z"
                        :icon/funnel "M30.4,3.5C30.2,3.2,29.9,3,29.5,3H5.5C5.1,3,4.8,3.2,4.6,3.5c-0.2,0.3-0.1,0.7,0.1,1L15,18.7v12.8c0,0.2,0.1,0.4,0.3,0.5c0.1,0,0.1,0,0.2,0c0.1,0,0.2,0,0.3-0.1l4-3.5c0.1-0.1,0.2-0.2,0.2-0.4v-9.3L30.3,4.6C30.6,4.3,30.6,3.9,30.4,3.5zM19.1,18.2C19,18.3,19,18.4,19,18.5v9.3l-3,2.6V18.5c0-0.1,0-0.2-0.1-0.3L5.5,4l24.1,0L19.1,18.2z"
                        :icon/git "M33.9,17.3c0-0.5-0.3-0.9-0.6-1.3c-0.1-0.1-0.2-0.2-0.3-0.3c-4.6-4.6-9.2-9.2-13.8-13.8c-0.1-0.1-0.3-0.3-0.4-0.4C18.4,1.2,17.9,1,17.4,1c-0.5,0-1,0.2-1.3,0.5c-0.2,0.2-0.4,0.4-0.6,0.5c-0.8,0.8-1.7,1.7-2.5,2.5c0.1,0.1,0.2,0.2,0.2,0.3c1.1,1.1,2.2,2.1,3.2,3.2c0.1,0.1,0.3,0.2,0.5,0.2c0.1,0,0.1,0,0.2,0c0.2,0,0.3,0,0.5,0c1.8,0,2.8,1.7,2.5,3.1c-0.1,0.2,0,0.4,0.2,0.6c1.1,1,2.1,2.1,3.1,3.1c0.1,0.1,0.2,0.2,0.4,0.2c0,0,0.1,0,0.2,0c0.2,0,0.4-0.1,0.6-0.1c1.1,0,2.2,0.8,2.5,2.1c0.2,1.3-0.5,2.6-2,3c-0.2,0-0.4,0.1-0.6,0.1c-1.2,0-2.2-0.8-2.4-1.9c-0.1-0.4-0.1-0.9,0-1.3c0-0.2,0.1-0.3-0.1-0.4c-1-1-2.1-2-3.1-3.1c0,0,0,0-0.1,0c0,2.9,0,5.8,0,8.8c0.7,0.4,1.2,1,1.3,1.8c0.1,0.5,0,1-0.1,1.5c-0.4,0.9-1.4,1.6-2.3,1.6c0,0-0.1,0-0.1,0c-1.4-0.1-2.2-1-2.4-2c-0.3-1.1,0.2-2.3,1.4-3c0-0.1,0-0.2,0-0.4c0-2.7,0-5.5,0-8.2c0-0.3-0.1-0.5-0.4-0.6c-0.8-0.5-1.1-1.2-1.2-2.1c0-0.2,0.1-0.5,0.1-0.7c0-0.2,0-0.3-0.1-0.4c-1.2-1.2-2.3-2.3-3.5-3.5c-0.1,0.1-0.2,0.2-0.3,0.3c-3.1,3.1-6.3,6.3-9.4,9.4c-0.3,0.3-0.6,0.7-0.7,1.2c-0.2,0.7,0,1.3,0.5,1.9c0.1,0.1,0.2,0.2,0.3,0.3c4.6,4.6,9.3,9.3,13.9,13.9c0.4,0.4,0.8,0.7,1.3,0.8c0,0,0,0,0,0.1c0.2,0,0.5,0,0.7,0c0,0,0-0.1,0.1-0.1c0.6-0.1,1-0.5,1.4-0.9c4.6-4.6,9.2-9.2,13.8-13.8c0.5-0.5,0.8-0.9,0.8-1.6c0,0,0.1-0.1,0.1-0.1c0,0,0-0.1,0-0.1C34,17.4,33.9,17.3,33.9,17.3zM32.9,17.6c0,0.3-0.1,0.5-0.5,0.9c-4.6,4.6-9.2,9.2-13.8,13.8c-0.3,0.4-0.6,0.5-0.9,0.6c-0.1,0-0.1,0-0.2,0h-0.1c-0.1,0-0.1,0-0.2,0c-0.3-0.1-0.5-0.2-0.8-0.6c-3.7-3.7-7.3-7.3-11-11l-2.9-2.9l-0.1-0.1c-0.1-0.1-0.1-0.1-0.2-0.2c-0.3-0.3-0.4-0.6-0.3-1c0.1-0.3,0.2-0.5,0.5-0.7l6.4-6.4l2.6-2.6l0.1,0.1c0.8,0.8,1.7,1.7,2.5,2.5C14,10.3,14,10.6,14,10.8c0.1,1.2,0.6,2.2,1.6,2.9c0,2,0,3.9,0,5.9l0,2c-1.2,0.9-1.8,2.3-1.4,3.8c0.4,1.6,1.6,2.6,3.3,2.7c0.1,0,0.2,0,0.2,0c1.3,0,2.7-1,3.2-2.2c0.3-0.7,0.3-1.4,0.2-2.1c-0.2-0.8-0.6-1.5-1.3-2.1v-5.9c0.4,0.4,0.8,0.8,1.2,1.2c-0.1,0.5-0.1,1,0,1.5c0.4,1.6,1.8,2.7,3.4,2.7c0.3,0,0.5,0,0.8-0.1c1.9-0.4,3.1-2.2,2.8-4.1c-0.3-1.7-1.8-2.9-3.5-2.9c-0.2,0-0.4,0-0.5,0c-0.9-1-1.9-1.9-2.8-2.8c0.2-0.9-0.1-1.9-0.7-2.7c-0.7-0.9-1.7-1.3-2.8-1.3c-0.2,0-0.3,0-0.5,0c-0.7-0.7-1.5-1.5-2.2-2.2l-0.5-0.5C15,4,15.6,3.4,16.2,2.7c0.2-0.2,0.4-0.3,0.5-0.5C16.9,2.1,17.2,2,17.4,2c0.3,0,0.5,0.1,0.7,0.2l0.1,0.1c0.1,0.1,0.2,0.2,0.3,0.3l13.2,13.2l0.6,0.6l0.1,0.1c0.1,0.1,0.1,0.1,0.2,0.2c0.3,0.3,0.3,0.5,0.3,0.7c0,0.1,0,0.1,0,0.2C32.9,17.5,32.9,17.6,32.9,17.6z"
                        :icon/github "M33.5,13.3c-2.2-8.4-10.6-13.6-19.1-12C6.9,2.8,1.4,9,1,16.5c-0.3,6,2.2,10.8,7.2,14.4c1.2,0.8,2.5,1.5,3.9,1.9c0.5,0.2,1,0.1,1.3-0.1c0.3-0.2,0.5-0.5,0.5-0.9c0-0.7,0-1.4,0-2.1l0-0.9c0-0.1-0.1-0.3-0.2-0.4c-0.1-0.1-0.2-0.2-0.4-0.1c-0.2,0-0.5,0-0.7,0c-0.5,0-1,0.1-1.4,0c-1.4-0.1-2.4-0.7-2.9-1.9c-0.4-1.1-1-1.8-1.9-2.3c0,0,0,0,0.1,0c0.8,0.2,1.3,0.6,1.9,1.5c1.1,1.8,2.9,2.3,5,1.5c0.2-0.1,0.5-0.3,0.5-0.6c0.1-0.7,0.4-1.2,0.8-1.7c0.1-0.1,0.2-0.3,0.1-0.5c-0.1-0.2-0.2-0.3-0.4-0.3l-0.7-0.1c-0.4-0.1-0.9-0.2-1.3-0.3c-2.3-0.6-3.7-1.8-4.4-3.8c-0.2-0.5-0.3-1.1-0.4-1.7c-0.3-2.2,0.1-3.9,1.3-5.3c0.2-0.2,0.2-0.6,0.2-0.8c-0.3-1-0.3-2,0-3.1c0-0.1,0.1-0.2,0.1-0.3c0.1,0,0.3,0,0.4,0C11,8.8,12,9.4,13,10c0.3,0.2,0.6,0.2,0.9,0.1c2.4-0.6,4.8-0.6,7.2,0c0.4,0.1,0.8-0.1,0.9-0.1c0.3-0.1,0.5-0.3,0.8-0.4c0.3-0.2,0.6-0.3,0.9-0.5c0.7-0.3,1.3-0.5,1.9-0.5c0.4,1.2,0.4,2.3,0.1,3.3c-0.1,0.5,0.1,0.8,0.2,1c0.6,0.7,1,1.4,1.1,2.2c0.4,1.9,0.2,3.8-0.6,5.5c-0.7,1.5-2.1,2.5-4.1,3c-0.4,0.1-0.8,0.2-1.3,0.3l-0.7,0.1c-0.2,0-0.3,0.2-0.4,0.3c-0.1,0.2,0,0.4,0.1,0.5c0.8,0.9,0.9,1.9,0.9,2.9c0,1.1,0,2.2,0,3.3l0,0.6c0,0.7,0.3,1.1,0.5,1.2c0.2,0.1,0.4,0.2,0.7,0.2c0.2,0,0.4,0,0.6-0.1C30.9,30.2,35.6,21.4,33.5,13.3zM22.6,31.9c-0.3,0.1-0.4,0.1-0.5,0c-0.1,0-0.1-0.2-0.1-0.4l0-0.6c0-1.1,0-2.2,0-3.3c0-0.9-0.1-1.9-0.6-2.9c0.4-0.1,0.8-0.2,1.2-0.3c2.3-0.6,3.9-1.7,4.7-3.5c0.9-1.9,1.2-4,0.7-6.2c-0.2-0.9-0.7-1.8-1.4-2.6c0.4-1.3,0.3-2.6-0.2-4c-0.1-0.3-0.3-0.6-0.9-0.6c-0.8,0-1.5,0.2-2.4,0.6c-0.3,0.2-0.6,0.3-0.9,0.5C22,8.8,21.8,9,21.5,9.1c-0.1,0-0.2,0.1-0.2,0.1c-2.5-0.6-5.1-0.6-7.7,0c0,0-0.1,0-0.1,0c-1.1-0.6-2.2-1.3-3.5-1.5c-1-0.2-1.3,0-1.6,1C8,9.8,8,11,8.3,12.2c-1.3,1.6-1.8,3.5-1.5,6C7,18.8,7.1,19.5,7.3,20c0.8,2.3,2.5,3.8,5.1,4.5c0.4,0.1,0.8,0.2,1.2,0.3c-0.2,0.4-0.4,0.9-0.5,1.4c-1.1,0.4-2.7,0.6-3.7-1.1c-0.7-1.1-1.5-1.7-2.5-1.9c-0.4-0.1-0.7,0-1,0c-0.4,0.1-0.6,0.3-0.6,0.5c0,0.1-0.1,0.4,0.2,0.8c0.2,0.2,0.3,0.3,0.5,0.5c0.7,0.4,1.2,1,1.6,1.9c0.6,1.6,1.9,2.5,3.7,2.6c0.5,0,1,0,1.6,0c0.1,0,0.1,0,0.2,0l0,0.3c0,0.7,0,1.4,0,2.1c0,0.1,0,0.1-0.1,0.1c-0.1,0-0.2,0.1-0.4,0c-1.4-0.4-2.6-1.1-3.7-1.8C4,26.8,1.8,22.2,2,16.6c0.3-7,5.5-12.9,12.5-14.3c8-1.5,15.9,3.4,17.9,11.3C34.5,21.2,30.1,29.4,22.6,31.9z"
                        :icon/globe "M21.9,1.6c-0.1,0-0.1,0-0.2,0c-1.2-0.3-2.5-0.5-3.8-0.5c0,0-0.1,0-0.1,0c-0.1,0-0.2,0-0.3,0C11.8,1,6.7,3.9,3.8,8.4c0,0,0,0,0,0.1c0,0,0,0,0,0c0,0,0-0.1,0-0.1C3.5,8.7,3.3,8.9,3.3,9.2c-1.1,1.9-1.9,4.1-2.2,6.4c0,0,0,0,0,0.1c0,0,0,0,0,0C1,16.2,1,16.9,1,17.5c0,4.4,1.7,8.4,4.5,11.3c0,0,0,0.1,0.1,0.1c3,3.1,7.2,5.1,11.9,5.1C26.6,34,34,26.6,34,17.5C34,9.9,28.9,3.6,21.9,1.6zM32.9,16.2c-0.1-0.2-0.2-0.4-0.3-0.6c-0.8-1.5-1.3-2.5-1.9-2.7c-0.5-0.2-3.4-0.5-3.8,0.4c-0.3,0.7,0.7,1.2,1.1,1.4c0.4,0.2,0.8,0.1,1.1,0.1c0.4,0,0.5,0,0.6,0.1c0,0.4-1,2-1.3,2.2c-0.4,0.3-0.5,0.3-0.5,0.3c-0.1,0-0.2-0.3-0.3-0.5c-0.1-0.3-0.3-0.6-0.6-0.9c-0.7-0.9-2-2.3-2.8-1.9c-0.1,0.1-0.4,0.3-0.3,0.8c0.2,1,2.9,4.5,3.9,4.6c0.4,0,0.8-0.2,1-0.3c0.1,0,0.1-0.1,0.2-0.1c0,0.2-0.2,0.6-0.9,1.6L28,20.6c-0.2,0.2-0.4,0.5-0.5,0.7c-0.5,0.5-0.9,1.1-1.1,1.9c-0.1,0.5-0.1,0.9,0,1.4c0,0.4,0.1,0.9,0,1.2c-0.1,0.2-0.3,0.3-0.6,0.5c-0.3,0.2-0.8,0.5-1,1c-0.2,0.4-0.3,0.7-0.3,1c-0.1,0.3-0.1,0.5-0.4,0.8c-0.7,0.7-3.1,2-3.5,1.7c-0.3-0.3-1.3-3-1.3-3.4c0-0.1,0.1-0.3,0.2-0.4c0.2-0.3,0.4-0.6,0.4-1c0-0.4-0.2-0.7-0.4-1c-0.1-0.1-0.1-0.2-0.2-0.3c-0.1-0.4-0.3-0.7-0.5-1c-0.2-0.3-0.4-0.6-0.5-0.9C18,22.6,18,22.5,18,22.5c0.2-0.4,0-0.7-0.6-1.2c-0.5-0.5-1.2-0.7-2-0.8c-0.6-0.1-1.1,0-1.6,0c-1.4,0.1-2.1,0.2-2.6-1.6c-0.6-1.8-0.1-3.3,1.5-4.6c0.1-0.1,0.3-0.2,0.5-0.3c0.8-0.5,1.7-1.2,1.7-2.1c0-0.6-0.5-0.8-0.7-0.9c0,0,0,0,0,0c0,0,0-0.1,0.1-0.1c0.1-0.1,0.1-0.2,0.4-0.2c0.3,0,0.6,0,0.9-0.3c0,0,0,0,0,0h0c0,0,0,0,0,0c0.1-0.1,0.1-0.2,0.1-0.3c0.1-0.2,0.3-0.8,0.4-1c0.1-0.1,0.1-0.3,0.1-0.4l0-0.2c0-0.2-0.1-0.3-0.2-0.5c-0.2-0.3-0.5-0.4-0.8-0.4c-0.1,0-0.1,0-0.2,0c0.1-0.2,0.4-0.5,0.5-0.6c0,0,0,0.1,0,0.1c0.1,0.3,0.1,0.6,0.4,0.8c0.6,0.5,0.8,0.7,1.4,0.5C17.3,8.1,17.5,8,18,8c0.1,0,0.3,0,0.5,0.1c0.4,0.1,1,0.3,1.5-0.2c0.5-0.5,0.7-2.4,0.2-3.1c-0.2-0.3-0.7-0.5-1.2-0.1c-0.1,0.1-0.2,0.2-0.2,0.3c-0.3,0.9-0.3,1-1,0.9c-0.1,0-0.1,0-0.1,0h0c-0.1-0.2,0.2-0.7,0.7-1.1c0.2-0.1,0.3-0.2,0.4-0.4C19,4.2,19.2,4,19.4,4c0,0,0,0,0,0c0.4,0,0.7,0.3,1,0.6c0.4,0.4,0.9,1,1.7,0.5c0.4-0.3,0.7-0.6,0.8-1.1c0.1-0.4,0-0.7-0.1-1.1C28.3,5,32.4,10.1,32.9,16.2zM28.3,18.2c-0.1,0.1-0.3,0.2-0.4,0.2c0,0-0.1,0-0.2-0.1C27.9,18.3,28.1,18.3,28.3,18.2zM26.6,17.1c-0.4-0.5-0.9-1.1-1.2-1.6c0.3,0.3,0.6,0.6,0.8,0.9C26.4,16.7,26.5,16.9,26.6,17.1zM11.4,3.7c-0.2-0.1-0.3-0.2-0.5-0.2c2-1,4.2-1.5,6.6-1.5c0.1,0,0.2,0,0.3,0c0.1,0,0.1,0,0.2,0.1c-0.1,0.2-0.6,0.5-1.8,1.1c-0.1,0-0.2,0.1-0.2,0.1c-0.5,0.3-0.8,0.7-1,1.1c-0.2,0.4-0.4,0.6-0.7,0.7c-0.1-0.2-0.3-0.6-0.8-0.6c-0.4,0-0.8,0.2-1.1,0.3C12.3,4.9,12.1,5,11.9,5c-0.1,0-0.2,0-0.3,0C11.8,4.7,12,4.1,11.4,3.7zM21.4,2.5c0.3,0.3,0.6,0.9,0.5,1.4c0,0.2-0.1,0.3-0.3,0.4c-0.1,0-0.3-0.2-0.4-0.4c-0.4-0.4-0.9-0.9-1.7-1c-0.6,0-1,0.4-1.3,0.7C18,3.8,17.9,3.9,17.7,4c-0.6,0.4-1.3,1.4-1.1,2.1c0.1,0.2,0.2,0.7,1,0.8c1.1,0.2,1.6-0.3,1.8-0.9c0,0.4-0.1,1-0.2,1.1c-0.1,0.1-0.2,0.1-0.6,0C18.5,7,18.2,7,17.9,7C17.3,7,17,7.2,16.8,7.3c-0.1-0.1-0.2-0.2-0.3-0.3c0,0-0.1-0.2-0.1-0.3c-0.1-0.3-0.2-0.9-0.9-1c-0.1,0-0.2,0-0.4,0.1c0.4-0.3,0.6-0.6,0.8-0.9c0.2-0.3,0.4-0.6,0.6-0.8L16.6,4c1.7-0.8,2.4-1.3,2.4-2C19.9,2.2,20.7,2.3,21.4,2.5zM10.2,3.8c-0.2,0.3-0.4,0.6-0.5,1C9.6,5,9.5,5.1,9.5,5.2C9.4,5.4,9.2,5.5,9.1,5.6C8.8,5.8,8.5,6,8.3,6.5C8.1,6.8,8.1,7.1,8.1,7.3c0,0.1,0,0.3,0,0.4c-1.9,0-2,0-2.1,0.1C5.7,7.8,5.5,8,5.2,8.2C6.5,6.4,8.2,4.9,10.2,3.8zM2,16.8c0.2,0.5,0.3,1.2,0.4,1.5c0.1,0.4,0.1,0.7,0.2,0.9c0.5,1.4,1.5,2.7,2.2,3.5c0.3,0.3,0.6,0.7,0.6,0.8c0,0,0,0.1,0,0.1c-0.1,0.3-0.2,0.7-0.2,1.5c0,0.4,0.1,0.8,0.2,1.1c0.1,0.2,0.1,0.4,0.1,0.6c0,0.2,0,0.3-0.1,0.5C3.3,24.6,2,21.2,2,17.5C2,17.3,2,17,2,16.8zM17.5,33c-4.4,0-8.3-1.8-11.2-4.8c0-0.2,0-0.4,0.1-0.6c0.1-0.2,0.2-0.5,0.2-1c0-0.3-0.1-0.6-0.2-0.8c-0.1-0.3-0.2-0.5-0.2-0.8c0-0.7,0.1-1,0.2-1.2c0.2-0.7,0-0.9-0.8-1.9c-0.7-0.8-1.6-2-2-3.2c-0.1-0.2-0.1-0.4-0.2-0.7c-0.2-1-0.5-2.3-1.3-2.7c0.3-2,0.9-3.8,1.9-5.5c0.4-0.1,0.8-0.3,1.5-0.7C5.8,9,6.1,8.7,6.2,8.7c0,0,0.3,0,2,0c0,0,0,0,0,0c0.1,0,0.3,0,0.4-0.1C9,8.3,9,7.8,9.1,7.4c0-0.2,0-0.3,0.1-0.4c0.2-0.3,0.3-0.4,0.5-0.6c0.2-0.2,0.4-0.3,0.6-0.7c0-0.1,0.1-0.2,0.1-0.3c0,0.1,0,0.1,0.1,0.2C10.8,6,11.2,6.1,12,6c0.3,0,0.6-0.2,0.9-0.4c0.2-0.1,0.4-0.2,0.5-0.2c0,0,0.1,0.1,0.1,0.2c0.1,0.2,0.3,0.5,0.7,0.5c0.3,0,0.5-0.1,0.7-0.2c-0.1,0.1-0.3,0.2-0.5,0.4c-0.5,0.5-0.7,0.9-0.6,1.3c0.1,0.6,0.7,0.7,1,0.8c0.1,0,0.2,0,0.2,0C15,8.6,15,8.6,15,8.6c-0.1,0.2-0.3,0.5-0.4,1c0,0-0.1,0-0.1,0c-0.3,0-0.9,0-1.3,0.7c-0.1,0.2-0.3,0.6-0.1,1c0.1,0.3,0.4,0.5,0.6,0.5c0,0,0.1,0,0.1,0c0,0.4-0.8,0.9-1.2,1.2c-0.2,0.1-0.4,0.3-0.5,0.4c-1.9,1.5-2.5,3.5-1.8,5.7c0.8,2.5,2.2,2.4,3.6,2.3c0.5,0,1-0.1,1.5,0c0.6,0.1,1,0.2,1.4,0.6c0.1,0.1,0.2,0.2,0.3,0.3c0,0.2,0,0.5,0.1,0.8c0.1,0.4,0.4,0.8,0.6,1.1c0.2,0.3,0.3,0.6,0.5,0.8c0.1,0.2,0.2,0.3,0.3,0.5c0.1,0.1,0.2,0.3,0.2,0.4c0,0.1-0.1,0.3-0.2,0.5c-0.2,0.3-0.4,0.6-0.4,1.1c0,0.4,1.1,3.8,1.8,4.2c0.2,0.1,0.4,0.1,0.6,0.1c1.4,0,3.8-1.6,4.2-2c0.5-0.5,0.6-0.9,0.7-1.3c0-0.2,0.1-0.4,0.2-0.7c0.1-0.3,0.4-0.4,0.7-0.6c0.2-0.1,0.5-0.3,0.7-0.5c-0.1,0.4-0.2,0.9-0.1,1.2c0.1,0.4,0.5,0.6,0.9,0.6c0.8,0,1.7-1.5,1.9-2.3c0.2-0.8-0.3-1.1-0.5-1.2c-0.3-0.1-0.8-0.2-1.4,0.4c-0.1,0.1-0.3,0.3-0.4,0.5c0.1-0.4,0-0.8,0-1.2c0-0.4-0.1-0.8,0-1.1c0.2-0.6,0.5-1,0.9-1.5c0.2-0.2,0.4-0.5,0.6-0.8l0.1-0.1c0.8-1.2,1.3-2.1,0.9-2.7c-0.3-0.5-0.8-0.5-1.1-0.4c0.1,0,0.1-0.1,0.2-0.1c0.4-0.3,2.1-2.5,1.7-3.4c-0.4-0.9-1.2-0.8-1.6-0.8c-0.2,0-0.4,0.1-0.6,0c0,0-0.1,0-0.1-0.1c0.7-0.1,1.7,0,2.1,0.1c0.3,0.1,1,1.5,1.3,2.2c0.5,1,0.9,1.8,1.3,2.1C32.7,26.3,25.9,33,17.5,33zM27.8,27.3c0-0.2,0.1-0.6,0.4-1.1c0.2-0.3,0.3-0.4,0.4-0.4C28.6,26.2,28.1,27,27.8,27.3zM21.1,9.8c-0.2,0.2-0.3,0.5-0.5,0.7c0,0-0.1,0.1-0.1,0.1c-0.1-0.1-0.2-0.2-0.5-0.2c-0.3,0-0.5,0.1-0.7,0.5c-0.4-0.5-1-1-1-1l-0.2-0.1c-0.6-0.3-0.8-0.3-1.3,0c-0.1,0.1-0.2,0.1-0.4,0.2c-0.5,0.3-1.4,1.1-1.6,1.9c-0.1,0.3,0,0.5,0.1,0.7c0.6,0.8,1.6,0.4,2.2,0.1c0.1,0,0.3-0.1,0.4-0.2c0,0.3,0.1,0.6,0.4,0.9c0.2,0.2,2,0.9,2.4,0.9c0,0,0,0,0,0c0.8-0.1,0.9-0.7,1-0.9c0,0,0,0,0,0c0.1,0,0.2,0,0.4,0c0.2,0.1,0.5,0.1,0.8,0c1-0.2,1.3-0.4,1.2-1.3c0-0.1,0-0.2,0-0.4c0-0.2-0.1-0.3-0.2-0.4c-0.1-0.1-0.2-0.2-0.4-0.3c0,0,0-0.1,0.1-0.1c0,0,0.1-0.1,0.1-0.1c0.1,0,0.1-0.1,0.2-0.1c0.3-0.1,0.8-0.3,0.9-0.8c0-0.5-0.4-0.7-0.6-0.8C22.2,8.2,21.6,9.1,21.1,9.8zM22.3,11.6c0.1,0.2,0.3,0.3,0.5,0.4c0,0.1,0,0.1,0,0.2c0,0.1,0,0.1,0,0.2c-0.1,0-0.3,0.1-0.4,0.1c-0.1,0-0.2,0-0.3,0c-0.3-0.1-0.6-0.2-1,0c-0.4,0.2-0.5,0.6-0.6,0.8c0,0,0,0.1,0,0.1c0,0,0,0,0,0c0,0,0,0,0,0c-0.3,0-1.6-0.5-1.7-0.6c0-0.1-0.1-0.3-0.1-0.4c0-0.3-0.1-0.6-0.4-0.7c-0.1-0.1-0.2-0.1-0.4-0.1c-0.3,0-0.7,0.1-1,0.3c-0.3,0.1-0.8,0.3-1,0.2c0.1-0.3,0.7-0.9,1.1-1.1c0.2-0.1,0.4-0.2,0.5-0.3c0,0,0,0,0,0c0.1,0,0.2,0.1,0.3,0.1l0.1,0c0.1,0.1,0.6,0.7,0.8,0.9c0.1,0.1,0.2,0.2,0.3,0.3c0.4,0.1,0.7,0.1,0.9-0.1c0.1,0.1,0.1,0.2,0.2,0.2c0.1,0.1,0.3,0.1,0.6,0c0.6-0.2,0.6-0.6,0.6-0.8c0.3-0.3,0.5-0.6,0.7-0.9c0.4-0.6,0.5-0.8,1.2-0.5c-0.2,0.1-0.4,0.2-0.6,0.4c-0.3,0.3-0.4,0.6-0.4,1C22.2,11.4,22.2,11.5,22.3,11.6z"
                        :icon/google-play "M32.2,14.9L6.5,0.8c-0.7-0.4-1.6-0.4-2.4,0C3.4,1.3,3,2,3,2.9v29.3c0,0.9,0.4,1.6,1.2,2.1c0.4,0.2,0.8,0.3,1.2,0.3c0.4,0,0.8-0.1,1.2-0.3l25.6-14.7c0.9-0.4,1.4-1.3,1.4-2.3S33.1,15.4,32.2,14.9zM5.4,1.5c0.2,0,0.5,0.1,0.7,0.2l19,10.4l-4.8,4.8L4.8,1.7C5,1.6,5.2,1.5,5.4,1.5zM4.2,32.8C4.1,32.6,4,32.4,4,32.2V2.9c0-0.2,0-0.3,0.1-0.5l15.4,15.2L4.2,32.8zM6.1,33.4c-0.4,0.2-0.8,0.2-1.1,0.1l15.3-15.2l4.5,4.4L6.1,33.4zM31.7,18.7l-6.1,3.5l-4.7-4.6l5-5l5.8,3.2c0.5,0.3,0.9,0.8,0.9,1.4C32.6,17.8,32.3,18.4,31.7,18.7z"
                        :icon/group "M17,17c2.8,0,5-2.2,5-5s-2.2-5-5-5s-5,2.2-5,5S14.2,17,17,17zM17,8c2.2,0,4,1.8,4,4s-1.8,4-4,4s-4-1.8-4-4S14.8,8,17,8zM27,11c2.8,0,5-2.2,5-5s-2.2-5-5-5s-5,2.2-5,5S24.2,11,27,11zM27,2c2.2,0,4,1.8,4,4s-1.8,4-4,4s-4-1.8-4-4S24.8,2,27,2zM27,12c-3,0-5.7,3-6.6,7.3c-1-0.8-2.1-1.3-3.4-1.3c-1,0-1.9,0.3-2.8,0.9C13,15.2,10.7,13,8,13c-3.9,0-7,4.8-7,11c0,3.1,2.6,5,7,5c0.7,0,1.4-0.1,2-0.2c0,0.1,0,0.1,0,0.2c0,3.1,2.6,5,7,5s7-1.9,7-5c0-0.5,0-0.9-0.1-1.4c0.9,0.2,1.9,0.4,3.1,0.4c4.4,0,7-1.9,7-5C34,16.8,30.9,12,27,12zM8,28c-3.8,0-6-1.5-6-4c0-5.6,2.6-10,6-10c2.3,0,4.4,2.1,5.4,5.5c-1.8,1.7-3.1,4.7-3.3,8.3C9.4,27.9,8.7,28,8,28zM17,33c-3.8,0-6-1.5-6-4c0-5.6,2.6-10,6-10s6,4.4,6,10C23,31.5,20.8,33,17,33zM27,27c-1.2,0-2.3-0.2-3.2-0.5c-0.4-2.6-1.3-4.8-2.6-6.3C22,15.9,24.3,13,27,13c3.4,0,6,4.4,6,10C33,25.5,30.8,27,27,27zM8,12c2.8,0,5-2.2,5-5s-2.2-5-5-5S3,4.2,3,7S5.2,12,8,12zM8,3c2.2,0,4,1.8,4,4s-1.8,4-4,4S4,9.2,4,7S5.8,3,8,3z"
                        :icon/heart "M35,10.5c-0.1-2.2-0.9-4.3-2.4-6C30.5,2.2,27.6,1,24.8,1c-2.6,0-5.3,1-7.3,3h0c-2-2-4.6-3-7.3-3C7.4,1,4.5,2.2,2.5,4.5c-1.5,1.7-2.3,3.8-2.4,6c-0.1,1.7,0.2,3.5,0.9,5.1c0.4,1,1,2.3,1.9,3.5c0.8,1.1,6,8.4,13.8,13.3c0.3,0.2,0.6,0.3,0.9,0.3c0.3,0,0.6-0.1,0.9-0.3c7.8-4.9,12.9-12.2,13.8-13.3c0.9-1.2,1.5-2.5,1.9-3.5C34.8,14,35.1,12.2,35,10.5zM33.1,15.2c-0.4,0.9-1,2.2-1.8,3.3l-0.2,0.2c-5,6.8-10.1,10.8-13.4,12.9c-0.1,0.1-0.2,0.1-0.3,0.1c-0.1,0-0.2,0-0.3-0.1c-3.3-2.1-8.3-6-13.4-12.9l-0.2-0.2c-0.8-1.1-1.4-2.4-1.8-3.3c-0.7-1.4-0.9-3-0.8-4.6c0.1-2,0.9-3.9,2.2-5.4C5,3.2,7.5,2,10.2,2c2.5,0,4.8,1,6.6,2.7C17,4.9,17.2,5,17.5,5c0.3,0,0.5-0.1,0.7-0.3C20,3,22.3,2,24.8,2c2.7,0,5.2,1.2,7,3.2c1.3,1.5,2.1,3.3,2.2,5.4C34.1,12.2,33.8,13.8,33.1,15.2z"
                        :icon/heart-filled "M35,10.5c-0.1-2.2-0.9-4.3-2.4-6c-4-4.5-10.9-4.7-15-0.5c-2-2-4.7-3-7.3-3C7.4,1,4.5,2.2,2.5,4.5C1,6.2,0.2,8.4,0,10.5C-0.1,12.2,0.2,14,1,15.6c0.4,1,1,2.3,1.9,3.5c0.8,1.1,6,8.4,13.8,13.3c0.5,0.3,1.2,0.3,1.7,0c7.8-4.9,12.9-12.2,13.8-13.3c0.9-1.2,1.5-2.5,1.9-3.5C34.8,14,35.1,12.2,35,10.5zM13.6,13.5c-0.3,0.1-0.5-0.1-0.6-0.4c-0.2-0.7-0.8-1.1-1.5-1.1s-1.3,0.5-1.5,1.1c0,0.2-0.3,0.4-0.5,0.4c0,0-0.1,0-0.1,0c-0.3-0.1-0.4-0.3-0.4-0.6c0.3-1.1,1.3-1.9,2.4-1.9s2.1,0.8,2.4,1.9C14,13.1,13.9,13.4,13.6,13.5zM20.4,17.1C20.1,18.2,19.1,19,18,19s-2.1-0.8-2.4-1.9c-0.1-0.3,0.1-0.5,0.4-0.6c0.3-0.1,0.5,0.1,0.6,0.4c0.2,0.7,0.8,1.1,1.5,1.1s1.3-0.5,1.5-1.1c0.1-0.3,0.3-0.4,0.6-0.4C20.3,16.6,20.5,16.9,20.4,17.1zM26,13.5c0,0-0.1,0-0.1,0c-0.2,0-0.4-0.2-0.5-0.4c-0.2-0.7-0.8-1.1-1.5-1.1s-1.3,0.5-1.5,1.1c-0.1,0.3-0.3,0.4-0.6,0.4c-0.3-0.1-0.4-0.3-0.4-0.6c0.3-1.1,1.3-1.9,2.4-1.9s2.1,0.8,2.4,1.9C26.4,13.1,26.2,13.4,26,13.5z"
                        :icon/html5 "M6.2,7l2.2,25l10.1,2.8L28.7,32l2.2-25H6.2zM27.7,31.2l-9.2,2.6l-9.2-2.6L7.3,8h22.5L27.7,31.2zM29.5,6h-4V1h1.7v3.3h2.4V6zM14.9,2.6h-1.5V1H18v1.7h-1.5V6h-1.7V2.6zM20.6,6h-1.6V1h1.7l1.1,1.8L22.8,1h1.7v5h-1.7V3.5l-1.1,1.8h0l-1.2-1.8V6zM9.5,6H7.8V1h1.7v1.7H11V1h1.7v5H11V4.3H9.5V6zM11.1,12.5H26l-0.3,3H14.4l0.3,3h10.8l-0.8,9.1l-6.1,1.7l0,0l-6.1-1.7l-0.4-4.1h3l0.2,1.8l3.3,0.9l0,0v0l3.3-0.9l0.3-3.8H11.9L11.1,12.5z"
                        :icon/john-doe "M17.5,16C11.7,16,7,20.7,7,26.5V35h21v-8.5C28,20.7,23.3,16,17.5,16zM14.7,25.5c-0.1,0-0.1,0-0.2,0c-0.2,0-0.4-0.1-0.5-0.3c-0.2-0.4-0.6-0.7-1-0.7s-0.9,0.3-1,0.7c-0.1,0.3-0.4,0.4-0.6,0.3c-0.3-0.1-0.4-0.4-0.3-0.6c0.3-0.8,1.1-1.3,2-1.3s1.7,0.5,2,1.3C15.1,25.1,14.9,25.4,14.7,25.5zM19,28.1c-0.2,0.8-0.8,1.4-1.5,1.4s-1.3-0.6-1.5-1.4c0-0.1,0-0.3,0.1-0.4c0.1-0.1,0.2-0.2,0.4-0.2h2c0.2,0,0.3,0.1,0.4,0.2C19,27.8,19,28,19,28.1zM23.7,25.5c-0.1,0-0.1,0-0.2,0c-0.2,0-0.4-0.1-0.5-0.3c-0.2-0.4-0.6-0.7-1-0.7s-0.9,0.3-1,0.7c-0.1,0.3-0.4,0.4-0.6,0.3c-0.3-0.1-0.4-0.4-0.3-0.6c0.3-0.8,1.1-1.3,2-1.3s1.7,0.5,2,1.3C24.1,25.1,23.9,25.4,23.7,25.5z"
                        :icon/king "M35,16.5c0-0.8-0.4-1.6-1-2.1c-0.5-0.4-1.1-0.6-1.7-0.6c-0.7,0-1.8,0.3-2.7,1.7c-0.5-0.3-1.1-0.6-1.8-0.6c-0.7,0-1.5,0.3-2.1,0.9c-0.4,0.4-0.8,0.9-1.1,1.5c-0.2-0.6-0.5-1.1-0.8-1.5c-0.5-0.5-1.1-0.8-1.7-0.8c0,0,0,0,0,0c-0.5,0-1.4,0.2-2.2,1.2c-0.3-0.2-0.6-0.2-0.8-0.2c-0.1,0-0.1,0-0.2,0c1.5-1.9,1.9-3.7,1.9-4.9c0-0.1,0-0.1,0-0.2c0-0.4,0-0.8-0.1-1.3c-0.5-2-1.9-3.4-3.6-3.4h0c-0.1,0-0.3,0-0.4,0c-1.6,0.2-2.8,1.6-2.9,3.2c0,0.1,0,0.4,0,0.8c0.2,1.4,1.3,2.6,2.7,2.9C15,15,11.1,17,6.8,17.9l0,0c-0.2-0.7-0.5-1.4-0.8-1.9c-1.1-1.7-2.3-2-3.1-2c-0.4,0-0.8,0.1-1.2,0.3c-1,0.5-1.6,1.4-1.6,2.5l0,0l0,0.1l0,0L0,17l0,0.1c0,1.8,1.3,3.1,2.9,3.1c0,0,0.1,0,0.1,0c0.3,0.9,0.4,1.8,0.5,2.7c0,0.3,0,0.5,0.1,0.8c0.1,0.8,0.1,1.5,0.2,2.2c0.2,1.4,0.6,4.6,4.3,4.6c0.4,0,0.9,0,1.4-0.1c0.2,0,0.7-0.2,0.8-0.9c0.1-0.5,0-1-0.4-1.2l0,0l-0.1,0c-0.4-0.2-0.8-0.8-1.1-1.5c0.9,1.3,1.8,2,2.9,2c0,0,0.1,0,0.1,0c0,0.2,0.1,0.4,0.1,0.5l0.4,0.9l0.9-0.3c0,0,1.5-0.6,4.4-0.6c2.3,0,4.8,0.4,7.5,1c0.9,0.2,1.7,0.4,2.4,0.4c3.7,0,4-3.2,4.1-4.4c0.1-0.8,0.1-1.8,0.2-2.8c0-0.6,0-1.2,0.1-1.8c0.1-0.7,0.2-1.3,0.3-1.9C33.9,19.6,35,18.3,35,16.5L35,16.5L35,16.5zM9.4,29c0.1,0,0,0.3,0,0.3c-0.5,0.1-0.9,0.1-1.3,0.1c-2.4,0-3-1.6-3.3-3.8c-0.1-0.9-0.2-1.9-0.3-3c-0.1-1.3-0.3-2.6-0.9-3.7c-0.2,0.1-0.5,0.2-0.8,0.2c-1.1,0-1.9-0.9-1.9-2.1C1,17,1,17,1,17c0,0,0,0,0,0c0,0,0,0,0,0c0,0,0,0,0,0c0,0,0,0,0,0c0-0.7,0.4-1.4,1-1.7c0.3-0.1,0.5-0.2,0.8-0.2c0.8,0,1.6,0.5,2.3,1.6c0.3,0.4,0.5,0.9,0.7,1.6l0,0l0.3,1c0.2,0.6,0.5,2.1,0.7,3.3l0,0.1l0.1,0.1c0,0,0,0,0,0C7.3,25.5,7.9,28.2,9.4,29zM34,16.6c0,1.1-0.6,2.1-1.8,2.1c-0.2,0-0.4,0-0.6-0.1c-0.3,0.9-0.5,1.9-0.6,2.9c-0.1,1.6-0.1,3.2-0.3,4.6c-0.2,2-0.9,3.5-3.1,3.5c-0.6,0-1.3-0.1-2.2-0.3c-3.2-0.8-5.8-1.1-7.8-1.1c-3.2,0-4.8,0.6-4.8,0.6s-0.1-0.2,0-0.3c0.6-0.9,3.5-1.9,7.4-1.9c0.2,0,0.4,0,0.7,0c2.2,0.1,4,0.6,5.3,0.6c1.4,0,2.2-0.6,2.6-2.9c0,0,0-0.1,0-0.1c-0.5,0.8-1.4,1.3-2.1,1.3c-0.5,0-1-0.2-1.4-0.8c-0.2-0.3-0.3-0.8-0.4-1.3c-0.5,1-1.2,2.1-2.4,2.1c0,0-0.1,0-0.1,0c-0.8,0-1-0.6-1-1.7c0-0.1,0-0.2,0-0.3c0-0.8,0.1-2.4,0.1-3.2c0,0,0,0,0-0.1c0-0.8-0.3-1.1-0.5-1.1c-0.4,0-0.8,0.6-0.9,1.4c0,0.1,0,0.4,0,0.7c0,1.2,0,3.1,0,3.9c0,0.2,0,0.3,0,0.4c-0.4,0.1-0.7,0.1-0.9,0.1c-1.3,0-1.6-0.6-1.6-1.7c-0.6,1.7-1.5,2.4-2.2,2.4c-0.6,0-1.2-0.5-1.4-1.5c-0.3,1.4-1.2,2.8-2.1,2.8c-1.2,0-2.3-1.4-3.9-5.2c0-0.2-0.1-0.4-0.1-0.5c-0.2-1.1-0.4-2.1-0.6-3c5.6-1.3,10.8-4.2,11.3-7c-0.2,0.1-0.5,0.2-0.8,0.3c-0.1,0-0.2,0-0.3,0c-1.2,0-2.2-0.9-2.3-2.1c0-0.4,0-0.7,0-0.7l0,0c0-1.1,0.9-2.1,2-2.3c0.1,0,0.2,0,0.3,0c1.2,0,2.2,1,2.6,2.6c0.1,0.4,0.1,0.7,0.1,1.1c0,0.1,0,0.1,0,0.2c0,0,0,0,0,0c-0.1,3-2.5,6.8-9.8,9.7c1.2,2.2,2.1,3.8,2.8,3.8c0.1,0,0.1,0,0.2,0c0.5-0.2,0.5-1.1,0.3-4.4c0,0,1.2-0.8,2.6-1.4c0,3.4,0.2,4.9,0.7,4.9c0.1,0,0.1,0,0.2-0.1c0.5-0.4,0.5-1.5,0.5-3.3c0-1.4-0.1-2.2-0.1-2.2c0.7-0.7,1.3-1.1,1.7-1.1c0.5,0,0.8,0.5,0.8,1.5c0.5-1.7,1.4-2.5,2.1-2.5c1,0,1.8,1.2,1.7,3.2c-0.2,3.6-0.1,4.2,0.2,4.2c0.4,0,0.8-0.7,0.7-1.5h0c-0.1-2,0.4-4.3,1.6-5.3c0.5-0.5,1-0.6,1.5-0.6c1,0,1.7,0.8,2.1,1.3c0-0.1,0-0.1,0.1-0.2c0.7-1.6,1.6-2.2,2.3-2.2c0.4,0,0.8,0.2,1.1,0.4C33.8,15.5,34,16,34,16.6C34,16.5,34,16.5,34,16.6C34,16.6,34,16.6,34,16.6zM24.2,25.9c0.2-0.1,0.3-0.3,0.5-0.4c0.2,0.2,0.4,0.4,0.7,0.6C25,26,24.7,26,24.2,25.9C24.3,25.9,24.2,25.9,24.2,25.9zM12.5,22.8c-0.3-0.4-0.7-1-1-1.6c0.3-0.1,0.6-0.3,1-0.4C12.5,21.5,12.5,22.3,12.5,22.8zM28.4,17.9c-0.2,0-0.4,0.1-0.7,0.3c-0.6,0.6-0.8,1.8-0.8,2.9c0,0.5,0.1,1,0.2,1.3c0.1,0.4,0.5,0.5,0.8,0.5c0.4,0,0.9-0.3,1.1-0.8c0.1-1.1,0.2-2.4,0.4-3.5C29.3,18.3,28.9,17.9,28.4,17.9zM28.1,21.7l0,0.2c0,0-0.1,0.1-0.1,0.1c0-0.1-0.1-0.4-0.1-0.8c0-1,0.2-1.8,0.5-2.1C28.3,19.9,28.2,20.8,28.1,21.7z"
                        :icon/license-apache "M9.3,34c0.1-0.3,0.2-0.6,0.3-1c0.7-2.2,1.4-4.5,2.2-6.7c0,0,0-0.1,0-0.1c-0.8-0.1-1.5,0.1-2.1,0.4c0.5-0.4,1.1-0.8,1.7-0.9c-0.6-0.3-1-0.5-1.2-0.8c0.1,0,0.2,0.1,0.2,0.1c0.4,0.1,0.8,0.2,1.2,0.2c0.1,0,0.1,0,0.1-0.1c-0.1-0.7-0.4-1.4-0.7-2.1c-0.2-0.3-0.4-0.6-0.6-1c0.5,0.3,1,0.7,1.4,1.2c0-0.2,0-0.4,0.1-0.6c0.1-0.9,0.3-1.9,0.5-2.8c0.5-2,1.2-3.9,2.1-5.8c1.5-3.2,3.4-6.2,5.5-9.1c0.7-1,1.5-2,2.4-2.8c0.3-0.3,0.6-0.5,0.9-0.8c0.1-0.1,0.3-0.2,0.5-0.2c0.1,0,0.2,0,0.3,0c0.3,0.1,0.5,0.2,0.7,0.4c0.5,0.5,0.8,1,0.9,1.7c0,0.2,0,0.3,0.1,0.5c0,0.2,0,0.4,0,0.5c0,0.2,0,0.3-0.1,0.5c-0.1,0.8-0.3,1.5-0.6,2.2c-0.5,1.1-1,2.1-1.7,3c-0.4,0.6-1,1.1-1.6,1.4c0,0-0.1,0-0.1,0.1c0.5-0.1,1.1-0.2,1.6-0.5c-0.1,0.3-0.2,0.6-0.3,0.8c-0.2,0.4-0.4,0.9-0.6,1.3c-0.4,0.7-1,1.2-1.7,1.5c0,0-0.1,0-0.1,0.1c0.2,0,0.7-0.2,1.5-0.6c-0.5,1-1.2,1.8-2.2,2.2c0.4,0,0.8-0.2,1.2-0.3c-0.1,0.3-0.2,0.6-0.3,0.9c-0.4,1.1-0.9,2.2-1.5,3.2c-0.9,1.4-2.1,2.5-3.7,3.1c-0.3,0.1-0.6,0.2-1,0.3c1.4,0.2,2.6-0.2,3.6-1.2c-0.6,1.3-1.5,2.1-2.7,2.6c0.4,0.2,1.1-0.1,1.6-0.6c-0.1,0.3-0.3,0.6-0.5,0.8c-0.5,0.6-1.2,0.9-1.9,1.1c-0.6,0.1-1.2,0.2-1.7,0.1c0,0,0,0-0.1,0c0.4,0.4,0.6,0.8,0.6,1.3c-0.3-0.4-0.5-0.8-0.9-1.1c0,0,0-0.1,0-0.1c1.5-4.4,3.2-8.7,5.1-13c1.1-2.4,2.4-4.8,3.8-7.1c0.5-0.8,1.1-1.6,1.7-2.5c0,0,0-0.1,0-0.1c0,0-0.1,0.1-0.1,0.1c-1,1.3-1.9,2.6-2.7,4c-1.7,2.8-3,5.7-4.3,8.7c-1.7,3.9-3.2,7.8-4.5,11.8c-0.6,1.7-1.1,3.5-1.7,5.2C9.7,33.7,9.6,33.9,9.3,34C9.3,34,9.3,34,9.3,34z"
                        :icon/license-cc-universal "M16.7,1c0.5,0,1,0,1.5,0c0.4,0,0.7,0.1,1.1,0.1c1.6,0.2,3.1,0.5,4.6,1.2c2.4,1,4.4,2.5,6,4.4c2.3,2.7,3.6,5.8,3.9,9.3c0.1,1.4,0.1,2.8-0.1,4.2c-0.3,1.7-0.8,3.4-1.5,4.9c-1.4,2.8-3.6,5-6.3,6.6c-3.2,1.9-6.6,2.6-10.2,2.3c-1.6-0.2-3.1-0.5-4.6-1.2c-3.7-1.6-6.5-4.2-8.4-7.8c-0.9-1.8-1.5-3.7-1.7-5.7c0-0.3-0.1-0.7-0.1-1c0-0.5,0-1,0-1.5c0-0.2,0-0.4,0.1-0.7c0.1-1.6,0.5-3.1,1.1-4.6c1.1-2.9,3-5.3,5.4-7.2c2.5-2,5.3-3,8.4-3.2C16.2,1,16.5,1,16.7,1zM31,17.4c0-0.8-0.1-1.7-0.3-2.7c-1-4.4-3.5-7.5-7.5-9.5c-1.6-0.8-3.3-1.1-5-1.2c-1.4-0.1-2.7,0.1-4,0.4C11.8,5,9.7,6.2,8,7.9c-2.3,2.4-3.7,5.2-4,8.5c-0.2,1.8,0,3.6,0.6,5.3c0.9,2.7,2.5,4.9,4.8,6.6c3,2.2,6.3,3,9.9,2.6c1.9-0.2,3.7-0.9,5.3-1.9c2.1-1.3,3.7-3,4.9-5.2C30.6,21.8,31,19.7,31,17.4zM10.7,17.1c0-1.1,0.1-2.5,0.6-4c0.3-1.1,0.8-2.2,1.5-3c0.8-1,1.9-1.6,3.2-1.9c1.3-0.3,2.5-0.2,3.8,0.2c1.4,0.5,2.5,1.5,3.2,2.8c0.6,1,0.9,2.2,1.1,3.3c0.4,2.1,0.3,4.3-0.1,6.4c-0.2,1.3-0.7,2.5-1.4,3.6c-1,1.5-2.4,2.3-4.2,2.5c-1.1,0.1-2.3,0-3.3-0.4c-1.4-0.5-2.4-1.6-3.1-2.9c-0.5-1-0.8-2.2-1-3.3C10.7,19.4,10.7,18.4,10.7,17.1zM20.2,14.7C20.2,14.7,20.2,14.7,20.2,14.7c-0.1,0-0.1,0.1-0.1,0.1c-1.4,2.3-2.7,4.7-4.1,7C16,21.9,16,22,15.9,22.1c-0.1,0.4,0,0.7,0.3,0.9c0.2,0.2,0.5,0.3,0.8,0.3c1.1,0.2,2-0.2,2.6-1.1c0.5-0.8,0.7-1.7,0.8-2.6c0.1-1.2,0.1-2.5,0.1-3.7C20.4,15.5,20.3,15.1,20.2,14.7zM14.7,19.7C14.7,19.7,14.7,19.7,14.7,19.7c0.1,0,0.1-0.1,0.1-0.1c1.1-2,2.1-3.9,3.2-5.9c0.1-0.3,0.3-0.5,0.4-0.8c0.2-0.4,0.1-0.8-0.2-1.2c-0.1-0.1-0.2-0.1-0.2-0.1c-1.1-0.2-2,0.2-2.5,1.2c-0.5,0.8-0.7,1.7-0.8,2.6c-0.1,1.1-0.1,2.3-0.1,3.4C14.6,19.2,14.7,19.4,14.7,19.7z"
                        :icon/license-gnu-gpl "M31.8,13.5c0.6,0.3,0.8,0.8,0.6,1.4c-0.1,0.6-0.4,1-0.8,1.5c-0.6,0.8-1.3,1.4-2,2.1c-1.5,1.2-3.1,2.2-4.9,2.9c-0.8,0.3-1.7,0.6-2.5,0.7c-0.8,0.1-1.5,0.1-2.3-0.2c-0.2-0.1-0.4-0.1-0.6-0.1c-5.7,0-11.4,0-17.1,0c-0.1,0-0.1,0-0.2,0c0.1-0.5,0.2-0.9,0.3-1.3c0.8-3.5,1.6-6.9,2.4-10.4c0-0.1,0.1-0.1,0.2-0.1c4.6,0,9.2,0,13.9,0c3.7,0,7.3,0,11,0c0.1,0,0.2,0,0.2,0c0.6-0.2,1.1-0.4,1.7-0.4c0.4,0,0.7-0.1,1.1,0c0.7,0.2,1.1,0.8,1,1.5c-0.1,0.6-0.4,1.1-0.8,1.5c-0.3,0.3-0.6,0.7-1,1C31.9,13.5,31.9,13.5,31.8,13.5zM25.9,13.6c0.3-0.2,0.6-0.4,0.9-0.6c0.7-0.4,1.4-0.8,2.2-1c0.3-0.1,0.6-0.1,0.8-0.1c0.2,0,0.3,0.2,0.3,0.4c-0.1,0.2-0.1,0.4-0.3,0.6c-0.5,0.8-1.2,1.4-2,1.9c-0.4,0.3-0.8,0.6-1.2,0.9c-0.2,0.2-0.4,0.3-0.6,0.5c0,0,0,0,0,0c0.1,0,0.1-0.1,0.2-0.1c0.7-0.3,1.4-0.6,2.1-0.7c0.3-0.1,0.7-0.2,1-0.1c0.3,0,0.4,0.2,0.2,0.5c-0.2,0.2-0.3,0.5-0.6,0.6c-1.6,1.3-3.4,2.2-5.4,2.6c-0.6,0.1-1.2,0.2-1.9,0c-0.7-0.2-1.1-0.7-1.1-1.4c0-0.3,0.1-0.6,0.2-0.9c0.3-0.8,0.8-1.5,1.4-2.1c1.6-1.8,3.4-3.1,5.6-4.1c0.3-0.2,0.7-0.3,1-0.4c0,0-0.1,0-0.1,0c-0.4,0.2-0.9,0.3-1.3,0.5c-2.2,1-4.2,2.4-5.9,4.1c-0.8,0.9-1.6,1.8-2.1,2.9c-0.3,0.6-0.5,1.3-0.5,2c0,0.8,0.4,1.4,1.2,1.7c0.6,0.3,1.2,0.3,1.8,0.2c0.9-0.1,1.7-0.3,2.5-0.6c1.8-0.7,3.4-1.6,4.8-2.8c0.8-0.6,1.5-1.3,2.1-2.1c0.3-0.4,0.6-0.9,0.7-1.4c0.1-0.5-0.1-0.9-0.7-0.9c0,0-0.1,0-0.1,0c-0.1,0-0.3,0-0.4,0c0.1-0.1,0.2-0.1,0.2-0.2c0.6-0.4,1.1-0.9,1.5-1.4c0.3-0.4,0.6-0.8,0.7-1.3c0.1-0.6-0.1-1-0.7-1.1c-0.1,0-0.2,0-0.3,0c-0.4,0-0.8,0-1.2,0.1c-0.2,0.1-0.4,0.1-0.6,0.2c-1.3,0.4-2.4,1.1-3.4,2C26.6,12.4,26.2,13,25.9,13.6zM9.2,13.4c0.1-0.5,0.2-1,0.3-1.5c0-0.2,0-0.3-0.2-0.4c-0.2-0.1-0.4-0.1-0.5-0.1c-1,0-1.9,0-2.9,0c-0.2,0-0.4,0-0.6,0.1C5.1,11.6,5,11.7,5,11.9c-0.4,1.9-0.8,3.7-1.2,5.6c-0.1,0.3,0,0.5,0.3,0.6c0.1,0,0.1,0,0.2,0c1.1,0,2.1,0,3.2,0c0,0,0,0,0.1,0C8,18,8.3,17.8,8.4,17.4c0.1-0.6,0.2-1.1,0.4-1.7c0.1-0.3,0.1-0.7,0.2-1c0,0,0,0-0.1,0c-0.7,0-1.4,0-2.1,0c0,0-0.1,0-0.1,0c0,0.2-0.1,0.3-0.1,0.5c0.4,0,0.8,0,1.2,0c0,0.1,0,0.1,0,0.1c-0.2,0.7-0.3,1.4-0.5,2c0,0.1-0.1,0.1-0.2,0.1c-0.8,0-1.5,0-2.3,0c0,0-0.1,0-0.1,0c0,0,0-0.1,0-0.1C5.2,15.6,5.6,13.8,6,12c0-0.1,0.1-0.1,0.2-0.1c0.8,0,1.5,0,2.3,0c0,0,0.1,0,0.1,0c-0.1,0.5-0.2,1-0.3,1.5C8.6,13.4,8.9,13.4,9.2,13.4zM10.5,15.4c0.1,0,0.1,0,0.2,0c0.8,0,1.6,0,2.4,0c0.1,0,0.2,0,0.4,0c0.3-0.1,0.6-0.2,0.7-0.6c0.2-1,0.4-2,0.6-3c0-0.2,0-0.3-0.2-0.4c-0.2-0.1-0.4-0.1-0.5-0.1c-1.2,0-2.3,0-3.5,0c-0.1,0-0.1,0-0.2,0.1c-0.4,2-0.9,4-1.3,5.9c-0.1,0.2-0.1,0.5-0.2,0.7c0.3,0,0.6,0,0.9,0c0.1,0,0.1,0,0.1-0.1c0.1-0.4,0.2-0.8,0.3-1.2C10.3,16.4,10.4,15.9,10.5,15.4zM18.7,19.1C18.7,19.1,18.7,19.1,18.7,19.1c0-0.1,0.1-0.3,0.1-0.4c0.2-0.9,0.6-1.6,1.1-2.3c0-0.1,0.1-0.1,0-0.2c-0.4-1.2-0.7-2.5-1-3.8c-0.1-0.8-0.2-1.5-0.2-2.3c0-0.1,0-0.1,0-0.2c0,0,0,0,0,0c0,0,0,0.1,0,0.1c-0.4,1.1-0.6,2.2-0.7,3.3c-0.1,1.8,0.1,3.6,0.5,5.3C18.6,18.8,18.6,19,18.7,19.1zM15.1,17.6c0.5-2.1,0.9-4.1,1.4-6.2c-0.3,0-0.7,0-1,0c-0.5,2.3-1,4.5-1.5,6.7c1.2,0,2.4,0,3.6,0c0-0.1-0.1-0.3-0.1-0.4c0-0.1-0.1-0.1-0.1-0.1c-0.7,0-1.4,0-2.1,0C15.2,17.6,15.2,17.6,15.1,17.6zM8.4,19.6c-0.1,0.2,0,0.4,0.2,0.5c0.3,0.2,0.8,0.1,1-0.2c0.2-0.3,0.1-0.5-0.2-0.6c-0.1,0-0.3-0.1-0.4-0.1c-0.1,0-0.2-0.1,0-0.2c0.1-0.1,0.3-0.1,0.4-0.1c0.1,0,0.1,0.1,0.2,0.2c0.1,0,0.1,0,0.2,0c0-0.2,0-0.3-0.1-0.4c-0.3-0.2-0.8-0.1-1,0.2c-0.1,0.2,0,0.5,0.2,0.5c0.1,0,0.3,0.1,0.4,0.1c0.1,0,0.1,0.1,0.1,0.1c0,0.1,0,0.1-0.1,0.2C9,20,8.7,19.9,8.7,19.6C8.6,19.6,8.5,19.6,8.4,19.6zM13,19.6C13,19.6,13,19.6,13,19.6c0-0.2-0.1-0.4-0.1-0.6c-0.1,0-0.2,0-0.3,0c0,0.4,0.1,0.8,0.1,1.2c0.3,0,0.3,0,0.4-0.2c0.1-0.1,0.1-0.3,0.2-0.4c0,0,0,0,0,0c0,0.2,0,0.4,0,0.6c0.3,0,0.3,0,0.4-0.2c0.1-0.2,0.2-0.4,0.3-0.6c0.1-0.1,0.1-0.2,0.2-0.4c-0.3,0-0.3,0-0.4,0.2c-0.1,0.2-0.2,0.3-0.2,0.5c0,0,0,0,0,0c0-0.2,0-0.4,0-0.6c-0.3,0-0.3,0-0.4,0.2C13.1,19.3,13.1,19.5,13,19.6zM14.9,20.1c0.1,0.1,0.2,0,0.3,0c0-0.2,0-0.3,0.1-0.5c0-0.1,0.1-0.3,0.1-0.4c0-0.2,0-0.3-0.2-0.3c-0.2,0-0.4,0-0.5,0.1c-0.1,0.1-0.2,0.2-0.3,0.4c0.1,0,0.3,0,0.3-0.1c0.1-0.1,0.2-0.1,0.3-0.1c0,0,0.1,0.1,0.1,0.1c0,0-0.1,0.1-0.1,0.1c-0.1,0-0.3,0-0.4,0.1c-0.2,0-0.3,0.1-0.3,0.3c-0.1,0.2,0,0.4,0.2,0.4C14.5,20.2,14.7,20.2,14.9,20.1zM3.6,19.2c0-0.1,0-0.1,0-0.2c0-0.1,0.1-0.2,0.2-0.1c0.2,0,0.4,0,0.6,0c0.1,0,0.1,0,0.1-0.1c0-0.1,0-0.1,0-0.2c-0.4,0-0.7,0-1,0c0,0-0.1,0.1-0.1,0.1c0,0.2-0.1,0.3-0.1,0.5c-0.1,0.3-0.1,0.6-0.2,1c0.1,0,0.1,0,0.2,0c0.1,0,0.2,0,0.2-0.1c0-0.1,0.1-0.3,0.1-0.4c0-0.1,0-0.1,0.1-0.1c0.2,0,0.3,0,0.5,0c0.1,0,0.1,0,0.1-0.1c0-0.1,0-0.1,0-0.2C4.1,19.2,3.9,19.2,3.6,19.2zM6.2,19.8c-0.1,0-0.3,0-0.4,0.1c-0.1,0.1-0.2,0.1-0.3,0c-0.1,0-0.1-0.1-0.1-0.2C5.7,19.7,5.9,19.7,6.2,19.8c0-0.1,0-0.1,0-0.1c0-0.2,0.1-0.3-0.1-0.5C6.1,19,5.9,19,5.8,19C5.3,19,5,19.5,5.2,19.9c0,0.1,0.1,0.2,0.2,0.2C5.6,20.3,6.1,20.1,6.2,19.8zM7.5,19.7c0.1-0.2,0.1-0.4-0.1-0.6C7.3,18.9,7.1,19,6.9,19c-0.3,0.1-0.5,0.6-0.4,0.9c0,0.1,0.1,0.2,0.2,0.2C6.9,20.3,7.4,20.1,7.5,19.7c-0.2,0.1-0.3,0.1-0.4,0.2C7,20,6.9,20,6.8,19.9c-0.1-0.1-0.1-0.1-0.1-0.2C7,19.7,7.2,19.7,7.5,19.7zM16.5,19.7C16.6,19.7,16.6,19.7,16.5,19.7c0.3,0,0.5,0,0.7,0c0,0,0.1,0,0.1,0c0.2-0.3,0-0.6-0.3-0.7c-0.2,0-0.4,0.1-0.6,0.3c-0.2,0.2-0.2,0.5-0.1,0.7c0,0,0,0.1,0.1,0.1c0.2,0.2,0.4,0.2,0.6,0.1c0.2-0.1,0.3-0.2,0.4-0.4c-0.2,0-0.2,0-0.4,0.1c-0.1,0-0.2,0.1-0.3,0C16.6,19.9,16.6,19.8,16.5,19.7zM11,19.4c0-0.3-0.2-0.5-0.5-0.4c-0.3,0-0.6,0.3-0.6,0.7c-0.1,0.4,0.2,0.6,0.5,0.5C10.8,20.1,11,19.8,11,19.4zM11.7,19.3c0.1,0,0.1,0,0.2,0c0-0.1,0-0.2,0.1-0.2c-0.1,0-0.1,0-0.2,0c0-0.1,0-0.2,0.2-0.1c0.1,0,0.1,0,0.1-0.1c0,0,0-0.1,0.1-0.2c-0.2,0-0.3,0-0.4,0c-0.2,0-0.2,0.2-0.3,0.4c-0.1,0-0.1,0-0.2,0c0,0.1,0,0.2-0.1,0.2c0.1,0,0.1,0,0.2,0c-0.1,0.3-0.1,0.6-0.2,0.9c0.1,0,0.2,0,0.3,0C11.5,19.9,11.6,19.6,11.7,19.3zM12.5,18.6c-0.2,0.1-0.4,0.2-0.4,0.4c-0.1,0-0.2,0.2-0.2,0.3c0,0,0.1,0,0.1,0c0,0.2-0.1,0.4-0.1,0.6c-0.1,0.3,0,0.4,0.3,0.3c0,0,0.1,0,0.1,0c0-0.1,0-0.2,0-0.2c-0.1,0-0.1,0-0.2,0c0-0.2,0.1-0.4,0.1-0.7c0.1,0,0.1,0,0.2,0c0-0.1,0-0.2,0.1-0.2c-0.1,0-0.1,0-0.2,0C12.4,18.9,12.4,18.7,12.5,18.6zM16.4,19.1c-0.1,0-0.2-0.1-0.3-0.1c-0.1,0-0.2,0-0.3,0c-0.1,0-0.1,0-0.1,0.1c-0.1,0.3-0.1,0.7-0.2,1c0,0,0,0.1,0,0.1c0.1,0,0.2,0,0.3,0c0.1-0.2,0.1-0.5,0.2-0.7c0.1-0.2,0.1-0.3,0.4-0.2C16.2,19.2,16.3,19.2,16.4,19.1zM4.6,20.2c0.1-0.2,0.1-0.4,0.2-0.7c0.1-0.2,0.1-0.3,0.4-0.2c0-0.1,0.1-0.2,0.1-0.3C5.1,19,5.1,19,5,19c-0.1,0-0.3,0-0.5,0c-0.1,0.4-0.2,0.8-0.3,1.2C4.4,20.2,4.5,20.2,4.6,20.2zM30.8,25.7c0.2,0,0.3,0,0.4,0c0.1,0,0.1,0,0.1-0.1c0.1-0.3,0.1-0.6,0.2-0.9c0.1-0.2,0.1-0.4,0.3-0.5c0.1,0,0.2-0.1,0.3,0c0.1,0,0.1,0.1,0.1,0.2c0,0.1,0,0.3-0.1,0.4c-0.1,0.3-0.1,0.6-0.2,0.9c0.1,0,0.1,0,0.2,0c0.4,0,0.4,0,0.4-0.3c0.1-0.3,0.1-0.6,0.3-0.9c0-0.1,0.1-0.2,0.2-0.3c0.1,0,0.2,0,0.2,0c0.1,0,0.1,0.1,0.1,0.2c0,0.1,0,0.1,0,0.2c-0.1,0.4-0.2,0.7-0.3,1.1c0.2,0,0.3,0,0.5,0c0.1,0,0.1,0,0.1-0.1c0.1-0.4,0.2-0.7,0.2-1.1c0-0.1,0-0.3,0-0.4c0-0.3-0.2-0.4-0.4-0.4c-0.2,0-0.4,0.1-0.6,0.2c0,0-0.1,0.1-0.1,0.1c-0.2-0.5-0.6-0.5-1.2,0c0-0.1,0-0.2,0-0.3c-0.2,0-0.3,0-0.5,0C31.1,24.4,31,25,30.8,25.7zM27.8,23.7c0.1-0.3,0.1-0.6,0.2-0.9c0.2,0,0.4,0,0.6,0c-0.2,1-0.4,1.9-0.7,2.9c-0.1,0-0.3,0-0.5,0c0-0.1,0-0.2,0-0.3c0,0,0,0,0,0c0,0,0,0.1-0.1,0.1c-0.2,0.2-0.4,0.3-0.6,0.2c-0.3,0-0.5-0.2-0.5-0.5c-0.1-0.7,0.5-1.6,1.5-1.5C27.8,23.7,27.8,23.7,27.8,23.7zM26.8,24.9c0.1,0.1,0.1,0.2,0.2,0.3c0.1,0.1,0.2,0.1,0.3,0c0.1-0.1,0.2-0.2,0.2-0.3c0.1-0.2,0.1-0.4,0.2-0.6c0,0,0-0.1-0.1-0.1c-0.2,0-0.4,0-0.5,0.1C27,24.5,26.9,24.7,26.8,24.9zM11.7,23.7c-0.1,0.7-0.3,1.3-0.4,2c-0.1,0-0.3,0-0.5,0c0-0.1,0-0.3,0-0.4c-0.1,0.1-0.1,0.1-0.2,0.2c-0.1,0.1-0.3,0.2-0.5,0.3c-0.4,0-0.6-0.2-0.6-0.6c0-0.8,0.6-1.4,1.4-1.4C11.2,23.7,11.4,23.7,11.7,23.7zM11,24.1c-0.5,0-0.9,0.4-0.9,0.9c0,0.1,0.1,0.2,0.2,0.2c0.2,0,0.3-0.1,0.4-0.3C10.9,24.7,10.9,24.4,11,24.1zM18.5,25.7c0.2,0,0.3,0,0.5,0c0.1,0,0.1,0,0.1-0.1c0.1-0.3,0.1-0.6,0.2-0.9c0-0.1,0.1-0.1,0.2-0.1c0.2,0,0.5,0,0.7,0c0,0,0.1,0,0.1-0.1c0-0.1,0.1-0.3,0.1-0.4c-0.3,0-0.6,0-0.9,0c0.1-0.2,0.1-0.5,0.2-0.7c0.3,0,0.7,0,1,0c0-0.2,0.1-0.4,0.1-0.5c-0.5,0-1.1,0-1.6,0C18.9,23.8,18.7,24.8,18.5,25.7zM1.5,25.7c0.1,0,0.1,0,0.1-0.1c0.1-0.3,0.1-0.6,0.2-0.9c0-0.1,0.1-0.1,0.1-0.1c0.2,0,0.5,0,0.7,0c0,0,0.1,0,0.1-0.1c0-0.1,0.1-0.2,0.1-0.4c-0.3,0-0.6,0-0.9,0c0.1-0.2,0.1-0.5,0.2-0.7c0.3,0,0.7,0,1,0c0-0.2,0.1-0.4,0.1-0.5c-0.5,0-0.9,0-1.3,0c-0.3,0-0.3,0-0.3,0.2C1.4,24,1.2,24.8,1,25.7C1.2,25.7,1.3,25.7,1.5,25.7zM29.4,25.7c-0.4,0-0.7-0.2-0.8-0.5c-0.1-0.6,0.3-1.5,1.2-1.5c0.6,0,0.9,0.4,0.8,1C30.5,25.3,30,25.7,29.4,25.7zM29.2,24.9c0,0.2,0.1,0.3,0.3,0.3c0.1,0,0.2,0,0.3-0.1c0.2-0.2,0.3-0.4,0.3-0.7c0-0.2-0.2-0.4-0.4-0.3c-0.1,0-0.1,0.1-0.2,0.2C29.3,24.5,29.2,24.7,29.2,24.9zM15.9,23.7c-0.1,0-0.1,0-0.1,0.1c-0.1,0.4-0.2,0.8-0.3,1.2c0,0.2-0.1,0.4-0.1,0.6c0.2,0,0.3,0,0.5,0c0.1,0,0.1,0,0.1-0.1c0.1-0.2,0.1-0.5,0.2-0.7c0-0.1,0.1-0.3,0.2-0.4c0.1-0.1,0.2-0.2,0.4-0.2c0.2,0,0.2,0.1,0.2,0.2c0,0.1,0,0.2-0.1,0.3c-0.1,0.3-0.2,0.6-0.2,1c0.2,0,0.3,0,0.5,0c0.1,0,0.1,0,0.1-0.1c0.1-0.3,0.2-0.7,0.2-1c0-0.1,0.1-0.3,0.1-0.4c0-0.2-0.1-0.4-0.4-0.4c-0.3,0-0.5,0.1-0.7,0.3c0,0-0.1,0.1-0.1,0.1c0-0.1,0-0.2,0-0.3C16.2,23.8,16,23.8,15.9,23.7zM7.3,24.9c0,0.2,0,0.3,0.3,0.3c0.2,0,0.5,0,0.7,0c0,0.1,0,0.2,0,0.4c-0.4,0.1-0.7,0.1-1.1,0c-0.3-0.1-0.4-0.3-0.5-0.5c-0.1-0.6,0.3-1.3,1-1.4c0.1,0,0.3,0,0.4,0c0.2,0,0.4,0.2,0.4,0.4c0.1,0.3,0,0.5-0.2,0.6c-0.2,0.2-0.5,0.2-0.8,0.2C7.5,24.9,7.4,24.9,7.3,24.9zM7.4,24.5c0.1,0,0.2,0,0.3,0c0.1,0,0.2,0,0.3-0.1c0,0,0.1-0.1,0.1-0.1c0,0,0-0.1-0.1-0.1C7.8,24.1,7.5,24.2,7.4,24.5zM22.7,24.9c0,0.2,0,0.3,0.2,0.3c0.2,0,0.5,0,0.7,0c0,0.1,0,0.2,0,0.4c-0.4,0.1-0.7,0.1-1.1,0c-0.2-0.1-0.4-0.2-0.4-0.5c-0.1-0.6,0.3-1.3,1-1.5c0.1,0,0.3,0,0.4,0c0.3,0,0.4,0.2,0.4,0.4c0,0.2,0,0.5-0.3,0.6c-0.2,0.2-0.5,0.2-0.8,0.2C22.9,24.9,22.8,24.9,22.7,24.9zM22.8,24.5c0.1,0,0.2,0,0.3,0c0.1,0,0.2,0,0.3-0.1c0,0,0.1-0.1,0.1-0.1c0,0,0-0.1-0.1-0.1C23.1,24.1,22.9,24.2,22.8,24.5zM24.8,24.9c0,0.2,0.1,0.3,0.3,0.3c0.2,0,0.5,0,0.7,0c0,0.1,0,0.2,0,0.3c0,0,0,0.1-0.1,0.1c-0.3,0-0.6,0.1-0.9,0c-0.4,0-0.6-0.3-0.6-0.7c0-0.6,0.4-1.2,1-1.3c0.1,0,0.3,0,0.4,0c0.3,0,0.4,0.2,0.5,0.4c0,0.2-0.1,0.5-0.3,0.6c-0.2,0.1-0.5,0.2-0.8,0.2C24.9,24.9,24.8,24.9,24.8,24.9zM24.8,24.5c0.2,0,0.4,0,0.6-0.1c0,0,0.1-0.1,0.1-0.1c0,0,0-0.1-0.1-0.1C25.2,24.1,24.9,24.2,24.8,24.5zM6.2,25.2c0,0,0,0.1,0,0.2c0,0.2,0,0.2-0.2,0.3c-0.3,0-0.5,0-0.8,0c-0.3,0-0.5-0.2-0.5-0.5c-0.1-0.7,0.4-1.6,1.3-1.5c0.3,0,0.5,0.2,0.5,0.4c0.1,0.3,0,0.5-0.3,0.7c-0.2,0.1-0.5,0.2-0.8,0.2c-0.1,0-0.2,0-0.2,0c0,0.2,0,0.3,0.2,0.3C5.7,25.2,6,25.2,6.2,25.2zM5.3,24.5c0.1,0,0.2,0,0.3,0c0.1,0,0.2,0,0.3-0.1c0,0,0.1-0.1,0.1-0.1c0,0,0-0.1-0.1-0.1C5.7,24.1,5.4,24.2,5.3,24.5zM12.9,25.6c0.2-0.1,0.4-0.3,0.4-0.5c0-0.2-0.1-0.4-0.3-0.5c-0.1-0.1-0.2-0.1-0.3-0.2c0,0-0.1-0.1-0.1-0.2c0,0,0.1-0.1,0.1-0.1c0.2,0,0.3,0,0.5,0c0-0.1,0.1-0.3,0.2-0.4c-0.4-0.1-0.8-0.1-1.1,0.1c-0.4,0.3-0.4,0.7,0,1c0.1,0,0.2,0.1,0.2,0.1c0,0,0.1,0.1,0.1,0.2c-0.1,0-0.1,0.1-0.2,0.1c-0.2,0-0.4,0-0.6,0c0,0.1-0.1,0.3-0.2,0.4C12.1,25.7,12.5,25.8,12.9,25.6zM3.9,24.7c0.1-0.3,0.3-0.6,0.7-0.5c0-0.2,0.1-0.4,0.1-0.5c-0.4,0-0.6,0.2-0.8,0.5c0-0.2,0-0.3,0-0.4c-0.2,0-0.3,0-0.5,0C3.3,24.4,3.2,25,3,25.7c0.2,0,0.4,0,0.6,0C3.7,25.4,3.8,25,3.9,24.7zM21,23.7c-0.1,0-0.1,0-0.1,0.1c-0.1,0.3-0.1,0.6-0.2,0.9c-0.1,0.3-0.1,0.6-0.2,0.9c0,0,0,0,0,0c0.1,0,0.2,0,0.3,0c0.2,0,0.2,0,0.3-0.2c0.1-0.3,0.1-0.6,0.2-0.8c0.1-0.3,0.3-0.5,0.7-0.5c0-0.2,0.1-0.3,0.1-0.5c-0.4,0-0.6,0.2-0.8,0.4c0-0.1,0-0.2,0-0.4C21.3,23.8,21.1,23.8,21,23.7zM14.8,23.7c-0.1,0-0.1,0-0.1,0.1c-0.1,0.4-0.2,0.8-0.3,1.2c0,0.2-0.1,0.4-0.1,0.6c0.2,0,0.3,0,0.5,0c0.1,0,0.1,0,0.1-0.1c0.1-0.6,0.3-1.1,0.4-1.7c0,0,0-0.1,0-0.2C15.1,23.8,15,23.8,14.8,23.7zM15.2,22.8c-0.2,0-0.4,0.1-0.4,0.3c0,0.2,0.1,0.4,0.3,0.4c0.2,0,0.4-0.2,0.4-0.4C15.5,22.9,15.4,22.8,15.2,22.8zM13.7,11.9c-0.8,0-1.5,0-2.3,0c-0.1,0-0.1,0-0.1,0.1c-0.2,0.7-0.3,1.4-0.5,2.1c-0.1,0.3-0.1,0.5-0.2,0.8c0.8,0,1.7,0,2.5,0C13.4,13.9,13.6,12.9,13.7,11.9C13.8,11.9,13.8,11.9,13.7,11.9zM14.5,19.8c0,0-0.1,0.1-0.1,0.1c0,0,0,0.1,0.1,0.1c0.2,0.1,0.4-0.1,0.4-0.3C14.8,19.7,14.7,19.7,14.5,19.8zM6,19.4c0-0.1,0-0.2-0.1-0.2c-0.1,0-0.2,0-0.3,0.2C5.7,19.4,5.8,19.4,6,19.4zM7.2,19.4c0-0.1,0-0.2-0.1-0.2c-0.1,0-0.2,0-0.3,0.2C7,19.4,7.1,19.4,7.2,19.4zM17.1,19.4c0-0.1,0-0.2-0.1-0.2c-0.1,0-0.2,0-0.3,0.2C16.8,19.4,16.9,19.4,17.1,19.4zM10.3,19.9c0.1,0,0.2,0,0.2,0c0.2-0.1,0.2-0.2,0.2-0.4c0-0.1,0-0.2-0.1-0.2c-0.1,0-0.2,0-0.3,0c-0.1,0.1-0.2,0.3-0.2,0.5C10.2,19.8,10.2,19.9,10.3,19.9z"
                        :icon/license-mit "M13.2,11.8c0.9,0,1.8,0,2.7,0c0,4.1,0,8.2,0,12.3c-0.6,0-1.1,0-1.7,0c0-3.2,0-6.4,0-9.7c-0.8,3.3-1.6,6.5-2.5,9.7c-0.5,0-1,0-1.5,0c-0.8-3.2-1.6-6.4-2.4-9.7c0,3.3,0,6.5,0,9.7c-0.6,0-1.1,0-1.8,0c0-4.1,0-8.2,0-12.3c0.9,0,1.8,0,2.7,0c0.7,2.9,1.5,5.9,2.2,9C11.7,17.7,12.4,14.8,13.2,11.8zM20.9,11.9c0,0.7,0,1.3,0,2c1.1,0,2.1,0,3.1,0c0,3.5,0,6.9,0,10.3c0.7,0,1.4,0,2.1,0c0-3.5,0-6.9,0-10.4c1.1,0,2.1,0,3.1,0c0-0.7,0-1.3,0-1.9C26.4,11.9,23.7,11.9,20.9,11.9zM17.7,24.1c0.7,0,1.3,0,1.9,0c0-4.1,0-8.2,0-12.3c-0.7,0-1.3,0-1.9,0C17.7,16,17.7,20,17.7,24.1zM34,17.5C34,26.6,26.6,34,17.5,34C8.4,34,1,26.6,1,17.5C1,8.4,8.4,1,17.5,1C26.6,1,34,8.4,34,17.5zM31.9,17.5c0-7.9-6.4-14.4-14.4-14.4C9.6,3.1,3.1,9.6,3.1,17.5c0,7.9,6.4,14.4,14.4,14.4C25.4,31.9,31.9,25.4,31.9,17.5z"
                        :icon/link "M31.7,20.7l-3.1-3.1c-1.2-1.2-3.1-1.2-4.2,0c-0.6,0.6-0.9,1.3-0.9,2.1c0,0.8,0.3,1.5,0.9,2.1l3.1,3.1c0.3,0.3,0.5,0.8,0.5,1.3c0,0.5-0.2,1-0.5,1.3c-0.7,0.7-1.9,0.7-2.6,0l-5.4-5.4c0.8,0,1.5-0.3,2-0.9c1.5-1.5,2.3-3.4,2.3-5.5s-0.8-4-2.3-5.5l-7-6.9C12.9,1.8,11,1,8.9,1C6.8,1,4.8,1.8,3.3,3.3C1.8,4.8,1,6.7,1,8.8s0.8,4,2.3,5.5l3.4,3.3c1.2,1.2,3.1,1.2,4.2,0c0.6-0.6,0.9-1.3,0.9-2.1c0-0.8-0.3-1.5-0.9-2.1l-3.4-3.3C7.2,9.8,7,9.3,7,8.8s0.2-1,0.5-1.3c0.7-0.7,1.9-0.7,2.6,0l5.4,5.4c-0.7,0-1.5,0.3-2,0.9c-1.5,1.5-2.3,3.4-2.3,5.5s0.8,4,2.3,5.5l7,6.9c1.5,1.5,3.5,2.3,5.6,2.3c2.1,0,4.1-0.8,5.6-2.3c1.5-1.5,2.3-3.4,2.3-5.5C34,24.1,33.2,22.1,31.7,20.7zM8.9,6c-0.8,0-1.5,0.3-2,0.8C6.3,7.3,6,8,6,8.8c0,0.8,0.3,1.5,0.8,2l3.4,3.3c0.4,0.4,0.6,0.9,0.6,1.4c0,0.5-0.2,1-0.6,1.4c-0.8,0.8-2.1,0.8-2.8,0L4,13.6c-1.3-1.3-2-3-2-4.8S2.7,5.3,4,4c1.3-1.3,3-2,4.9-2c1.8,0,3.6,0.7,4.9,2l7,6.9c1.3,1.3,2,3,2,4.8c0,1.8-0.7,3.5-2,4.8c-0.8,0.8-2.1,0.8-2.8,0c-0.4-0.4-0.6-0.8-0.6-1.4c0-0.5,0.2-0.9,0.5-1.2c0.6-0.6,0.9-1.3,0.9-2.1c0,0,0,0,0,0c0,0,0-0.1,0-0.1c0-0.8-0.3-1.5-0.8-2l-7-6.9C10.4,6.3,9.6,6,8.9,6zM31,31c-1.3,1.3-3,2-4.9,2c-1.8,0-3.6-0.7-4.9-2l-7-6.9c-1.3-1.3-2-3-2-4.8s0.7-3.5,2-4.8c0.4-0.4,0.9-0.6,1.4-0.6s1,0.2,1.4,0.6c0.4,0.4,0.6,0.8,0.6,1.4c0,0.5-0.2,0.9-0.5,1.2c-0.6,0.6-0.9,1.3-0.9,2.1c0,0,0,0,0,0c0,0,0,0.1,0,0.1c0,0.8,0.3,1.5,0.8,2l7,6.9c1.1,1.1,3,1.1,4,0c0.5-0.5,0.8-1.3,0.8-2c0-0.8-0.3-1.5-0.8-2l-3.1-3.1c-0.4-0.4-0.6-0.9-0.6-1.4c0-0.5,0.2-1,0.6-1.4c0.8-0.8,2.1-0.8,2.8,0l3.1,3.1c1.3,1.3,2,3,2,4.8C33,28,32.3,29.7,31,31z"
                        :icon/lock "M28.5,14H27v-3.5C27,5.2,22.7,1,17.5,1h-0.1C12.3,1,8,5.2,8,10.5V14H6.5C4.6,14,3,15.6,3,17.5v13C3,32.4,4.6,34,6.5,34h22c1.9,0,3.5-1.6,3.5-3.5v-13C32,15.6,30.4,14,28.5,14zM9,10.5C9,5.8,12.8,2,17.5,2h0.1c4.7,0,8.5,3.8,8.5,8.5V14h-3v-3.5c0-3-2.5-5.5-5.5-5.5S12,7.5,12,10.5V14H9V10.5zM22,14h-9v-3.5C13,8,15,6,17.5,6S22,8,22,10.5V14zM31,30.5c0,1.4-1.1,2.5-2.5,2.5h-22C5.1,33,4,31.9,4,30.5v-13C4,16.1,5.1,15,6.5,15h22c1.4,0,2.5,1.1,2.5,2.5V30.5zM20.4,21.9c0-0.9-0.4-1.7-1.1-2.2c-0.7-0.6-1.6-0.8-2.5-0.6c-1.1,0.2-2,1.1-2.3,2.3c-0.2,1.1,0.3,2.3,1.2,2.9l-0.6,3.4c-0.1,0.3,0,0.7,0.3,1c0.2,0.3,0.6,0.4,0.9,0.4h2.1c0.4,0,0.7-0.2,0.9-0.4c0.2-0.3,0.3-0.6,0.3-1l-0.6-3.4C20,23.7,20.4,22.8,20.4,21.9zM18.6,23.4c-0.3,0.2-0.5,0.5-0.4,0.9l0.6,3.5c0,0.1,0,0.1,0,0.1c0,0-0.1,0.1-0.2,0.1h-2.1c-0.1,0-0.1,0-0.2-0.1c0,0-0.1-0.1,0-0.1l0.6-3.5c0.1-0.3-0.1-0.7-0.4-0.9c-0.7-0.4-1-1.2-0.9-1.9c0.1-0.7,0.7-1.3,1.5-1.5c0.1,0,0.3,0,0.4,0c0.5,0,0.9,0.2,1.3,0.4c0.4,0.4,0.7,0.9,0.7,1.4C19.4,22.5,19.1,23.1,18.6,23.4z"
                        :icon/logo-dark "M29.3,10.1V3.5l-6-3.5l-5.7,3.3L11.8,0l-6,3.5v6.6L0,13.4v7l5.7,3.3v6.6l6,3.5l5.7-3.3l5.7,3.3l6-3.5v-6.6l5.7-3.3v-7L29.3,10.1zM22.9,0.9v5.6l-4.8-2.8L22.9,0.9zM17.2,17.1V23l-5.1-3v-5.9L17.2,17.1zM22.9,20l-5.1,3v-5.9l5.1-3V20L22.9,20zM16.9,3.7l-4.8,2.8V0.9L16.9,3.7zM0.6,19.7v-5.9l5.7-3.3V3.8l5.1-3v6.6l5.7-3.3v5.9l-5.7,3.3V20l-5.1,3v-6.6L0.6,19.7zM6.3,24l4.8,2.8l-4.8,2.8V24L6.3,24zM28.7,29.6l-4.8-2.8l4.8-2.8V29.6L28.7,29.6zM34.4,19.7l-5.7-3.3V23l-5.1-3v-6.6l-5.7-3.3V4.2l5.7,3.3V0.9l5.1,3v6.6l5.7,3.3L34.4,19.7L34.4,19.7z"
                        :icon/logo-light "M29.3,10.1V3.5l-6-3.5l-5.7,3.3L11.8,0l-6,3.5v6.6L0,13.4v7l5.7,3.3v6.6l6,3.5l5.7-3.3l5.7,3.3l6-3.5v-6.6l5.7-3.3v-7L29.3,10.1zM22.6,13.6l-5.1,3l-5.1-3l5.1-3L22.6,13.6zM12.1,14.1l5.1,3V23l-5.1-3V14.1zM16.9,3.7l-4.8,2.8V0.9L16.9,3.7zM0.9,20.2l4.8-2.8V23L0.9,20.2zM23.2,33.1l-5.7-3.3l-5.7,3.3l-5.1-3l5.7-3.3l-5.7-3.3l5.1-3l5.7,3.3l5.7-3.3l5.1,3l-5.7,3.3l5.7,3.3L23.2,33.1zM28.7,29.6l-4.8-2.8l4.8-2.8V29.6L28.7,29.6zM29.3,23v-5.6l4.8,2.8L29.3,23zM34.4,19.7l-5.7-3.3V23l-5.1-3v-6.6l-5.7-3.3V4.2l5.7,3.3V0.9l5.1,3v6.6l5.7,3.3L34.4,19.7L34.4,19.7z"
                        :icon/mag-glass "M30.9,30.1L23,22.3c1.9-2,3-4.8,3-7.8C26,8.2,20.8,3,14.5,3S3,8.2,3,14.5S8.2,26,14.5,26c3,0,5.7-1.2,7.8-3l7.9,7.9c0.1,0.1,0.2,0.1,0.4,0.1s0.3,0,0.4-0.1C31,30.7,31,30.3,30.9,30.1zM14.5,25C8.7,25,4,20.3,4,14.5S8.7,4,14.5,4S25,8.7,25,14.5S20.3,25,14.5,25z"
                        :icon/open-in-editor "M35,10.5v21c0,1.9-1.6,3.5-3.5,3.5h-28C1.6,35,0,33.4,0,31.5v-28C0,1.6,1.6,0,3.5,0h21C24.8,0,25,0.2,25,0.5S24.8,1,24.5,1h-21C2.1,1,1,2.1,1,3.5v28C1,32.9,2.1,34,3.5,34h28c1.4,0,2.5-1.1,2.5-2.5v-21c0-0.3,0.2-0.5,0.5-0.5S35,10.2,35,10.5zM3,4c0,0.6,0.4,1,1,1s1-0.4,1-1c0-0.6-0.4-1-1-1S3,3.4,3,4zM9,4c0-0.6-0.4-1-1-1S7,3.4,7,4c0,0.6,0.4,1,1,1S9,4.6,9,4zM12,3c-0.6,0-1,0.4-1,1c0,0.6,0.4,1,1,1s1-0.4,1-1C13,3.4,12.6,3,12,3zM18.5,14.4l-2.8,10.3c-0.1,0.3,0.1,0.5,0.4,0.6c0,0,0.1,0,0.1,0c0.2,0,0.4-0.1,0.5-0.4l2.8-10.3c0.1-0.3-0.1-0.5-0.4-0.6C18.8,13.9,18.6,14.1,18.5,14.4zM22.4,23.5c0.1,0.2,0.3,0.3,0.4,0.3c0.1,0,0.2,0,0.2-0.1l6.4-3.2c0.4-0.2,0.6-0.5,0.6-0.9c0-0.4-0.2-0.7-0.6-0.9L23,15.6c-0.2-0.1-0.5,0-0.7,0.2c-0.1,0.2,0,0.5,0.2,0.7l6.4,3.2l-6.4,3.2C22.3,23,22.2,23.3,22.4,23.5zM12.8,15.8c-0.1-0.2-0.4-0.3-0.7-0.2l-6.4,3.2c-0.4,0.2-0.6,0.5-0.6,0.9c0,0.4,0.2,0.7,0.6,0.9l6.4,3.2c0.1,0,0.1,0.1,0.2,0.1c0.2,0,0.4-0.1,0.4-0.3c0.1-0.2,0-0.5-0.2-0.7l-6.4-3.2l6.4-3.2C12.8,16.3,12.9,16,12.8,15.8zM35,0.3c-0.1-0.1-0.1-0.2-0.3-0.3c-0.1,0-0.1,0-0.2,0h-6C28.2,0,28,0.2,28,0.5S28.2,1,28.5,1h4.8l-5.1,5.1c-0.2,0.2-0.2,0.5,0,0.7C28.2,7,28.4,7,28.5,7s0.3,0,0.4-0.1L34,1.7v4.8C34,6.8,34.2,7,34.5,7S35,6.8,35,6.5v-6C35,0.4,35,0.4,35,0.3z"
                        :icon/plus-sign "M30,3H5C3.9,3,3,3.9,3,5v25c0,1.1,0.9,2,2,2h25c1.1,0,2-0.9,2-2V5C32,3.9,31.1,3,30,3zM31,30c0,0.6-0.4,1-1,1H5c-0.6,0-1-0.4-1-1V5c0-0.6,0.4-1,1-1h25c0.6,0,1,0.4,1,1V30zM25,17.5c0,0.3-0.2,0.5-0.5,0.5H18v6.5c0,0.3-0.2,0.5-0.5,0.5S17,24.8,17,24.5V18h-6.5c-0.3,0-0.5-0.2-0.5-0.5s0.2-0.5,0.5-0.5H17v-6.5c0-0.3,0.2-0.5,0.5-0.5s0.5,0.2,0.5,0.5V17h6.5C24.8,17,25,17.2,25,17.5z"
                        :icon/rar "M32,9.2c0-0.6-0.2-1.2-0.7-1.6l-5.9-5.9C25,1.2,24.4,1,23.8,1L4.3,1C3.6,1,3,1.6,3,2.3l0,30.4C3,33.4,3.6,34,4.3,34h26.4c0.7,0,1.3-0.6,1.3-1.3L32,9.2zM30.7,33H4.3C4.1,33,4,32.9,4,32.7L4,2.3C4,2.1,4.1,2,4.3,2l19.5,0c0.3,0,0.7,0.1,0.9,0.4l5.9,5.9C30.8,8.5,31,8.9,31,9.2l0,23.5C31,32.9,30.9,33,30.7,33zM29.5,9.5c0,0.3-0.2,0.5-0.5,0.5l-4.9,0C23.5,10,23,9.5,23,8.9L23,4c0-0.3,0.2-0.5,0.5-0.5S24,3.7,24,4l0,4.9C24,9,24,9,24.1,9L29,9C29.3,9,29.5,9.2,29.5,9.5zM18,13.5c0,0.3-0.2,0.5-0.5,0.5S17,13.8,17,13.5s0.2-0.5,0.5-0.5S18,13.2,18,13.5zM18,11.5c0,0.3-0.2,0.5-0.5,0.5S17,11.8,17,11.5s0.2-0.5,0.5-0.5S18,11.2,18,11.5zM18,9.5c0,0.3-0.2,0.5-0.5,0.5S17,9.8,17,9.5C17,9.2,17.2,9,17.5,9S18,9.2,18,9.5zM18,7.5c0,0.3-0.2,0.5-0.5,0.5S17,7.8,17,7.5S17.2,7,17.5,7S18,7.2,18,7.5zM18,5.5c0,0.3-0.2,0.5-0.5,0.5S17,5.8,17,5.5C17,5.2,17.2,5,17.5,5S18,5.2,18,5.5zM17,3.5C17,3.2,17.2,3,17.5,3S18,3.2,18,3.5s-0.2,0.5-0.5,0.5S17,3.8,17,3.5zM15.9,12.6c0-0.3,0.2-0.5,0.5-0.5s0.5,0.2,0.5,0.5s-0.2,0.5-0.5,0.5S15.9,12.9,15.9,12.6zM15.9,10.6c0-0.3,0.2-0.5,0.5-0.5s0.5,0.2,0.5,0.5s-0.2,0.5-0.5,0.5S15.9,10.9,15.9,10.6zM15.9,8.6c0-0.3,0.2-0.5,0.5-0.5S17,8.2,17,8.6c0,0.3-0.2,0.5-0.5,0.5S15.9,8.9,15.9,8.6zM15.9,6.6c0-0.3,0.2-0.5,0.5-0.5S17,6.2,17,6.6s-0.2,0.5-0.5,0.5S15.9,6.9,15.9,6.6zM15.9,4.6c0-0.3,0.2-0.5,0.5-0.5S17,4.2,17,4.6c0,0.3-0.2,0.5-0.5,0.5S15.9,4.9,15.9,4.6zM18,15.5v2.9c0,0.3-0.2,0.5-0.5,0.5h-0.9c-0.3,0-0.5-0.2-0.5-0.5v-2.9c0-0.3,0.2-0.5,0.5-0.5h0.9C17.8,15,18,15.2,18,15.5zM10,27.5c0,0.3-0.2,0.5-0.5,0.5S9,27.8,9,27.5C9,27.2,9.2,27,9.5,27S10,27.2,10,27.5zM15,24.5c0-0.8-0.7-1.5-1.5-1.5h-2c-0.3,0-0.5,0.2-0.5,0.5v4c0,0.3,0.2,0.5,0.5,0.5s0.5-0.2,0.5-0.5V26h0.7l1.3,1.8c0.1,0.1,0.2,0.2,0.4,0.2c0.1,0,0.2,0,0.3-0.1c0.2-0.2,0.3-0.5,0.1-0.7l-1-1.3C14.6,25.7,15,25.2,15,24.5zM13.5,25H12v-1h1.5c0.3,0,0.5,0.2,0.5,0.5S13.8,25,13.5,25zM25,24.5c0-0.8-0.7-1.5-1.5-1.5h-2c-0.3,0-0.5,0.2-0.5,0.5v4c0,0.3,0.2,0.5,0.5,0.5s0.5-0.2,0.5-0.5V26h0.7l1.3,1.8c0.1,0.1,0.2,0.2,0.4,0.2c0.1,0,0.2,0,0.3-0.1c0.2-0.2,0.3-0.5,0.1-0.7l-1-1.3C24.6,25.7,25,25.2,25,24.5zM23.5,25H22v-1h1.5c0.3,0,0.5,0.2,0.5,0.5S23.8,25,23.5,25zM18,23c-1.1,0-2,0.9-2,2v2.5c0,0.3,0.2,0.5,0.5,0.5s0.5-0.2,0.5-0.5V26h2v1.5c0,0.3,0.2,0.5,0.5,0.5s0.5-0.2,0.5-0.5V25C20,23.9,19.1,23,18,23zM17,25c0-0.6,0.4-1,1-1s1,0.4,1,1H17z"
                        :icon/rar-hover "M32,27.5c0,0.3-0.2,0.5-0.5,0.5S31,27.8,31,27.5l0-18.3c0-0.3-0.1-0.7-0.4-0.9l-5.9-5.9C24.5,2.1,24.1,2,23.8,2L4.3,2C4.1,2,4,2.1,4,2.3l0,30.4C4,32.9,4.1,33,4.3,33h19.2c0.3,0,0.5,0.2,0.5,0.5c0,0.3-0.2,0.5-0.5,0.5H4.3C3.6,34,3,33.4,3,32.7L3,2.3C3,1.6,3.6,1,4.3,1l19.5,0c0.6,0,1.2,0.2,1.6,0.7l5.9,5.9C31.7,8,32,8.6,32,9.2L32,27.5zM24.1,10l4.9,0c0.3,0,0.5-0.2,0.5-0.5C29.5,9.2,29.3,9,29,9l-4.9,0C24,9,24,9,24,8.9L24,4c0-0.3-0.2-0.5-0.5-0.5S23,3.7,23,4l0,4.9C23,9.5,23.5,10,24.1,10zM17.5,13c-0.3,0-0.5,0.2-0.5,0.5s0.2,0.5,0.5,0.5s0.5-0.2,0.5-0.5S17.8,13,17.5,13zM17.5,11c-0.3,0-0.5,0.2-0.5,0.5s0.2,0.5,0.5,0.5s0.5-0.2,0.5-0.5S17.8,11,17.5,11zM17.5,9C17.2,9,17,9.2,17,9.5c0,0.3,0.2,0.5,0.5,0.5S18,9.8,18,9.5C18,9.2,17.8,9,17.5,9zM17.5,7C17.2,7,17,7.2,17,7.5s0.2,0.5,0.5,0.5S18,7.8,18,7.5S17.8,7,17.5,7zM17.5,5C17.2,5,17,5.2,17,5.5c0,0.3,0.2,0.5,0.5,0.5S18,5.8,18,5.5C18,5.2,17.8,5,17.5,5zM17.5,3C17.2,3,17,3.2,17,3.5s0.2,0.5,0.5,0.5S18,3.8,18,3.5S17.8,3,17.5,3zM16.5,13.1c0.3,0,0.5-0.2,0.5-0.5S16.8,12,16.5,12s-0.5,0.2-0.5,0.5S16.2,13.1,16.5,13.1zM16.5,11.1c0.3,0,0.5-0.2,0.5-0.5S16.8,10,16.5,10s-0.5,0.2-0.5,0.5S16.2,11.1,16.5,11.1zM16.5,9.1c0.3,0,0.5-0.2,0.5-0.5C17,8.2,16.8,8,16.5,8s-0.5,0.2-0.5,0.5C15.9,8.9,16.2,9.1,16.5,9.1zM16.5,7.1c0.3,0,0.5-0.2,0.5-0.5S16.8,6,16.5,6s-0.5,0.2-0.5,0.5S16.2,7.1,16.5,7.1zM16.5,4c-0.3,0-0.5,0.2-0.5,0.5c0,0.3,0.2,0.5,0.5,0.5S17,4.9,17,4.6C17,4.2,16.8,4,16.5,4zM17.5,15h-0.9c-0.3,0-0.5,0.2-0.5,0.5v2.9c0,0.3,0.2,0.5,0.5,0.5h0.9c0.3,0,0.5-0.2,0.5-0.5v-2.9C18,15.2,17.8,15,17.5,15zM9,27.5C9,27.8,9.2,28,9.5,28s0.5-0.2,0.5-0.5c0-0.3-0.2-0.5-0.5-0.5S9,27.2,9,27.5zM13.9,25.9l1,1.3c0.2,0.2,0.1,0.5-0.1,0.7C14.7,28,14.6,28,14.5,28c-0.2,0-0.3-0.1-0.4-0.2L12.7,26H12v1.5c0,0.3-0.2,0.5-0.5,0.5S11,27.8,11,27.5v-4c0-0.3,0.2-0.5,0.5-0.5h2c0.8,0,1.5,0.7,1.5,1.5C15,25.2,14.6,25.7,13.9,25.9zM14,24.5c0-0.3-0.2-0.5-0.5-0.5H12v1h1.5C13.8,25,14,24.8,14,24.5zM21.5,23h2c0.8,0,1.5,0.7,1.5,1.5c0,0.7-0.4,1.2-1.1,1.4l1,1.3c0.2,0.2,0.1,0.5-0.1,0.7C24.7,28,24.6,28,24.5,28c-0.2,0-0.3-0.1-0.4-0.2L22.7,26H22v1.5c0,0.3-0.2,0.5-0.5,0.5S21,27.8,21,27.5v-4C21,23.2,21.2,23,21.5,23zM23.5,24H22v1h1.5c0.3,0,0.5-0.2,0.5-0.5S23.8,24,23.5,24zM19,27.5V26h-2v1.5c0,0.3-0.2,0.5-0.5,0.5S16,27.8,16,27.5V25c0-1.1,0.9-2,2-2s2,0.9,2,2v2.5c0,0.3-0.2,0.5-0.5,0.5S19,27.8,19,27.5zM19,25c0-0.6-0.4-1-1-1s-1,0.4-1,1H19zM30.7,29.6L28,32.3v-6.8c0-0.3-0.2-0.5-0.5-0.5S27,25.2,27,25.5v6.8l-2.7-2.7c-0.2-0.2-0.5-0.2-0.7,0s-0.2,0.5,0,0.7l3.5,3.5c0,0,0.1,0.1,0.2,0.1c0.1,0,0.1,0,0.2,0s0.1,0,0.2,0c0.1,0,0.1-0.1,0.2-0.1l3.5-3.5c0.2-0.2,0.2-0.5,0-0.7S30.9,29.4,30.7,29.6z"
                        :icon/triangle-error "M33.6,27.4L20.1,3.9c-0.6-1-1.6-1.5-2.6-1.5s-2,0.5-2.6,1.5L1.4,27.4C0.2,29.4,1.7,32,4.1,32h26.8C33.3,32,34.8,29.4,33.6,27.4zM32.7,30c-0.4,0.7-1.1,1.1-1.8,1.1H4.1c-0.8,0-1.4-0.4-1.8-1.1c-0.4-0.7-0.4-1.4,0-2.1L15.8,4.4c0.4-0.6,1-1,1.7-1s1.3,0.4,1.7,1l13.5,23.4C33.1,28.5,33.1,29.3,32.7,30zM18.1,25.5c0,0.4-0.3,0.7-0.7,0.7s-0.7-0.3-0.7-0.7c0-0.4,0.3-0.7,0.7-0.7S18.1,25.1,18.1,25.5zM17,22.6v-9.1c0-0.3,0.2-0.5,0.5-0.5s0.5,0.2,0.5,0.5v9.1c0,0.3-0.2,0.5-0.5,0.5S17,22.9,17,22.6z"
                        :icon/triangle-sad "M33.6,27.4L20.1,3.9c-0.6-1-1.6-1.5-2.6-1.5s-2,0.5-2.6,1.5L1.4,27.4C0.2,29.4,1.7,32,4.1,32h26.8C33.3,32,34.8,29.4,33.6,27.4zM32.7,30c-0.4,0.7-1.1,1.1-1.8,1.1H4.1c-0.8,0-1.4-0.4-1.8-1.1c-0.4-0.7-0.4-1.4,0-2.1L15.8,4.4c0.4-0.6,1-1,1.7-1s1.3,0.4,1.7,1l13.5,23.4C33.1,28.5,33.1,29.3,32.7,30zM24,19.6c-0.2,0.8-1.1,1.4-2,1.4s-1.7-0.6-2-1.4c-0.1-0.3,0.1-0.5,0.3-0.6c0.3-0.1,0.5,0.1,0.6,0.3c0.1,0.4,0.5,0.6,1,0.6s0.9-0.3,1-0.6c0.1-0.3,0.4-0.4,0.6-0.3C23.9,19.1,24.1,19.4,24,19.6zM15,19.6c-0.2,0.8-1.1,1.4-2,1.4s-1.7-0.6-2-1.4c-0.1-0.3,0.1-0.5,0.3-0.6c0.3-0.1,0.5,0.1,0.6,0.3c0.1,0.4,0.5,0.6,1,0.6s0.9-0.3,1-0.6c0.1-0.3,0.4-0.4,0.6-0.3C14.9,19.1,15.1,19.4,15,19.6zM20,25.3c0.1,0.3,0,0.5-0.3,0.6c-0.1,0-0.1,0-0.2,0c-0.2,0-0.4-0.1-0.5-0.3c-0.1-0.3-0.7-0.7-1.5-0.7s-1.4,0.4-1.5,0.7c-0.1,0.3-0.4,0.4-0.6,0.3c-0.3-0.1-0.4-0.4-0.3-0.6c0.3-0.8,1.3-1.3,2.5-1.3S19.7,24.5,20,25.3z"
                        :icon/server "M33.5,16.5c0-0.1-0.1-0.2-0.2-0.3c-0.1-0.1-0.3-0.1-0.4,0c-2.6,0.7-5.1-0.1-7-2c-0.2-0.2-0.5-0.2-0.7,0c-1.9,1.9-4.4,2.7-7,2c-0.1,0-0.3,0-0.4,0c-0.1,0.1-0.2,0.2-0.2,0.3c-0.3,1.3-0.5,2.5-0.5,3.8c0,6,3.4,11.2,8.3,12.7c0,0,0.1,0,0.1,0s0.1,0,0.1,0c4.9-1.5,8.3-6.7,8.3-12.7C34,19,33.8,17.7,33.5,16.5zM25.5,32c-4.4-1.4-7.5-6.2-7.5-11.7c0-1,0.1-2.1,0.3-3.1c2.6,0.5,5.1-0.2,7.1-2c2,1.8,4.5,2.6,7.1,2c0.2,1,0.3,2.1,0.3,3.1C33,25.8,29.9,30.5,25.5,32zM30.1,20.4c0.2,0.2,0.2,0.5,0,0.7l-5.5,5.5c-0.1,0.1-0.2,0.1-0.4,0.1s-0.3,0-0.4-0.1l-2.8-2.8c-0.2-0.2-0.2-0.5,0-0.7s0.5-0.2,0.7,0l2.4,2.4l5.1-5.1C29.6,20.2,29.9,20.2,30.1,20.4zM5.5,5C4.7,5,4,5.7,4,6.5S4.7,8,5.5,8S7,7.3,7,6.5S6.3,5,5.5,5zM5.5,7C5.2,7,5,6.8,5,6.5S5.2,6,5.5,6S6,6.2,6,6.5S5.8,7,5.5,7zM9.5,5C8.7,5,8,5.7,8,6.5S8.7,8,9.5,8S11,7.3,11,6.5S10.3,5,9.5,5zM9.5,7C9.2,7,9,6.8,9,6.5S9.2,6,9.5,6S10,6.2,10,6.5S9.8,7,9.5,7zM24.5,5h-5C18.7,5,18,5.7,18,6.5S18.7,8,19.5,8h5C25.3,8,26,7.3,26,6.5S25.3,5,24.5,5zM24.5,7h-5C19.2,7,19,6.8,19,6.5S19.2,6,19.5,6h5C24.8,6,25,6.2,25,6.5S24.8,7,24.5,7zM4,16.5C4,17.3,4.7,18,5.5,18S7,17.3,7,16.5S6.3,15,5.5,15S4,15.7,4,16.5zM6,16.5C6,16.8,5.8,17,5.5,17S5,16.8,5,16.5S5.2,16,5.5,16S6,16.2,6,16.5zM8,16.5C8,17.3,8.7,18,9.5,18s1.5-0.7,1.5-1.5S10.3,15,9.5,15S8,15.7,8,16.5zM10,16.5c0,0.3-0.2,0.5-0.5,0.5S9,16.8,9,16.5S9.2,16,9.5,16S10,16.2,10,16.5zM18.5,31H3c-0.6,0-1-0.4-1-1v-7c0-0.6,0.4-1,1-1h11.5H15c0.3,0,0.5-0.2,0.5-0.5S15.3,21,15,21h-0.5H3c-0.6,0-1-0.4-1-1v-7c0-0.6,0.4-1,1-1h24c0.6,0,1,0.4,1,1v0.5c0,0.3,0.2,0.5,0.5,0.5s0.5-0.2,0.5-0.5V13c0-0.6-0.3-1.1-0.7-1.5c0.4-0.4,0.7-0.9,0.7-1.5V3c0-1.1-0.9-2-2-2H3C1.9,1,1,1.9,1,3v7c0,0.6,0.3,1.1,0.7,1.5C1.3,11.9,1,12.4,1,13v7c0,0.6,0.3,1.1,0.7,1.5C1.3,21.9,1,22.4,1,23v7c0,1.1,0.9,2,2,2h1c0,0.6,0.4,1,1,1h3c0.6,0,1-0.4,1-1h9.5c0.3,0,0.5-0.2,0.5-0.5S18.8,31,18.5,31zM2,10V3c0-0.6,0.4-1,1-1h24c0.6,0,1,0.4,1,1v7c0,0.6-0.4,1-1,1H3C2.4,11,2,10.6,2,10zM4,26.5C4,27.3,4.7,28,5.5,28S7,27.3,7,26.5S6.3,25,5.5,25S4,25.7,4,26.5zM6,26.5C6,26.8,5.8,27,5.5,27S5,26.8,5,26.5S5.2,26,5.5,26S6,26.2,6,26.5zM8,26.5C8,27.3,8.7,28,9.5,28s1.5-0.7,1.5-1.5S10.3,25,9.5,25S8,25.7,8,26.5zM10,26.5c0,0.3-0.2,0.5-0.5,0.5S9,26.8,9,26.5S9.2,26,9.5,26S10,26.2,10,26.5z"
                        :icon/share "M30,25c-1.8,0-3.3,0.9-4.2,2.3l-15.1-7.6c0.2-0.6,0.3-1.1,0.3-1.8s-0.1-1.2-0.3-1.8l15.1-7.6c0.9,1.4,2.4,2.3,4.2,2.3c2.8,0,5-2.2,5-5s-2.2-5-5-5s-5,2.2-5,5c0,0.6,0.1,1.2,0.3,1.8l-15.1,7.6C9.3,13.9,7.8,13,6,13c-2.8,0-5,2.2-5,5s2.2,5,5,5c1.8,0,3.3-0.9,4.2-2.3l15.1,7.6C25.1,28.8,25,29.4,25,30c0,2.8,2.2,5,5,5s5-2.2,5-5S32.8,25,30,25zM30,2c2.2,0,4,1.8,4,4s-1.8,4-4,4c-1.5,0-2.9-0.9-3.5-2.2c0,0,0,0,0,0c0,0,0,0,0-0.1C26.2,7.2,26,6.6,26,6C26,3.8,27.8,2,30,2zM6,22c-2.2,0-4-1.8-4-4s1.8-4,4-4c1.5,0,2.9,0.9,3.5,2.2c0,0,0,0,0,0c0,0,0,0,0,0C9.8,16.8,10,17.4,10,18c0,0.6-0.2,1.2-0.4,1.7c0,0,0,0,0,0.1c0,0,0,0,0,0C8.9,21.1,7.5,22,6,22zM30,34c-2.2,0-4-1.8-4-4c0-0.6,0.2-1.2,0.4-1.7c0,0,0,0,0,0c0,0,0,0,0,0c0.7-1.3,2-2.2,3.5-2.2c2.2,0,4,1.8,4,4S32.2,34,30,34z"
                        :icon/slack "M15,16.6l1.5,4.4l4.5-1.5l-1.5-4.4L15,16.6zM17.1,19.7l-0.8-2.5l2.6-0.9l0.8,2.5L17.1,19.7zM27.1,16.5c-0.3,0-0.6,0-0.9,0.1L25,17l-0.8-2.5l1.2-0.4c0.7-0.2,1.3-0.7,1.6-1.4c0.3-0.7,0.4-1.4,0.2-2.1c-0.4-1.1-1.4-1.9-2.6-1.9c-0.3,0-0.6,0-0.9,0.1l-1.3,0.4L22,8c-0.4-1.1-1.4-1.9-2.6-1.9c-0.3,0-0.6,0-0.9,0.1c-0.7,0.2-1.3,0.7-1.6,1.4c-0.3,0.7-0.4,1.4-0.2,2.1l0.4,1.3l-2.6,0.9l-0.4-1.3c-0.4-1.1-1.4-1.9-2.6-1.9c-0.3,0-0.6,0-0.9,0.1C9.9,9.1,9.3,9.6,9,10.3c-0.3,0.7-0.4,1.4-0.2,2.1l0.4,1.3L8,14.1c-0.7,0.2-1.3,0.7-1.6,1.4c-0.3,0.7-0.4,1.4-0.1,2.1c0.4,1.1,1.4,1.8,2.5,1.9c0.1,0,0.1,0,0.2,0c0.3,0,0.5-0.1,0.8-0.1l1.3-0.4l0.8,2.5l-1.2,0.4c-0.7,0.2-1.3,0.7-1.6,1.4c-0.3,0.7-0.4,1.4-0.1,2.1c0.4,1.1,1.4,1.8,2.5,1.9c0.1,0,0.1,0,0.2,0c0.3,0,0.5-0.1,0.8-0.1l1.3-0.4l0.4,1.3c0.4,1.1,1.4,1.8,2.5,1.9c0.1,0,0.1,0,0.2,0c0.3,0,0.5-0.1,0.8-0.1c0.7-0.2,1.3-0.7,1.6-1.4c0.3-0.7,0.4-1.4,0.2-2.1l-0.4-1.3l2.6-0.9l0.4,1.3c0.4,1.1,1.4,1.8,2.5,1.9c0.1,0,0.1,0,0.2,0c0.3,0,0.5-0.1,0.8-0.1c0.7-0.2,1.3-0.7,1.6-1.4c0.3-0.7,0.4-1.4,0.2-2.1l-0.4-1.3l1.2-0.4c0.7-0.2,1.3-0.7,1.6-1.4c0.3-0.7,0.4-1.4,0.2-2.1C29.4,17.2,28.3,16.5,27.1,16.5zM27.7,20.9l-2.2,0.7l0.8,2.3c0.3,0.9-0.2,1.9-1.1,2.2c-0.2,0-0.3,0.1-0.5,0.1c0,0-0.1,0-0.1,0c-0.7,0-1.4-0.5-1.6-1.2l-0.8-2.3l-4.5,1.5l0.8,2.3c0.3,0.9-0.2,1.9-1.1,2.2c-0.2,0-0.3,0.1-0.5,0.1c0,0-0.1,0-0.1,0c-0.7,0-1.4-0.5-1.6-1.2l-0.8-2.3L12,26.1c-0.2,0-0.3,0.1-0.5,0.1c0,0-0.1,0-0.1,0c-0.7,0-1.4-0.5-1.6-1.2c-0.3-0.9,0.2-1.9,1.1-2.2l2.2-0.7l-1.5-4.4l-2.2,0.7c-0.2,0-0.3,0.1-0.5,0.1c0,0-0.1,0-0.1,0c-0.7,0-1.4-0.5-1.6-1.2c-0.3-0.9,0.2-1.9,1.1-2.2l2.2-0.7L9.8,12c-0.3-0.9,0.2-1.9,1.1-2.2c0.2-0.1,0.4-0.1,0.6-0.1c0.7,0,1.4,0.5,1.7,1.2l0.8,2.3l4.5-1.5l-0.8-2.3c-0.3-0.9,0.2-1.9,1.1-2.2c0.2-0.1,0.4-0.1,0.6-0.1c0.7,0,1.4,0.5,1.7,1.2l0.8,2.3L24,9.8c0.2-0.1,0.4-0.1,0.6-0.1c0.7,0,1.4,0.5,1.7,1.2c0.3,0.9-0.2,1.9-1.1,2.2l-2.2,0.7l1.5,4.4l2.2-0.7c0.2-0.1,0.4-0.1,0.6-0.1c0.7,0,1.4,0.5,1.7,1.2C29.1,19.6,28.6,20.6,27.7,20.9zM33.6,13.3C31,4.7,27.6,1,21.3,1c-2.3,0-4.9,0.5-8,1.4C1.6,5.9-1.1,11,2.4,22.7C5,31.3,8.4,35,14.7,35c2.3,0,4.9-0.5,8-1.4C34.4,30.1,37.1,25,33.6,13.3zM22.4,32.6c-3.1,0.9-5.6,1.4-7.7,1.4c-5.6,0-8.8-3.3-11.3-11.6C1.6,16.6,1.5,12.8,3.1,10c1.5-2.8,4.8-4.8,10.6-6.6C16.7,2.5,19.2,2,21.3,2c5.6,0,8.8,3.3,11.3,11.6c1.7,5.8,1.8,9.6,0.3,12.4C31.4,28.9,28.2,30.9,22.4,32.6z"
                        :icon/star-filled "M31.2,15l-4.9,6.3c-0.3,0.4-0.5,0.9-0.5,1.4l0.2,7.9c0,0.2-0.2,0.3-0.4,0.3l-7.5-2.7c-0.2-0.1-0.5-0.1-0.8-0.1s-0.5,0-0.8,0.1l-7.5,2.7c-0.2,0.1-0.4-0.1-0.4-0.3l0.2-7.9c0-0.5-0.2-1-0.5-1.4L3.8,15c-0.1-0.2,0-0.4,0.1-0.5l7.6-2.3c0.5-0.2,0.9-0.5,1.2-0.9l4.5-6.6c0.1-0.2,0.4-0.2,0.5,0l4.5,6.6c0.3,0.4,0.7,0.7,1.2,0.9l7.6,2.3C31.3,14.6,31.3,14.8,31.2,15zM28,22.5c0,0-0.1,0.1-0.1,0.2l0.3,9.5c0,0.4-0.2,0.8-0.5,1c-0.2,0.2-0.5,0.2-0.7,0.2c-0.1,0-0.3,0-0.4-0.1l-8.9-3.2c-0.1,0-0.1,0-0.2,0l-8.9,3.2c-0.4,0.1-0.8,0.1-1.2-0.2c-0.3-0.2-0.5-0.6-0.5-1l0.3-9.5c0-0.1,0-0.1-0.1-0.2L1.3,15c-0.3-0.3-0.3-0.8-0.2-1.2c0.1-0.4,0.4-0.7,0.8-0.8l9.1-2.7c0.1,0,0.1-0.1,0.1-0.1l5.3-7.8c0.5-0.7,1.6-0.7,2.1,0l5.3,7.8c0,0,0.1,0.1,0.1,0.1l9.1,2.7c0.4,0.1,0.7,0.4,0.8,0.8c0.1,0.4,0.1,0.8-0.2,1.2L28,22.5zM32.9,14.4c0.1-0.1,0.1-0.2,0-0.2c0,0-0.1-0.1-0.2-0.2l-9.1-2.7c-0.3-0.1-0.5-0.3-0.7-0.5L17.7,3c-0.1-0.2-0.3-0.2-0.4,0l-5.3,7.8c-0.2,0.2-0.4,0.4-0.7,0.5L2.2,14C2.1,14,2,14.1,2,14.1c0,0,0,0.1,0,0.2l5.8,7.5c0.2,0.2,0.3,0.5,0.3,0.8l-0.3,9.5c0,0.1,0.1,0.2,0.1,0.2c0,0,0.1,0.1,0.2,0l8.9-3.2c0.1,0,0.3-0.1,0.4-0.1c0.1,0,0.3,0,0.4,0.1l8.9,3.2c0.1,0,0.2,0,0.2,0c0,0,0.1-0.1,0.1-0.2l-0.3-9.5c0-0.3,0.1-0.6,0.3-0.8L32.9,14.4z"
                        :icon/star-outline "M26.9,33.4c-0.1,0-0.3,0-0.4-0.1l-8.9-3.2c-0.1,0-0.1,0-0.2,0l-8.9,3.2c-0.4,0.1-0.8,0.1-1.2-0.2c-0.3-0.2-0.5-0.6-0.5-1l0.3-9.5c0-0.1,0-0.1-0.1-0.2L1.3,15c-0.3-0.3-0.3-0.8-0.2-1.2c0.1-0.4,0.4-0.7,0.8-0.8l9.1-2.7c0.1,0,0.1-0.1,0.1-0.1l5.3-7.8c0.5-0.7,1.6-0.7,2.1,0l5.3,7.8c0,0,0.1,0.1,0.1,0.1l9.1,2.7c0.4,0.1,0.7,0.4,0.8,0.8c0.1,0.4,0.1,0.8-0.2,1.2L28,22.5c0,0-0.1,0.1-0.1,0.2l0.3,9.5c0,0.4-0.2,0.8-0.5,1C27.4,33.3,27.2,33.4,26.9,33.4zM17.5,29.1c0.1,0,0.3,0,0.4,0.1l8.9,3.2c0.1,0,0.2,0,0.2,0c0,0,0.1-0.1,0.1-0.2l-0.3-9.5c0-0.3,0.1-0.6,0.3-0.8l5.8-7.5c0.1-0.1,0.1-0.2,0-0.2c0,0-0.1-0.1-0.2-0.2l-9.1-2.7c-0.3-0.1-0.5-0.3-0.7-0.5L17.7,3l0,0c-0.1-0.2-0.3-0.2-0.4,0l-5.3,7.8c-0.2,0.2-0.4,0.4-0.7,0.5L2.2,14C2.1,14,2,14.1,2,14.1c0,0,0,0.1,0,0.2l5.8,7.5c0.2,0.2,0.3,0.5,0.3,0.8l-0.3,9.5c0,0.1,0.1,0.2,0.1,0.2c0,0,0.1,0.1,0.2,0l8.9-3.2C17.2,29.1,17.4,29.1,17.5,29.1z"
                        :icon/twitter "M33.1,7.3C33.1,7.3,33.1,7.3,33.1,7.3c0,0-0.1,0-0.1,0c-0.1,0-0.2,0-0.3,0.1c0,0,0,0-0.1,0C32.9,7,33,6.5,33.2,6.1c0.1-0.4,0-0.8-0.3-1.1c-0.2-0.1-0.4-0.2-0.6-0.2c-0.2,0-0.4,0-0.5,0.1c-1,0.6-2.1,1-3.2,1.3c-1.3-1.2-3.1-1.9-5-1.9c-4.1,0-7.4,3.3-7.4,7.4c0,0.1,0,0.2,0,0.3C11.9,11.5,7.8,9.3,5,5.8C4.8,5.6,4.6,5.5,4.3,5.5c0,0-0.1,0-0.1,0C3.8,5.5,3.6,5.7,3.4,6c-0.7,1.1-1,2.4-1,3.7c0,1.2,0.3,2.4,0.9,3.5c-0.1,0-0.3,0.1-0.4,0.1c-0.3,0.2-0.5,0.5-0.5,0.8l0,0.1c0,2.2,1,4.2,2.6,5.6c0,0-0.1,0.1-0.1,0.1c-0.2,0.3-0.3,0.6-0.2,1c0.7,2.1,2.3,3.8,4.3,4.6c-1.6,0.8-3.4,1.2-5.3,1.2c-0.5,0-0.9,0-1.4-0.1c0,0-0.1,0-0.1,0c-0.4,0-0.8,0.3-0.9,0.7c-0.2,0.4,0,0.9,0.4,1.2c3.1,2,6.6,3,10.3,3c7.2,0,11.7-3.4,14.2-6.2c3.1-3.5,4.9-8.2,4.9-12.9c0-0.1,0-0.2,0-0.3c1.1-0.9,2.1-2,3-3.2c0.1-0.2,0.2-0.4,0.2-0.6C34.1,7.7,33.7,7.3,33.1,7.3zM29.9,11.6c0,0.3,0,0.5,0,0.8c0,8.4-6.4,18.1-18.1,18.1c-3.6,0-6.9-1.1-9.7-2.9c0.5,0.1,1,0.1,1.5,0.1c3,0,5.7-1,7.9-2.7c-2.8-0.1-5.1-1.9-5.9-4.4c0.4,0.1,0.8,0.1,1.2,0.1c0.6,0,1.1-0.1,1.7-0.2c-2.9-0.6-5.1-3.2-5.1-6.2c0,0,0-0.1,0-0.1c0.9,0.5,1.8,0.8,2.9,0.8c-1.7-1.1-2.8-3.1-2.8-5.3c0-1.2,0.3-2.3,0.9-3.2c3.1,3.8,7.8,6.4,13.1,6.6c-0.1-0.5-0.2-1-0.2-1.5c0-3.5,2.8-6.4,6.4-6.4c1.8,0,3.5,0.8,4.6,2c1.4-0.3,2.8-0.8,4-1.5c-0.5,1.5-1.5,2.7-2.8,3.5c1.3-0.2,2.5-0.5,3.7-1C32.2,9.6,31.2,10.7,29.9,11.6z"
                        :icon/upload "M34,25v8.5c0,0.3-0.2,0.5-0.5,0.5h-32C1.2,34,1,33.8,1,33.5V25c0-0.3,0.2-0.5,0.5-0.5S2,24.7,2,25v8h31v-8c0-0.3,0.2-0.5,0.5-0.5S34,24.7,34,25zM8,12.3l9-9V28c0,0.3,0.2,0.5,0.5,0.5S18,28.3,18,28V3.2l9,9c0.1,0.1,0.2,0.1,0.4,0.1s0.3,0,0.4-0.1c0.2-0.2,0.2-0.5,0-0.7l-9.9-9.9c0,0-0.1-0.1-0.2-0.1c-0.1-0.1-0.3-0.1-0.4,0c-0.1,0-0.1,0.1-0.2,0.1l-9.9,9.9c-0.2,0.2-0.2,0.5,0,0.7S7.8,12.4,8,12.3z"
                        :icon/vk "M30.6,15.9l0.4-0.5c0.9-1.1,1.7-2.3,2.4-3.6c0.2-0.4,0.4-0.9,0.6-1.5c0.2-0.7-0.1-1.2-0.8-1.4c-0.3-0.1-0.6-0.1-1-0.1c0,0,0,0,0,0c-0.9,0-1.7,0-2.6,0c-0.9,0-1.7,0-2.6,0c-0.9,0-1.3,0.2-1.7,1.1l0,0.1c-0.7,1.7-1.6,3.4-2.7,5.1c-0.5,0.7-0.9,1.2-1.4,1.6c-0.1,0.1-0.2,0.1-0.2,0.1c0,0,0,0-0.1-0.1c-0.1-0.2-0.2-0.4-0.2-0.6c0-0.8,0-1.5,0-2.3l0-1.8c0-0.2,0-0.4,0-0.6c0-0.5,0-1,0-1.5c-0.1-1.1-0.5-1.5-1.5-1.8C19.1,8.1,18.9,8,18.7,8c-1.9-0.2-3.6-0.2-5.2,0.1c-0.6,0.1-1.1,0.4-1.5,0.8c-0.1,0.1-0.2,0.3-0.2,0.4l-0.4,0.6l0.7,0.3c0.1,0,0.2,0.1,0.3,0.1c0.6,0.1,0.9,0.4,1,0.8c0.1,0.4,0.2,0.7,0.2,1.1c0.1,1.1,0.1,2.2-0.1,3.4c-0.1,0.4-0.1,0.9-0.4,1.2c-0.1,0-0.2,0-0.5-0.2c-0.6-0.4-1-1-1.5-1.9c-0.8-1.6-1.6-3.2-2.4-4.9c-0.4-0.8-1-1.2-1.9-1.2L6,8.7c-1.1,0-2.1,0-3.2,0c-0.3,0-0.5,0-0.7,0.1C1.5,8.9,1.3,9.1,1.2,9.3c-0.1,0.2-0.2,0.5-0.1,1c0.1,0.2,0.2,0.5,0.3,0.7c2,4.5,4.2,8.2,6.7,11.3c1.2,1.5,2.8,2.8,4.8,3.8c2,1,4.1,1,6.4,0.9c0.8,0,1.4-0.6,1.4-1.5l0-0.4c0-0.3,0-0.6,0.1-0.9c0.1-0.5,0.3-0.9,0.6-1.2c0.2-0.2,0.2-0.2,0.5-0.1c0.1,0,0.2,0.1,0.3,0.2c0.3,0.3,0.6,0.6,0.9,0.9c0.2,0.2,0.5,0.5,0.7,0.7c0.5,0.5,1,1,1.5,1.5c0.6,0.6,1.5,0.9,2.3,0.9c1.3,0,2.5,0,3.8-0.1l0.6,0c0.4,0,0.7-0.1,1-0.1c0.7-0.2,1.1-0.8,1-1.4c-0.1-0.4-0.1-0.8-0.4-1.2c-0.5-0.7-1.1-1.5-1.8-2.3c-0.5-0.5-1-1.1-1.6-1.6c-0.3-0.3-0.7-0.7-1-1c-0.5-0.5-0.5-0.8-0.1-1.3C29.5,17.3,30,16.6,30.6,15.9zM28.4,20.1c0.3,0.4,0.7,0.7,1,1c0.5,0.5,1,1,1.5,1.5c0.6,0.7,1.2,1.5,1.7,2.2c0.1,0.2,0.2,0.6,0.2,0.8c0,0.2-0.2,0.3-0.3,0.3C32.4,26,32.2,26,31.9,26l-0.6,0c-1.3,0-2.5,0.1-3.8,0.1c-0.6,0-1.2-0.2-1.7-0.7c-0.5-0.4-1-0.9-1.4-1.4c-0.2-0.2-0.5-0.5-0.7-0.7c-0.3-0.3-0.6-0.6-1-0.9c-0.2-0.2-0.3-0.3-0.5-0.3c-0.2-0.1-0.5-0.2-0.7-0.2c-0.3,0-0.6,0.1-0.9,0.4c-0.5,0.4-0.8,1-0.9,1.8c-0.1,0.3-0.1,0.7-0.1,1l0,0.4c0,0.5-0.3,0.5-0.5,0.6c-2.3,0.1-4.1,0.1-5.9-0.8c-1.9-1-3.4-2.2-4.5-3.5c-2.4-3-4.6-6.6-6.6-11.1C2.2,10.4,2.1,10.2,2,10c0-0.1,0-0.2,0-0.2c0,0,0.1-0.1,0.2-0.1c0.2,0,0.4,0,0.6,0c1.1,0,2.1,0,3.2,0l0.9,0c0,0,0,0,0,0c0.5,0,0.7,0.2,1,0.7c0.8,1.6,1.6,3.3,2.4,4.9c0.5,0.9,1.1,1.7,1.7,2.2c0.2,0.1,0.7,0.6,1.4,0.3c0.2-0.1,0.4-0.2,0.6-0.4c0.3-0.5,0.4-1.1,0.5-1.6c0.2-1.3,0.2-2.5,0.1-3.7c0-0.4-0.1-0.9-0.2-1.2c-0.2-0.7-0.6-1.2-1.4-1.5c0.2-0.1,0.4-0.2,0.7-0.3c1.5-0.2,3.1-0.3,4.9-0.1c0.1,0,0.3,0,0.4,0.1c0.5,0.1,0.6,0.2,0.7,0.9c0.1,0.4,0,0.9,0,1.3c0,0.2,0,0.4,0,0.7l0,1.8c0,0.8,0,1.5,0,2.3c0,0.5,0.2,0.9,0.4,1.1c0.2,0.4,0.6,0.5,0.8,0.6c0.2,0,0.6,0,1-0.3c0.6-0.5,1-1,1.6-1.8c1.1-1.7,2.1-3.5,2.8-5.3l0.1-0.1c0.2-0.5,0.2-0.5,0.7-0.5c0.9,0,1.7,0,2.6,0c0.9,0,1.7,0,2.6,0c0.2,0,0.5,0,0.7,0.1C33,9.9,33,9.9,33,10c-0.1,0.5-0.3,1-0.5,1.3c-0.6,1.2-1.5,2.4-2.3,3.5l-0.4,0.6c-0.5,0.7-1.1,1.4-1.5,2.1C27.6,18.4,27.6,19.3,28.4,20.1z"
                        :icon/zip "M32,9.2c0-0.6-0.2-1.2-0.7-1.6l-5.9-5.9C25,1.2,24.4,1,23.8,1L4.3,1C3.6,1,3,1.6,3,2.3l0,30.4C3,33.4,3.6,34,4.3,34l26.4,0c0.7,0,1.3-0.6,1.3-1.3L32,9.2zM30.7,33L4.3,33C4.1,33,4,32.9,4,32.7L4,2.3C4,2.1,4.1,2,4.3,2l19.5,0c0.3,0,0.7,0.1,0.9,0.4l5.9,5.9C30.8,8.5,31,8.9,31,9.2l0,23.5C31,32.9,30.9,33,30.7,33zM29.5,9.5c0,0.3-0.2,0.5-0.5,0.5l-4.9,0C23.5,10,23,9.5,23,8.9L23,4c0-0.3,0.2-0.5,0.5-0.5S24,3.7,24,4l0,4.9C24,9,24,9,24.1,9L29,9C29.3,9,29.5,9.2,29.5,9.5zM18,13.5c0,0.3-0.2,0.5-0.5,0.5S17,13.8,17,13.5s0.2-0.5,0.5-0.5S18,13.2,18,13.5zM18,11.5c0,0.3-0.2,0.5-0.5,0.5S17,11.8,17,11.5s0.2-0.5,0.5-0.5S18,11.2,18,11.5zM18,9.5c0,0.3-0.2,0.5-0.5,0.5S17,9.8,17,9.5C17,9.2,17.2,9,17.5,9S18,9.2,18,9.5zM18,7.5c0,0.3-0.2,0.5-0.5,0.5S17,7.8,17,7.5S17.2,7,17.5,7S18,7.2,18,7.5zM18,5.5c0,0.3-0.2,0.5-0.5,0.5S17,5.8,17,5.5C17,5.2,17.2,5,17.5,5S18,5.2,18,5.5zM18,3.5c0,0.3-0.2,0.5-0.5,0.5S17,3.8,17,3.5S17.2,3,17.5,3S18,3.2,18,3.5zM15.9,12.6c0-0.3,0.2-0.5,0.5-0.5s0.5,0.2,0.5,0.5s-0.2,0.5-0.5,0.5S15.9,12.9,15.9,12.6zM15.9,10.6c0-0.3,0.2-0.5,0.5-0.5s0.5,0.2,0.5,0.5s-0.2,0.5-0.5,0.5S15.9,10.9,15.9,10.6zM15.9,8.6c0-0.3,0.2-0.5,0.5-0.5S17,8.2,17,8.6c0,0.3-0.2,0.5-0.5,0.5S15.9,8.9,15.9,8.6zM15.9,6.6c0-0.3,0.2-0.5,0.5-0.5S17,6.2,17,6.6s-0.2,0.5-0.5,0.5S15.9,6.9,15.9,6.6zM15.9,4.6c0-0.3,0.2-0.5,0.5-0.5S17,4.2,17,4.6c0,0.3-0.2,0.5-0.5,0.5S15.9,4.9,15.9,4.6zM16,27.5c0,0.3-0.2,0.5-0.5,0.5h-3c-0.2,0-0.4-0.1-0.4-0.3s-0.1-0.4,0-0.5l2.4-3.2h-2c-0.3,0-0.5-0.2-0.5-0.5s0.2-0.5,0.5-0.5h3c0.2,0,0.4,0.1,0.4,0.3s0.1,0.4,0,0.5L13.5,27h2C15.8,27,16,27.2,16,27.5zM18,23.5v4c0,0.3-0.2,0.5-0.5,0.5S17,27.8,17,27.5v-4c0-0.3,0.2-0.5,0.5-0.5S18,23.2,18,23.5zM21.5,23h-2c-0.3,0-0.5,0.2-0.5,0.5v4c0,0.3,0.2,0.5,0.5,0.5s0.5-0.2,0.5-0.5V26h1.5c0.8,0,1.5-0.7,1.5-1.5S22.3,23,21.5,23zM21.5,25H20v-1h1.5c0.3,0,0.5,0.2,0.5,0.5S21.8,25,21.5,25zM11,27.5c0,0.3-0.2,0.5-0.5,0.5S10,27.8,10,27.5c0-0.3,0.2-0.5,0.5-0.5S11,27.2,11,27.5zM18,15.5v2.9c0,0.3-0.2,0.5-0.5,0.5h-0.9c-0.3,0-0.5-0.2-0.5-0.5v-2.9c0-0.3,0.2-0.5,0.5-0.5h0.9C17.8,15,18,15.2,18,15.5z"
                        :icon/zip-hover "M31.4,29.6c0.2,0.2,0.2,0.5,0,0.7l-3.5,3.5c0,0-0.1,0.1-0.2,0.1c-0.1,0-0.1,0-0.2,0s-0.1,0-0.2,0c-0.1,0-0.1-0.1-0.2-0.1l-3.5-3.5c-0.2-0.2-0.2-0.5,0-0.7s0.5-0.2,0.7,0l2.7,2.7v-6.8c0-0.3,0.2-0.5,0.5-0.5s0.5,0.2,0.5,0.5v6.8l2.7-2.7C30.9,29.4,31.2,29.4,31.4,29.6zM32,9.2c0-0.6-0.2-1.2-0.7-1.6l-5.9-5.9C25,1.2,24.4,1,23.8,1L4.3,1C3.6,1,3,1.6,3,2.3l0,30.4C3,33.4,3.6,34,4.3,34h19.2c0.3,0,0.5-0.2,0.5-0.5c0-0.3-0.2-0.5-0.5-0.5H4.3C4.1,33,4,32.9,4,32.7L4,2.3C4,2.1,4.1,2,4.3,2l19.5,0c0.3,0,0.7,0.1,0.9,0.4l5.9,5.9C30.8,8.5,31,8.9,31,9.2l0,18.3c0,0.3,0.2,0.5,0.5,0.5s0.5-0.2,0.5-0.5L32,9.2zM29,10c0.3,0,0.5-0.2,0.5-0.5C29.5,9.2,29.3,9,29,9l-4.9,0C24,9,24,9,24,8.9L24,4c0-0.3-0.2-0.5-0.5-0.5S23,3.7,23,4l0,4.9c0,0.6,0.5,1.1,1.1,1.1L29,10zM17.5,13c-0.3,0-0.5,0.2-0.5,0.5s0.2,0.5,0.5,0.5s0.5-0.2,0.5-0.5S17.8,13,17.5,13zM17.5,11c-0.3,0-0.5,0.2-0.5,0.5s0.2,0.5,0.5,0.5s0.5-0.2,0.5-0.5S17.8,11,17.5,11zM17.5,9C17.2,9,17,9.2,17,9.5c0,0.3,0.2,0.5,0.5,0.5S18,9.8,18,9.5C18,9.2,17.8,9,17.5,9zM17.5,7C17.2,7,17,7.2,17,7.5s0.2,0.5,0.5,0.5S18,7.8,18,7.5S17.8,7,17.5,7zM17.5,5C17.2,5,17,5.2,17,5.5c0,0.3,0.2,0.5,0.5,0.5S18,5.8,18,5.5C18,5.2,17.8,5,17.5,5zM17.5,3C17.2,3,17,3.2,17,3.5s0.2,0.5,0.5,0.5S18,3.8,18,3.5S17.8,3,17.5,3zM16.5,13.1c0.3,0,0.5-0.2,0.5-0.5S16.8,12,16.5,12s-0.5,0.2-0.5,0.5S16.2,13.1,16.5,13.1zM16.5,11.1c0.3,0,0.5-0.2,0.5-0.5S16.8,10,16.5,10s-0.5,0.2-0.5,0.5S16.2,11.1,16.5,11.1zM16.5,9.1c0.3,0,0.5-0.2,0.5-0.5C17,8.2,16.8,8,16.5,8s-0.5,0.2-0.5,0.5C15.9,8.9,16.2,9.1,16.5,9.1zM16.5,7.1c0.3,0,0.5-0.2,0.5-0.5S16.8,6,16.5,6s-0.5,0.2-0.5,0.5S16.2,7.1,16.5,7.1zM16.5,5.1c0.3,0,0.5-0.2,0.5-0.5C17,4.2,16.8,4,16.5,4s-0.5,0.2-0.5,0.5C15.9,4.9,16.2,5.1,16.5,5.1zM12.1,27.7c0.1,0.2,0.3,0.3,0.4,0.3h3c0.3,0,0.5-0.2,0.5-0.5S15.8,27,15.5,27h-2l2.4-3.2c0.1-0.2,0.1-0.4,0-0.5S15.7,23,15.5,23h-3c-0.3,0-0.5,0.2-0.5,0.5s0.2,0.5,0.5,0.5h2l-2.4,3.2C12,27.4,12,27.6,12.1,27.7zM17,23.5v4c0,0.3,0.2,0.5,0.5,0.5s0.5-0.2,0.5-0.5v-4c0-0.3-0.2-0.5-0.5-0.5S17,23.2,17,23.5zM21.5,23c0.8,0,1.5,0.7,1.5,1.5S22.3,26,21.5,26H20v1.5c0,0.3-0.2,0.5-0.5,0.5S19,27.8,19,27.5v-4c0-0.3,0.2-0.5,0.5-0.5H21.5zM21.5,24H20v1h1.5c0.3,0,0.5-0.2,0.5-0.5S21.8,24,21.5,24zM10,27.5c0,0.3,0.2,0.5,0.5,0.5s0.5-0.2,0.5-0.5c0-0.3-0.2-0.5-0.5-0.5S10,27.2,10,27.5zM18,15.5c0-0.3-0.2-0.5-0.5-0.5h-0.9c-0.3,0-0.5,0.2-0.5,0.5v2.9c0,0.3,0.2,0.5,0.5,0.5h0.9c0.3,0,0.5-0.2,0.5-0.5V15.5z"))
      (dissoc :type)
      (provide-defaults :fill (case type
                                :icon/circle-check "#65c647"
                                (:icon/triangle-error :icon/triangle-sad) "#e32f44"
                                "#9fb0be"))))

(deftype PartialFn [pfn fn args]
  Fn
  IFn
  (invoke [_]
    (pfn))
  (invoke [_ a]
    (pfn a))
  (invoke [_ a b]
    (pfn a b))
  (invoke [_ a b c]
    (pfn a b c))
  (invoke [_ a b c d]
    (pfn a b c d))
  (invoke [_ a b c d e]
    (pfn a b c d e))
  (invoke [_ a b c d e f]
    (pfn a b c d e f))
  (invoke [_ a b c d e f g]
    (pfn a b c d e f g))
  (invoke [_ a b c d e f g h]
    (pfn a b c d e f g h))
  (invoke [_ a b c d e f g h i]
    (pfn a b c d e f g h i))
  (invoke [_ a b c d e f g h i j]
    (pfn a b c d e f g h i j))
  (invoke [_ a b c d e f g h i j k]
    (pfn a b c d e f g h i j k))
  (invoke [_ a b c d e f g h i j k l]
    (pfn a b c d e f g h i j k l))
  (invoke [_ a b c d e f g h i j k l m]
    (pfn a b c d e f g h i j k l m))
  (invoke [_ a b c d e f g h i j k l m n]
    (pfn a b c d e f g h i j k l m n))
  (invoke [_ a b c d e f g h i j k l m n o]
    (pfn a b c d e f g h i j k l m n o))
  (invoke [_ a b c d e f g h i j k l m n o p]
    (pfn a b c d e f g h i j k l m n o p))
  (invoke [_ a b c d e f g h i j k l m n o p q]
    (pfn a b c d e f g h i j k l m n o p q))
  (invoke [_ a b c d e f g h i j k l m n o p q r]
    (pfn a b c d e f g h i j k l m n o p q r))
  (invoke [_ a b c d e f g h i j k l m n o p q r s]
    (pfn a b c d e f g h i j k l m n o p q r s))
  (invoke [_ a b c d e f g h i j k l m n o p q r s t]
    (pfn a b c d e f g h i j k l m n o p q r s t))
  (invoke [_ a b c d e f g h i j k l m n o p q r s t rest]
    (apply pfn a b c d e f g h i j k l m n o p q r s t rest))
  (applyTo [_ arglist]
    (apply pfn arglist))
  IHashEq
  (hasheq [_]
    (hash [fn args]))
  Object
  (equals [_ obj]
    (if (instance? PartialFn obj)
      (let [^PartialFn that obj]
        (and (= fn (.-fn that))
             (= args (.-args that))))
      false)))

(defn partial [f & args]
  (PartialFn. (apply clojure.core/partial f args) f args))
