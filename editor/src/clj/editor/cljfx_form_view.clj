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

(ns editor.cljfx-form-view
  (:require [cljfx.api :as fx]
            [cljfx.ext.list-view :as fx.ext.list-view]
            [cljfx.ext.table-view :as fx.ext.table-view]
            [cljfx.fx.anchor-pane :as fx.anchor-pane]
            [cljfx.fx.button :as fx.button]
            [cljfx.fx.check-box :as fx.check-box]
            [cljfx.fx.column-constraints :as fx.column-constraints]
            [cljfx.fx.combo-box :as fx.combo-box]
            [cljfx.fx.context-menu :as fx.context-menu]
            [cljfx.fx.grid-pane :as fx.grid-pane]
            [cljfx.fx.h-box :as fx.h-box]
            [cljfx.fx.image-view :as fx.image-view]
            [cljfx.fx.label :as fx.label]
            [cljfx.fx.list-view :as fx.list-view]
            [cljfx.fx.menu-item :as fx.menu-item]
            [cljfx.fx.scroll-pane :as fx.scroll-pane]
            [cljfx.fx.separator :as fx.separator]
            [cljfx.fx.stack-pane :as fx.stack-pane]
            [cljfx.fx.table-column :as fx.table-column]
            [cljfx.fx.table-view :as fx.table-view]
            [cljfx.fx.text-field :as fx.text-field]
            [cljfx.fx.text-formatter :as fx.text-formatter]
            [cljfx.fx.tooltip :as fx.tooltip]
            [cljfx.fx.v-box :as fx.v-box]
            [clojure.set :as set]
            [clojure.string :as string]
            [dynamo.graph :as g]
            [editor.dialogs :as dialogs]
            [editor.error-reporting :as error-reporting]
            [editor.field-expression :as field-expression]
            [editor.form :as form]
            [editor.fxui :as fxui]
            [editor.icons :as icons]
            [editor.handler :as handler]
            [editor.resource :as resource]
            [editor.resource-dialog :as resource-dialog]
            [editor.settings :as settings]
            [editor.ui :as ui]
            [editor.url :as url]
            [editor.view :as view]
            [editor.workspace :as workspace]
            [internal.util :as util])
  (:import [javafx.event Event]
           [javafx.scene Node]
           [javafx.scene.control Cell ComboBox ListView$EditEvent TableColumn$CellEditEvent TableView ListView]
           [javafx.scene.input KeyCode KeyEvent]
           [javafx.util StringConverter]))

(set! *warn-on-reflection* true)

(def ^:private line-height 27)

(def ^:private cell-height (inc line-height))

(g/defnk produce-form-view [renderer form-data ui-state]
  (renderer {:form-data form-data
             :ui-state ui-state})
  nil)

(g/defnode CljfxFormView
  (inherits view/WorkbenchView)
  (input form-data g/Any :substitute {})
  (property ui-state g/Any (default {:filter-term ""}))
  (property renderer g/Any)
  (output form-view g/Any :cached produce-form-view))

(defmulti handle-event :event-type)

(defmethod handle-event :default [e]
  (prn :handle
       (:event-type e)
       (dissoc e :event-type :ui-state :form-data :workspace :project :parent)))

(defmethod handle-event :clear [{:keys [path]}]
  {:clear path})

(defmethod handle-event :set [{:keys [path fx/event]}]
  {:set [path event]})

(def with-anchor-pane-props
  (fx/make-ext-with-props fx.anchor-pane/props))

(def uri-string-converter
  (proxy [StringConverter] []
    (toString
      ([] "uri-string-converter")
      ([v] (str v)))
    (fromString [v]
      (url/try-parse v))))

(def number-converter
  (proxy [StringConverter] []
    (toString
      ([] "number-string-converter")
      ([v] (field-expression/format-number v)))
    (fromString [v]
      (or (field-expression/to-double v)
          (throw (RuntimeException.))))))

(def int-converter
  (proxy [StringConverter] []
    (toString
      ([] "int-string-converter")
      ([v] (field-expression/format-int v)))
    (fromString [v]
      (or (int (field-expression/to-double v))
          (throw (RuntimeException.))))))

(defn- make-resource-string-converter [workspace]
  (proxy [StringConverter] []
    (toString
      ([] "resource-string-converter")
      ([v] (resource/resource->proj-path v)))
    (fromString [v]
      (some->> (when-not (string/blank? v) v)
               (workspace/to-absolute-path)
               (workspace/resolve-workspace-resource workspace)))))

(defn- contains-ignore-case? [^String str ^String sub]
  (let [sub-length (.length sub)]
    (if (zero? sub-length)
      true
      (let [str-length (.length str)]
        (loop [i 0]
          (cond
            (= i str-length) false
            (.regionMatches str true i sub 0 sub-length) true
            :else (recur (inc i))))))))

(defn- text-field [props]
  (assoc props :fx/type fx.text-field/lifecycle
               :style-class ["text-field" "cljfx-form-text-field"]))

(defn- add-image-fit-size [{:keys [fit-size] :as props}]
  (-> props
      (dissoc :fit-size)
      (update :graphic assoc :fit-width fit-size :fit-height fit-size)))

(defn- add-image [props]
  (cond-> (-> props
              (dissoc :image)
              (assoc :graphic {:fx/type fx.image-view/lifecycle
                               :image (icons/get-image (:image props))}))

          (contains? props :fit-size)
          add-image-fit-size))

(defn- icon-button [props]
  (cond-> (assoc props :fx/type fx.button/lifecycle
                       :style-class ["button" "cljfx-form-icon-button"])

          (contains? props :image)
          add-image))

(defmulti form-input-view
  "Form input component function

  Expects a form field with specific keys for each `:type`, plus:
  - `:value` - current-value
  - `:on-value-changed` - a map event that will get notification of new value
  - `:state` - this component's local state
  - `:state-path` - a path for `assoc-in` used by component to change it's state
  - `:resource-string-converter` - a resource string converter"
  :type)

(defmethod form-input-view :default [{:keys [value type] :as field}]
  {:fx/type fx.label/lifecycle
   :wrap-text true
   :text (str type " " (if (nil? value) "***NIL***" value) " " field)})

(defmulti cell-input-view
  "Analogous to form input, but displayed in a cell (in list-view or table-view)

  Expects same keys as [[form-input-view]], but `:on-value-changed` is used for
  temporarily saving value while continuing edit. Additional keys:
  - `:on-commit` - a map event that will get notification with committed value
  - `:on-cancel` - a map event that will get notification if user cancels edit"
  :type)

(def ^:private ext-with-key-pressed-props
  (fx/make-ext-with-props
    {:on-filter-key-pressed (fxui/make-event-filter-prop KeyEvent/KEY_PRESSED)
     :on-key-pressed (fxui/make-event-handler-prop KeyEvent/KEY_PRESSED)}))

(defmethod handle-event :cancel-on-escape [{:keys [on-cancel ^KeyEvent fx/event]}]
  (when (= KeyCode/ESCAPE (.getCode event))
    {:dispatch (assoc on-cancel :fx/event event)}))

(defn- wrap-cancel-on-escape [desc on-cancel]
  {:fx/type ext-with-key-pressed-props
   :props {:on-filter-key-pressed {:event-type :cancel-on-escape
                                   :on-cancel on-cancel}}
   :desc desc})

(defn- default-cell-input-view [field]
  (wrap-cancel-on-escape
    (assoc field :fx/type form-input-view
                 :on-value-changed (:on-commit field))
    (:on-cancel field)))

(defmethod cell-input-view :default [field]
  (default-cell-input-view field))

(defn- focus-text-field-when-on-scene! [^Node node]
  (fxui/focus-when-on-scene! (.lookup node "TextField")))

(defn- wrap-focus-text-field [desc]
  {:fx/type fx/ext-on-instance-lifecycle
   :on-created focus-text-field-when-on-scene!
   :desc desc})

;; region string input

(defmethod form-input-view :string [{:keys [value on-value-changed]}]
  {:fx/type text-field
   :text-formatter {:fx/type fx.text-formatter/lifecycle
                    :value-converter :default
                    :value value
                    :on-value-changed on-value-changed}})

(defmethod cell-input-view :string [field]
  (wrap-focus-text-field (default-cell-input-view field)))

;; endregion

;; region boolean input

(defn- ensure-value [value field]
  (if (nil? value) (form/field-default field) value))

(defmethod form-input-view :boolean [{:keys [value on-value-changed] :as field}]
  {:fx/type fx.check-box/lifecycle
   :style-class ["check-box" "cljfx-form-check-box"]
   :selected (ensure-value value field)
   :on-selected-changed on-value-changed})

;; endregion

;; region integer input

(defmethod form-input-view :integer [{:keys [value on-value-changed] :as field}]
  {:fx/type text-field
   :alignment :center-right
   :max-width 80
   :text-formatter {:fx/type fx.text-formatter/lifecycle
                    :value-converter int-converter
                    :value (int (ensure-value value field))
                    :on-value-changed on-value-changed}})

(defmethod cell-input-view :integer [field]
  (wrap-focus-text-field (default-cell-input-view field)))

;; endregion

;; region number input

(defmethod form-input-view :number [{:keys [value on-value-changed] :as field}]
  {:fx/type text-field
   :alignment :center-right
   :max-width 80
   :text-formatter {:fx/type fx.text-formatter/lifecycle
                    :value-converter number-converter
                    :value (ensure-value value field)
                    :on-value-changed on-value-changed}})

(defmethod cell-input-view :number [field]
  (wrap-focus-text-field (default-cell-input-view field)))

;; endregion

;; region url input

(defmethod handle-event :skip-malformed-urls [{:keys [fx/event on-value-changed]}]
  (when event
    {:dispatch (assoc on-value-changed :fx/event event)}))

(defmethod form-input-view :url [{:keys [value on-value-changed]}]
  {:fx/type text-field
   :text-formatter {:fx/type fx.text-formatter/lifecycle
                    :value-converter uri-string-converter
                    :value value
                    :on-value-changed {:event-type :skip-malformed-urls
                                       :on-value-changed on-value-changed}}})

(defmethod cell-input-view :url [field]
  (wrap-focus-text-field (default-cell-input-view field)))

;; endregion

;; region vec4 input

(defmethod handle-event :on-vec4-element-change [{:keys [value index on-value-changed fx/event]}]
  {:dispatch (assoc on-value-changed :fx/event (assoc value index event))})

(defmethod form-input-view :vec4 [{:keys [value on-value-changed] :as field}]
  (let [labels ["X" "Y" "Z" "W"]
        value (ensure-value value field)]
    {:fx/type fx.h-box/lifecycle
     :padding {:left 5}
     :spacing 5
     :children (into []
                     (map-indexed
                       (fn [i n]
                         {:fx/type fx.h-box/lifecycle
                          :alignment :center
                          :spacing 5
                          :children [{:fx/type fx.label/lifecycle
                                      :min-width :use-pref-size
                                      :text (get labels i)}
                                     {:fx/type form-input-view
                                      :type :number
                                      :h-box/hgrow :always
                                      :value n
                                      :on-value-changed {:event-type :on-vec4-element-change
                                                         :on-value-changed on-value-changed
                                                         :value value
                                                         :index i}}]}))
                     value)}))

(defmethod handle-event :keep-edit [{:keys [state-path ui-state fx/event on-value-changed]}]
  [[:set-ui-state (assoc-in ui-state (conj state-path :value) event)]
   [:dispatch (assoc on-value-changed :fx/event event)]])

(defmethod handle-event :commit-on-enter [{:keys [^KeyEvent fx/event on-commit state-path ui-state]}]
  (when (and (= KeyCode/ENTER (.getCode event)))
    (let [value (get-in ui-state (conj state-path :value) ::no-value)]
      (when-not (= value ::no-value)
        {:dispatch (assoc on-commit :fx/event value)}))))

(defn- wrap-commit-on-enter [desc on-commit state-path]
  {:fx/type ext-with-key-pressed-props
   :props {:on-key-pressed {:event-type :commit-on-enter
                            :on-commit on-commit
                            :state-path state-path}}
   :desc desc})

(defmethod cell-input-view :vec4 [{:keys [on-cancel
                                          on-commit
                                          on-value-changed
                                          state
                                          state-path]
                                   :as field}]
  (-> field
      (assoc :fx/type form-input-view
             :state-path (conj state-path :form-input-state)
             :on-value-changed {:event-type :keep-edit
                                :state-path state-path
                                :on-value-changed on-value-changed})
      (cond-> (contains? state :form-input-state)
              (assoc :state (:form-input-state state)))
      (wrap-cancel-on-escape on-cancel)
      (wrap-commit-on-enter on-commit state-path)
      (wrap-focus-text-field)))

;; endregion

;; region choicebox input

(defmethod form-input-view :choicebox [{:keys [value
                                               on-value-changed
                                               options
                                               from-string
                                               to-string]
                                        :or {to-string str}}]
  (let [value->label (into {} options)
        label->value (set/map-invert value->label)]
    {:fx/type fx.combo-box/lifecycle
     :style-class ["combo-box" "combo-box-base" "cljfx-form-combo-box"]
     :value value
     :on-value-changed on-value-changed
     :converter (proxy [StringConverter] []
                  (toString
                    ([] "string-converter")
                    ([value]
                     (get value->label value (to-string value))))
                  (fromString [s]
                    (get label->value s (and from-string (from-string s)))))
     :editable (some? from-string)
     :button-cell (fn [x]
                    {:text (value->label x)})
     :cell-factory (fn [x]
                     {:text (value->label x)})
     :items (sort (mapv first options))}))

(defn- show-combo-box! [^ComboBox combo-box]
  (.show combo-box))

(defmethod cell-input-view :choicebox [field]
  {:fx/type fx/ext-on-instance-lifecycle
   :on-created show-combo-box!
   :desc (default-cell-input-view field)})

;; endregion

;; region list view input

(defmethod handle-event :on-list-edit-start [{:keys [^ListView$EditEvent fx/event state-path ui-state]}]
  {:set-ui-state (assoc-in ui-state (conj state-path :edit) {:index (.getIndex event)
                                                             :list (.getTarget event)})})

(defmethod handle-event :on-list-edit-cancel [{:keys [state-path ui-state]}]
  {:set-ui-state (update-in ui-state state-path dissoc :edit)})

(defmethod handle-event :commit-list-item [{:keys [index fx/event on-edited state-path ui-state]}]
  (ui/run-later
    (.select (.getSelectionModel ^ListView (get-in ui-state (conj state-path :edit :list)))
             ^int index))
  {:set-ui-state (update-in ui-state state-path dissoc :edit)
   :dispatch (assoc on-edited :fx/event {:index index :item event})})

(defmethod handle-event :add-list-element [{:keys [value
                                                   on-added
                                                   state-path
                                                   ui-state
                                                   element]}]
  [[:dispatch (assoc on-added :fx/event [element])]
   [:set-ui-state (assoc-in ui-state (conj state-path :selected-indices) [(count value)])]])

(defmethod handle-event :add-list-resource [{:keys [workspace
                                                    project
                                                    filter
                                                    value
                                                    on-added
                                                    state-path
                                                    ui-state]}]
  (let [resources (resource-dialog/make workspace project {:ext filter
                                                           :selection :multiple})]
    (when-not (empty? resources)
      (let [value-count (count value)
            added-count (count resources)
            indices (vec (range value-count
                                (+ value-count added-count)))]
        [[:dispatch (assoc on-added :fx/event resources)]
         [:set-ui-state (assoc-in ui-state (conj state-path :selected-indices) indices)]]))))

(defmethod handle-event :remove-list-selection [{:keys [on-removed state-path ui-state]}]
  (let [indices-path (conj state-path :selected-indices)]
    [[:dispatch (assoc on-removed :fx/event (set (get-in ui-state indices-path)))]
     [:set-ui-state (assoc-in ui-state indices-path [])]]))

(defmethod handle-event :list-select [{:keys [state-path fx/event ui-state]}]
  {:set-ui-state (assoc-in ui-state (conj state-path :selected-indices) event)})

(def ^:private ext-with-list-cell-factory-props
  (fx/make-ext-with-props
    {:cell-factory fxui/list-cell-factory-prop}))

(defmethod handle-event :cancel-list-edit [{:keys [^KeyEvent fx/event
                                                   ui-state
                                                   state-path]}]
  (when-let [cell (ui/closest-node-of-type ListView (.getTarget event))]
    [[:set-ui-state (update-in ui-state state-path dissoc :edit)]
     [:cancel-edit cell]]))

(defmethod handle-event :keep-list-edit [{:keys [ui-state state-path fx/event]}]
  {:set-ui-state (assoc-in ui-state (conj state-path :edit :value) event)})

(defn- edited-list-cell-view [element {:keys [state-path state on-edited] :as field} item]
  (let [edit (:edit state)]
    (-> element
        (assoc :fx/type cell-input-view
               :value item
               :on-cancel {:event-type :cancel-list-edit
                           :state-path state-path}
               :on-value-changed {:event-type :keep-list-edit
                                  :state-path state-path}
               :on-commit {:event-type :commit-list-item
                           :state-path state-path
                           :on-edited on-edited
                           :index (:index edit)}
               :resource-string-converter (:resource-string-converter field)
               :state-path (conj state-path :edit :state))
        (cond-> (contains? edit :state)
                (assoc :state (:state edit))))))

(defn- list-cell-factory [element edit-index [i v]]
  (let [edited (= edit-index i)
        ref {:fx/type fx/ext-get-ref :ref [::edit edit-index]}]
    (if edited
      (case (:type element)
        :url
        {:style {:-fx-padding 0}
         :graphic ref}

        :resource
        {:style {:-fx-padding [2 2 2 0]}
         :graphic ref}

        {:graphic ref})
      {:text (case (:type element)
               :choicebox (get (into {} (:options element)) v)
               :resource (resource/resource->proj-path v)
               (str v))})))

(defn- list-input [{:keys [;; state
                           state
                           state-path
                           ;; value
                           value
                           on-added                         ;; [item ...]
                           on-edited                        ;; {:index int :item item}
                           on-removed                       ;; #{index ...}
                           ;; field
                           element]
                    :or {state {:selected-indices []}
                         value []}
                    :as field}]
  (let [{:keys [selected-indices edit]} state
        disable-add (not (form/has-default? element))
        disable-remove (empty? selected-indices)
        add-event (if (= :resource (:type element))
                    {:event-type :add-list-resource
                     :value value
                     :on-added on-added
                     :state-path state-path
                     :filter (:filter element)}
                    {:event-type :add-list-element
                     :value value
                     :on-added on-added
                     :state-path state-path
                     :element (form/field-default element)})
        remove-event {:event-type :remove-list-selection
                      :on-removed on-removed
                      :state-path state-path}]
    {:fx/type fx.v-box/lifecycle
     :spacing 4
     :children [{:fx/type fx/ext-let-refs
                 :refs (when edit
                         {[::edit (:index edit)] (edited-list-cell-view
                                                   element
                                                   field
                                                   (get value (:index edit)))})
                 :desc {:fx/type fxui/ext-with-advance-events
                        :desc
                        {:fx/type ext-with-list-cell-factory-props
                         :props {:cell-factory (fxui/partial list-cell-factory
                                                             element
                                                             (:index edit))}
                         :desc
                         {:fx/type fx.ext.list-view/with-selection-props
                          :props {:selection-mode :multiple
                                  :selected-indices selected-indices
                                  :on-selected-indices-changed {:event-type :list-select
                                                                :state-path state-path}}
                          :desc
                          {:fx/type fx.list-view/lifecycle
                           :style-class ["list-view" "cljfx-form-list-view"]
                           :items (into [] (map-indexed vector) value)
                           :editable true
                           :on-edit-start {:event-type :on-list-edit-start
                                           :state-path state-path}
                           :on-edit-cancel {:event-type :on-list-edit-cancel
                                            :state-path state-path}
                           :pref-height (+ 2                ;; top and bottom insets
                                           1                ;; bottom padding
                                           9                ;; horizontal scrollbar height
                                           (* cell-height
                                              (max (count value) 1)))
                           :fixed-cell-size cell-height
                           :context-menu {:fx/type fx.context-menu/lifecycle
                                          :items [{:fx/type fx.menu-item/lifecycle
                                                   :text "Add"
                                                   :disable disable-add
                                                   :on-action add-event}
                                                  {:fx/type fx.menu-item/lifecycle
                                                   :text "Remove"
                                                   :disable disable-remove
                                                   :on-action remove-event}]}}}}}}
                {:fx/type fx.h-box/lifecycle
                 :spacing 4
                 :children [{:fx/type icon-button
                             :disable disable-add
                             :on-action add-event
                             :image "icons/32/Icons_M_07_plus.png"
                             :fit-size 16}
                            {:fx/type icon-button
                             :disable disable-remove
                             :on-action remove-event
                             :image "icons/32/Icons_M_11_minus.png"
                             :fit-size 16}]}]}))

(defmethod handle-event :add-list-items [{:keys [value on-value-changed fx/event]}]
  {:dispatch (assoc on-value-changed :fx/event (into value event))})

(defn- keep-indices [indices coll]
  (into []
        (keep-indexed
          (fn [i x]
            (when (indices i) x)))
        coll))

(defn- remove-indices [indices coll]
  (keep-indices (complement indices) coll))

(defmethod handle-event :remove-list-items [{:keys [value on-value-changed fx/event]}]
  {:dispatch (assoc on-value-changed :fx/event (remove-indices event value))})

(defmethod handle-event :edit-list-item [{:keys [value on-value-changed fx/event]}]
  (let [{:keys [index item]} event]
    {:dispatch (assoc on-value-changed :fx/event (assoc value index item))}))

(defmethod form-input-view :list [{:keys [value on-value-changed]
                                   :or {value []}
                                   :as field}]
  (assoc field :fx/type list-input
               :on-edited {:event-type :edit-list-item
                           :value value
                           :on-value-changed on-value-changed}
               :on-added {:event-type :add-list-items
                          :value value
                          :on-value-changed on-value-changed}
               :on-removed {:event-type :remove-list-items
                            :value value
                            :on-value-changed on-value-changed}))

;; endregion

;; region resource input

(defmethod handle-event :open-resource [{:keys [^Event fx/event value]}]
  {:open-resource [(.getTarget event) value]})

(defmethod handle-event :on-resource-selected [{:keys [workspace
                                                       project
                                                       filter
                                                       on-value-changed]}]
  (when-let [resource (first (resource-dialog/make workspace project {:ext filter}))]
    {:dispatch (assoc on-value-changed :fx/event resource)}))

(defmethod form-input-view :resource [{:keys [value
                                              on-value-changed
                                              filter
                                              resource-string-converter]}]
  {:fx/type fx.h-box/lifecycle
   :spacing 4
   :children [{:fx/type text-field
               :h-box/hgrow :always
               :text-formatter {:fx/type fx.text-formatter/lifecycle
                                :value-converter resource-string-converter
                                :value value
                                :on-value-changed on-value-changed}}
              {:fx/type icon-button
               :disable (not (and (resource/openable-resource? value)
                                  (resource/exists? value)))
               :image "icons/32/Icons_S_14_linkarrow.png"
               :fit-size 22
               :on-action {:event-type :open-resource
                           :value value}}
              {:fx/type icon-button
               :text "\u2026"
               :on-action {:event-type :on-resource-selected
                           :on-value-changed on-value-changed
                           :filter filter}}]})

(defmethod cell-input-view :resource [field]
  (wrap-focus-text-field (default-cell-input-view field)))

;; endregion

;; region file input

(defmethod handle-event :on-file-selected [{:keys [on-value-changed
                                                   filter
                                                   ^Event fx/event
                                                   title]}]
  (when-let [file (dialogs/make-file-dialog (or title "Select File")
                                            filter
                                            nil
                                            (.getWindow (.getScene ^Node (.getSource event))))]
    {:dispatch (assoc on-value-changed :fx/event (.getAbsolutePath file))}))

(defmethod form-input-view :file [{:keys [on-value-changed value filter title]}]
  {:fx/type fx.h-box/lifecycle
   :spacing 4
   :children [{:fx/type text-field
               :h-box/hgrow :always
               :text-formatter {:fx/type fx.text-formatter/lifecycle
                                :value-converter :default
                                :value value
                                :on-value-changed on-value-changed}}
              {:fx/type icon-button
               :text "\u2026"
               :on-action {:event-type :on-file-selected
                           :on-value-changed on-value-changed
                           :filter filter
                           :title title}}]})

;; endregion

;; region directory input

(defmethod handle-event :on-directory-selected [{:keys [on-value-changed title]}]
  (when-let [file (ui/choose-directory (or title "Select Directory") nil)]
    {:dispatch (assoc on-value-changed :fx/event file)}))

(defmethod form-input-view :directory [{:keys [on-value-changed value title]}]
  {:fx/type fx.h-box/lifecycle
   :spacing 4
   :children [{:fx/type text-field
               :h-box/hgrow :always
               :text-formatter {:fx/type fx.text-formatter/lifecycle
                                :value-converter :default
                                :value value
                                :on-value-changed on-value-changed}}
              {:fx/type icon-button
               :text "\u2026"
               :on-action {:event-type :on-directory-selected
                           :on-value-changed on-value-changed
                           :title title}}]})

;; endregion

;; region table input

(defmethod handle-event :on-table-edit-start [{:keys [^TableColumn$CellEditEvent fx/event
                                                      column-path
                                                      ui-state
                                                      state-path]}]
  (let [index (.getRow (.getTablePosition event))]
    {:set-ui-state (update-in ui-state
                              (conj state-path :edit)
                              (fn [edit]
                                (if (and (= (:index edit) index)
                                         (= (:path edit) column-path))
                                  edit
                                  {:index index
                                   :path column-path
                                   :table (.getTableView event)})))}))

(defn- value-with-edit [value edit]
  (let [edit-path (into [(:index edit)] (:path edit))]
    (assoc-in value edit-path (:value edit))))

(defmethod handle-event :on-table-edit-cancel [{:keys [value
                                                       on-value-changed
                                                       ui-state
                                                       state-path]}]
  (let [edit (get-in ui-state (conj state-path :edit))]
    (cond-> [[:set-ui-state (update-in ui-state state-path dissoc :edit)]]

            (some? (:value edit))
            (conj [:dispatch (assoc on-value-changed :fx/event (value-with-edit value edit))]))))

(defmethod handle-event :commit-table-edit [{:keys [fx/event
                                                    edit-path
                                                    ui-state
                                                    state-path
                                                    value
                                                    on-value-changed]}]
  (.edit ^TableView (get-in ui-state (conj state-path :edit :table)) -1 nil)
  {:dispatch (assoc on-value-changed :fx/event (assoc-in value edit-path event))})

(defmethod handle-event :keep-table-edit [{:keys [ui-state state-path fx/event]}]
  (let [state (get-in ui-state state-path)]
    (when (contains? state :edit)
      {:set-ui-state (assoc-in ui-state (conj state-path :edit :value) event)})))

(defmethod handle-event :cancel-table-edit [{:keys [^KeyEvent fx/event
                                                    ui-state
                                                    state-path]}]
  (when-let [^Cell cell (ui/closest-node-of-type Cell (.getTarget event))]
    [[:set-ui-state (update-in ui-state state-path dissoc :edit)]
     [:cancel-edit cell]]))

(defmethod handle-event :on-table-element-added [{:keys [ui-state
                                                         state-path
                                                         value
                                                         on-value-changed
                                                         element]}]
  (let [new-value (into value [element])]
    [[:dispatch (assoc on-value-changed :fx/event new-value)]
     [:set-ui-state (assoc-in ui-state
                              (conj state-path :selected-indices)
                              [(dec (count new-value))])]]))

(defmethod handle-event :remove-table-selection [{:keys [value
                                                         on-value-changed
                                                         ui-state
                                                         state-path]}]
  (let [indices-path (conj state-path :selected-indices)
        selected-indices (set (get-in ui-state indices-path))
        new-value (remove-indices selected-indices value)]
    [[:dispatch (assoc on-value-changed :fx/event new-value)]
     [:set-ui-state (assoc-in ui-state indices-path [])]]))

(defmethod handle-event :table-select [{:keys [ui-state state-path fx/event]}]
  {:set-ui-state (assoc-in ui-state (conj state-path :selected-indices) event)})

(defn- edited-table-cell-view [column
                               {:keys [value
                                       on-value-changed
                                       state-path
                                       state
                                       resource-string-converter]
                                :or {value []}}
                               item]
  (let [{:keys [edit]} state
        edit-path (into [(:index edit)] (:path column))
        input-state-path (conj state-path :edit :state)]
      (-> column
          (assoc :fx/type cell-input-view
                 :value item
                 :on-value-changed {:event-type :keep-table-edit
                                    :state-path state-path}
                 :on-cancel {:event-type :cancel-table-edit
                             :state-path state-path}
                 :on-commit {:event-type :commit-table-edit
                             :edit-path edit-path
                             :state-path state-path
                             :value value
                             :on-value-changed on-value-changed}
                 :resource-string-converter resource-string-converter
                 :state-path input-state-path)
          (cond-> (contains? edit :state)
                  (assoc :state (:state edit))))))

(defn- table-cell-value-factory [path [i x]]
  [i (get-in x path)])

(defn- table-cell-factory [{:keys [path type] :as column} edit [i x]]
  (let [edited (and (= i (:index edit))
                    (= path (:path edit)))
        ref {:fx/type fx/ext-get-ref :ref [::edit (:index edit) (:path edit)]}]
    (if edited
      (-> type
          (case
            :choicebox {:style {:-fx-padding -2}}
            :vec4 {:style {:-fx-padding "-2 0 0 0"}}
            (:integer :number :string) {:style {:-fx-padding -1}}
            :resource {:style {:-fx-padding [0 2 2 0]}}
            {})
          (assoc :graphic ref))
      (case type
        (:integer :number) {:text (field-expression/format-number x)
                            :alignment :center-right}
        :resource {:text (resource/resource->proj-path x)}
        :choicebox {:text (get (into {} (:options column)) x)}
        :vec4 {:text (->> x (mapv field-expression/format-number) (string/join "  "))}
        {:text (str x)}))))

(defn- table-column [{:keys [path label type] :as column}
                     {:keys [state state-path value on-value-changed]
                      :or {value []}}]
  (let [{:keys [edit]} state]
    {:fx/type fx.table-column/lifecycle
     :reorderable false
     :sortable false
     :min-width (cond
                  (and (= :vec4 type)
                       (= path (:path edit)))
                  235

                  (and (= :resource type)
                       (= path (:path edit)))
                  235

                  :else
                  80)
     :on-edit-start {:event-type :on-table-edit-start
                     :state-path state-path
                     :column-path path}
     :on-edit-cancel {:event-type :on-table-edit-cancel
                      :value value
                      :on-value-changed on-value-changed
                      :state-path state-path}
     :text label
     :cell-value-factory (fxui/partial table-cell-value-factory path)
     :cell-factory (fxui/partial table-cell-factory column (dissoc edit :value))}))

(defmethod form-input-view :table [{:keys [value
                                           on-value-changed
                                           columns
                                           state
                                           state-path]
                                    :or {value []}
                                    :as field}]
  (let [{:keys [selected-indices edit]} state
        disable-remove (empty? selected-indices)
        default-row (form/table-row-defaults field)
        disable-add (nil? default-row)
        on-added {:event-type :on-table-element-added
                  :value value
                  :on-value-changed on-value-changed
                  :state-path state-path
                  :element default-row}
        on-removed {:event-type :remove-table-selection
                    :value value
                    :on-value-changed on-value-changed
                    :state-path state-path}]
    {:fx/type fx/ext-let-refs
     :refs (when edit
             (let [i (:index edit)
                   path (:path edit)
                   edited-column (some #(when (= (:path edit) (:path %))
                                          %)
                                       columns)
                   edited-value (or (:value edit)
                                    (get-in value (into [i] path)))]
               {[::edit i path] (edited-table-cell-view edited-column field edited-value)}))
     :desc {:fx/type fx.v-box/lifecycle
            :spacing 4
            :children [{:fx/type fx.stack-pane/lifecycle
                        :style-class "cljfx-table-view-wrapper"
                        :children
                        [{:fx/type fxui/ext-with-advance-events
                          :desc {:fx/type fx.ext.table-view/with-selection-props
                                 :props {:selection-mode :multiple
                                         :selected-indices (vec selected-indices)
                                         :on-selected-indices-changed {:event-type :table-select
                                                                       :state-path state-path}}
                                 :desc {:fx/type fx.table-view/lifecycle
                                        :style-class ["table-view" "cljfx-table-view"]
                                        :editable true
                                        :fixed-cell-size line-height
                                        :pref-height (+ line-height ;; header
                                                        2   ;; insets
                                                        9   ;; bottom scrollbar
                                                        (* line-height
                                                           (max 1 (count value))))
                                        :columns (mapv #(table-column % field)
                                                       columns)
                                        :items (into [] (map-indexed vector) value)
                                        :context-menu {:fx/type fx.context-menu/lifecycle
                                                       :items [{:fx/type fx.menu-item/lifecycle
                                                                :text "Add"
                                                                :disable disable-add
                                                                :on-action on-added}
                                                               {:fx/type fx.menu-item/lifecycle
                                                                :text "Remove"
                                                                :disable disable-remove
                                                                :on-action on-removed}]}}}}]}
                       {:fx/type fx.h-box/lifecycle
                        :spacing 4
                        :children [{:fx/type icon-button
                                    :disable disable-add
                                    :on-action on-added
                                    :image "icons/32/Icons_M_07_plus.png"
                                    :fit-size 16}
                                   {:fx/type icon-button
                                    :disable disable-remove
                                    :on-action on-removed
                                    :image "icons/32/Icons_M_11_minus.png"
                                    :fit-size 16}]}]}}))

;; endregion

;; region 2panel-input

(defmethod handle-event :2panel-key-added [{:keys [value
                                                   on-value-changed
                                                   on-add
                                                   key-path
                                                   default-row
                                                   fx/event]}]
  (let [new-value (into value
                        (map #(assoc-in default-row key-path %))
                        event)]
    (if on-add
      (do (on-add) nil)
      {:dispatch (assoc on-value-changed :fx/event new-value)})))

(defmethod handle-event :2panel-key-edited [{:keys [value on-value-changed key-path set]
                                             {:keys [index item]} :fx/event}]
  (if set
    (do (set (get value index) key-path item) nil)
    (let [new-value (assoc-in value (into [index] key-path) item)]
      {:dispatch (assoc on-value-changed :fx/event new-value)})))

(defmethod handle-event :2panel-key-removed [{:keys [value
                                                     on-value-changed
                                                     fx/event
                                                     on-remove]}]
  (if on-remove
    (do (on-remove (keep-indices event value)) nil)
    {:dispatch (assoc on-value-changed :fx/event (remove-indices event value))}))

(defmethod handle-event :2panel-value-set [{:keys [value
                                                   index
                                                   value-path
                                                   on-value-changed
                                                   fx/event
                                                   set]}]
  (if set
    (do (set (get value index) value-path event) nil)
    (let [new-value (assoc-in value (into [index] value-path) event)]
      {:dispatch (assoc on-value-changed :fx/event new-value)})))

(defmethod handle-event :2panel-value-clear [{:keys [value
                                                     index
                                                     value-path
                                                     on-value-changed
                                                     set]}]
  (if set
    (do (set (get value index) value-path nil) nil)
    (let [path (into [index] value-path)
          new-value (update-in value (butlast path) dissoc (last path))]
      {:dispatch (assoc on-value-changed :fx/event new-value)})))

(defmethod form-input-view :2panel [{:keys [value
                                            on-value-changed
                                            state
                                            state-path
                                            panel-key
                                            panel-form
                                            resource-string-converter]
                                     :as field}]
  (let [default-row (form/two-panel-defaults field)
        key-path (:path panel-key)
        selected-index (-> state :key :selected-indices peek)
        fn-setter (:set field)]
    {:fx/type fx.v-box/lifecycle
     :spacing 4
     :children
     [(cond-> {:fx/type list-input
               :value (mapv #(get-in % key-path) value)
               :on-added {:event-type :2panel-key-added
                          :value value
                          :on-value-changed on-value-changed
                          :key-path key-path
                          :default-row default-row
                          :on-add (:on-add field)}
               :on-edited {:event-type :2panel-key-edited
                           :value value
                           :on-value-changed on-value-changed
                           :set fn-setter
                           :key-path key-path}
               :on-removed {:event-type :2panel-key-removed
                            :value value
                            :on-value-changed on-value-changed
                            :on-remove (:on-remove field)}
               :state-path (conj state-path :key)
               :element panel-key
               :resource-string-converter resource-string-converter}

              (contains? state :key)
              (assoc :state (:key state)))
      {:fx/type fx.v-box/lifecycle
       :spacing 6
       :children
       (if (some? selected-index)
         (into []
               (comp
                 (mapcat :fields)
                 (map
                   (fn [field]
                     (let [field-path (into [selected-index] (:path field))
                           field-value (get-in value field-path ::no-value)
                           field-state-path (conj state-path :val selected-index (:path field))
                           field-state (get-in state [:val selected-index (:path field)] ::no-value)]
                       {:fx/type fx.v-box/lifecycle
                        :spacing 4
                        :children
                        [{:fx/type fx.h-box/lifecycle
                          :spacing 4
                          :pref-height line-height
                          :children (cond-> [{:fx/type fx.label/lifecycle
                                              :h-box/margin {:top 5}
                                              :text (:label field)}]
                                            (and (form/optional-field? field)
                                                 (not= field-value ::no-value))
                                            (conj {:fx/type icon-button
                                                   :image "icons/32/Icons_S_02_Reset.png"
                                                   :on-action {:event-type :2panel-value-clear
                                                               :index selected-index
                                                               :value-path (:path field)
                                                               :set fn-setter
                                                               :value value
                                                               :on-value-changed on-value-changed}}))}
                         (cond->
                           (assoc field :fx/type form-input-view
                                        :value (if (= ::no-value field-value)
                                                 (form/field-default field)
                                                 field-value)
                                        :on-value-changed {:event-type :2panel-value-set
                                                           :index selected-index
                                                           :value-path (:path field)
                                                           :value value
                                                           :set fn-setter
                                                           :on-value-changed on-value-changed}
                                        :state-path field-state-path
                                        :resource-string-converter resource-string-converter)
                           (not= ::no-value field-state)
                           (assoc :state field-state))]}))))
               (:sections panel-form))
         [])}]}))

;; endregion

(defn- make-row [values ui-state resource-string-converter row field]
  (let [{:keys [path label help visible]} field
        value (get values path ::no-value)
        state-path [:components path]
        state (get-in ui-state state-path ::no-value)
        error (settings/get-setting-error
                (when-not (= value ::no-value) value)
                field
                :build-targets)]
    (cond-> []
            :always
            (conj (cond->
                    {:fx/type fx.label/lifecycle
                     :grid-pane/row row
                     :grid-pane/column 0
                     :grid-pane/valignment :top
                     :grid-pane/margin {:top 5}
                     :visible visible
                     :managed visible
                     :alignment :top-left
                     :text label}
                    (not-empty help)
                    (assoc :tooltip {:fx/type fx.tooltip/lifecycle
                                     :text help})))

            (and (form/optional-field? field)
                 (not= value ::no-value))
            (conj {:fx/type icon-button
                   :grid-pane/row row
                   :grid-pane/column 1
                   :grid-pane/valignment :top
                   :grid-pane/halignment :right
                   :visible visible
                   :managed visible
                   :image "icons/32/Icons_S_02_Reset.png"
                   :on-action {:event-type :clear
                               :path path}})

            :always
            (conj {:fx/type fx.v-box/lifecycle
                   :style-class (case (:severity error)
                                  :fatal ["cljfx-form-error"]
                                  :warning ["cljfx-form-warning"]
                                  [])
                   :grid-pane/row row
                   :grid-pane/column 2
                   :visible visible
                   :managed visible
                   :min-height line-height
                   :alignment :center-left
                   :children [(cond-> field
                                      :always
                                      (assoc :fx/type form-input-view
                                             :resource-string-converter resource-string-converter
                                             :value (if (= ::no-value value)
                                                      (form/field-default field)
                                                      value)
                                             :on-value-changed {:event-type :set
                                                                :path path}
                                             :state-path state-path)

                                      (not= ::no-value state)
                                      (assoc :state state))]}))))

(defn- section-view [{:keys [title help fields values ui-state resource-string-converter visible]}]
  {:fx/type fx.v-box/lifecycle
   :visible visible
   :managed visible
   :children (cond-> []

                     :always
                     (conj {:fx/type fx.label/lifecycle
                            :style-class ["label" "cljfx-form-title"]
                            :text title})

                     help
                     (conj {:fx/type fx.label/lifecycle
                            :text help})

                     :always
                     (conj {:fx/type fx.grid-pane/lifecycle
                            :style-class "cljfx-form-fields"
                            :column-constraints [{:fx/type fx.column-constraints/lifecycle
                                                  :min-width 150
                                                  :max-width 150}
                                                 {:fx/type fx.column-constraints/lifecycle
                                                  :min-width line-height
                                                  :max-width line-height}
                                                 {:fx/type fx.column-constraints/lifecycle
                                                  :hgrow :always
                                                  :min-width 200
                                                  :max-width 400}]
                            :children (first
                                        (reduce
                                          (fn [[acc row] field]
                                            [(into acc (make-row values
                                                                 ui-state
                                                                 resource-string-converter
                                                                 row
                                                                 field))
                                             (if (:visible field) (inc row) row)])
                                          [[] 0]
                                          fields))}))})

;; region filtering

(defmulti filterable-strings :type)

(defmethod filterable-strings :default [{:keys [value]}]
  (when (some? value)
    [(str value)]))

(defmethod filterable-strings :boolean [_]
  [])

(defmethod filterable-strings :list [{:keys [value] :as field}]
  (eduction
    (map #(assoc (:element field) :value %))
    (mapcat filterable-strings)
    value))

(defmethod filterable-strings :resource [{:keys [value]}]
  [(resource/resource->proj-path value)])

(defmethod filterable-strings :choicebox [{:keys [value] :as field}]
  (when (some? value)
    [((:to-string field str) value)]))

(defn- set-field-visibility [field values filter-term section-visible]
  (let [value (get values (:path field) ::no-value)
        visible (and (or section-visible
                         (contains-ignore-case? (:label field) filter-term)
                         (boolean (some #(contains-ignore-case? % filter-term)
                                        (filterable-strings
                                          (assoc field :value (if (= value ::no-value)
                                                                (form/field-default field)
                                                                value))))))
                     (or (not (:deprecated field))
                         (some? (settings/get-setting-error
                                  (if (= value ::no-value) nil value)
                                  field
                                  :build-targets))))]
    (assoc field :visible visible)))

(defn- set-section-visibility [{:keys [title help fields] :as section} values filter-term]
  (let [visible (or (contains-ignore-case? title filter-term)
                    (and (some? help)
                         (contains-ignore-case? help filter-term)))
        fields (into []
                     (comp
                       (remove :hidden?)
                       (map #(set-field-visibility % values filter-term visible)))
                     fields)]
    (assoc section :visible (or visible (boolean (some :visible fields)))
                   :fields fields)))

(defmethod handle-event :filter-text-changed [{:keys [ui-state fx/event]}]
  {:set-ui-state (assoc ui-state :filter-term event)})

(defmethod handle-event :filter-key-pressed [{:keys [^KeyEvent fx/event ui-state]}]
  (when (= KeyCode/ESCAPE (.getCode event))
    {:set-ui-state (assoc ui-state :filter-term "")}))

(defmethod handle-event :navigate-sections [{:keys [fx/event ui-state sections selected-section-title]}]
  (when (instance? KeyEvent event)
    (let [^KeyEvent event event]
      (when (= KeyEvent/KEY_PRESSED (.getEventType event))
        (condp = (.getCode event)
          KeyCode/DOWN
          (do (.consume event)
              (when-let [next-section (->> sections
                                           (drop-while #(not= % selected-section-title))
                                           second)]
                {:set-ui-state (assoc ui-state :selected-section-title next-section)}))

          KeyCode/UP
          (do (.consume event)
              (when-let [prev-section (->> sections
                                           (take-while #(not= % selected-section-title))
                                           last)]
                {:set-ui-state (assoc ui-state :selected-section-title prev-section)}))

          nil)))))

(defn- filter-text-field [{:keys [text sections selected-section-title]}]
  (wrap-focus-text-field
    {:fx/type fx.text-field/lifecycle
     :id "filter-text-field"
     :min-height :use-pref-size
     :pref-height 10
     :max-height :use-pref-size
     :style-class ["text-field" "filter-text-field"]
     :prompt-text "Filter"
     :text text
     :event-filter {:event-type :navigate-sections
                    :sections sections
                    :selected-section-title selected-section-title}
     :on-key-pressed {:event-type :filter-key-pressed}
     :on-text-changed {:event-type :filter-text-changed}}))

;; endregion

(defn- make-section-views [sections values ui-state resource-string-converter]
  (first
    (reduce
      (fn
        ([[acc seen-visible] section]
         (let [section-view (assoc section
                              :fx/type section-view
                              :values values
                              :ui-state ui-state
                              :resource-string-converter resource-string-converter)
               visible (:visible section)]
           (if (empty? acc)
             [(conj acc section-view) visible]
             [(into acc [{:fx/type fx.separator/lifecycle
                          :style-class "cljfx-form-separator"
                          :visible (and seen-visible visible)
                          :managed (and seen-visible visible)}
                         section-view])
              (or seen-visible visible)]))))
      [[] false]
      sections)))

;; region jump-to

(defmethod handle-event :jump-to [{:keys [section ui-state]}]
  {:set-ui-state (assoc ui-state :selected-section-title section)})

(defn- form-sections [{:keys [groups sections selected-section-title]}]
  {:fx/type fx/ext-let-refs
   :refs (into {}
               (comp
                 (mapcat val)
                 (map (juxt identity
                            (fn [title]
                              {:fx/type fx.label/lifecycle
                               :pseudo-classes (if (= title selected-section-title) #{:active} #{})
                               :max-width ##Inf
                               :style-class "cljfx-form-section-button"
                               :on-mouse-clicked {:event-type :jump-to :section title}
                               :text title}))))
               groups)
   :desc {:fx/type fxui/ext-ensure-scroll-pane-child-visible
          :child-desc {:fx/type fx/ext-get-ref :ref selected-section-title}
          :scroll-pane-desc {:fx/type fx.scroll-pane/lifecycle
                             :event-filter {:event-type :navigate-sections
                                            :sections sections
                                            :selected-section-title selected-section-title}
                             :style-class "cljfx-form-contents-scroll-pane"
                             :hbar-policy :never
                             :fit-to-width true
                             :fit-to-height true
                             :content {:fx/type fx.v-box/lifecycle
                                       :fill-width true
                                       :spacing 16
                                       :children (mapv (fn [[group titles]]
                                                         {:fx/type fx.v-box/lifecycle
                                                          :fill-width true
                                                          :children (into [{:fx/type fx.label/lifecycle
                                                                            :style-class "cljfx-form-section-header"
                                                                            :text group}]
                                                                          (map
                                                                            (fn [title]
                                                                              {:fx/type fx/ext-get-ref :ref title}))
                                                                          titles)})
                                                       groups)}}}})

;; endregion

(defn- form-view [{:keys [parent form-data ui-state resource-string-converter]}]
  (let [{:keys [sections values group-order default-section-name]} form-data
        filter-term (:filter-term ui-state)
        sections-with-visibility (mapv #(set-section-visibility % values filter-term) sections)
        navigation (:navigation form-data true)]
    {:fx/type with-anchor-pane-props
     :desc {:fx/type fxui/ext-value
            :value parent}
     :props {:children [(if navigation
                          (let [visible-sections (filterv :visible sections-with-visibility)
                                groups (->> visible-sections
                                            (util/group-into
                                              {}
                                              (sorted-set-by #(.compareToIgnoreCase ^String %1 %2))
                                              :group
                                              :title)
                                            (sort-by #(group-order (key %) ##Inf)))
                                visible-titles (into #{} (map :title) visible-sections)
                                selected-section-title (or (-> ui-state :selected-section-title (or default-section-name) visible-titles)
                                                           (some-> groups first val first visible-titles))]
                            {:fx/type :h-box
                             :anchor-pane/top 0
                             :anchor-pane/right 0
                             :anchor-pane/bottom 0
                             :anchor-pane/left 0
                             :children [{:fx/type fx.v-box/lifecycle
                                         :min-width 160
                                         :pref-width 160
                                         :style-class "cljfx-form-contents"
                                         :view-order 0
                                         :children [{:fx/type filter-text-field
                                                     :v-box/margin 12
                                                     :text filter-term
                                                     :sections (mapcat val groups)
                                                     :selected-section-title selected-section-title}
                                                    {:fx/type form-sections
                                                     :sections (mapcat val groups)
                                                     :selected-section-title selected-section-title
                                                     :groups groups}]}
                                        {:fx/type fx/ext-let-refs
                                         :h-box/hgrow :always
                                         :refs (into {}
                                                     (map (juxt :title
                                                                #(assoc % :fx/type section-view
                                                                          :values values
                                                                          :ui-state ui-state
                                                                          :resource-string-converter resource-string-converter)))
                                                     sections-with-visibility)
                                         :desc {:fx/type fx/ext-on-instance-lifecycle
                                                :on-created #(ui/context! % :form {:root parent} nil)
                                                :desc {:fx/type fx.scroll-pane/lifecycle
                                                       :id "scroll-pane"
                                                       :view-order 1
                                                       :fit-to-width true
                                                       :content {:fx/type fx.v-box/lifecycle
                                                                 :style-class "cljfx-form"
                                                                 :children (if selected-section-title
                                                                             [{:fx/type fx/ext-get-ref
                                                                               :ref selected-section-title}]
                                                                             [])}}}}]})

                          {:fx/type fx/ext-on-instance-lifecycle
                           :anchor-pane/top 0
                           :anchor-pane/right 0
                           :anchor-pane/bottom 0
                           :anchor-pane/left 0
                           :on-created #(ui/context! % :form {:root parent} nil)
                           :desc {:fx/type fx.scroll-pane/lifecycle
                                  :id "scroll-pane"
                                  :view-order 1
                                  :fit-to-width true
                                  :content {:fx/type fx.v-box/lifecycle
                                            :style-class "cljfx-form"
                                            :children (make-section-views
                                                        sections-with-visibility
                                                        values
                                                        ui-state
                                                        resource-string-converter)}}})]}}))

(defn- wrap-force-refresh [f view-id]
  (fn [event]
    (f event)
    (g/node-value view-id :form-view)))

(defn- create-renderer [view-id parent workspace project]
  (let [resource-string-converter (make-resource-string-converter workspace)]
    (fx/create-renderer
      :error-handler error-reporting/report-exception!
      :opts {:fx.opt/map-event-handler
             (-> handle-event
                 (fx/wrap-co-effects
                   {:ui-state #(g/node-value view-id :ui-state)
                    :form-data #(g/node-value view-id :form-data)
                    :workspace (constantly workspace)
                    :project (constantly project)
                    :parent (constantly parent)})
                 (fx/wrap-effects
                   {:dispatch fx/dispatch-effect
                    :set (fn [[path value] _]
                           (let [ops (:form-ops (g/node-value view-id :form-data))]
                             (form/set-value! ops path value)))
                    :clear (fn [path _]
                             (let [ops (:form-ops (g/node-value view-id :form-data))]
                               (when (form/can-clear? ops)
                                 (form/clear-value! ops path))))
                    :set-ui-state (fn [ui-state _]
                                    (g/set-property! view-id :ui-state ui-state))
                    :cancel-edit (fn [x _]
                                   (cond
                                     (instance? Cell x)
                                     (fx/run-later (.cancelEdit ^Cell x))

                                     (instance? ListView x)
                                     (.edit ^ListView x -1)))
                    :open-resource (fn [[node value] _]
                                     (ui/run-command node :open {:resources [value]}))})
                 (wrap-force-refresh view-id))}

      :middleware (comp
                    fxui/wrap-dedupe-desc
                    (fx/wrap-map-desc
                      (fn [{:keys [form-data ui-state]}]
                        {:fx/type form-view
                         :form-data form-data
                         :ui-state ui-state
                         :parent parent
                         :resource-string-converter resource-string-converter}))))))

(defn- make-form-view [graph parent resource-node opts]
  (let [{:keys [workspace project tab]} opts
        view-id (-> (g/make-nodes graph [view [CljfxFormView]]
                      (g/set-property view :renderer (create-renderer view
                                                                      parent
                                                                      workspace
                                                                      project))
                      (g/connect resource-node :form-data view :form-data))
                    g/transact
                    g/tx-nodes-added
                    first)
        repaint-timer (ui/->timer 30 "refresh-form-view"
                                  (fn [_timer _elapsed]
                                    (g/node-value view-id :form-view)))]
    (g/node-value view-id :form-view)
    (ui/timer-start! repaint-timer)
    (ui/timer-stop-on-closed! tab repaint-timer)
    view-id))

(defn register-view-types [workspace]
  (workspace/register-view-type workspace
                                :id :cljfx-form-view
                                :label "Form"
                                :make-view-fn make-form-view))

(handler/defhandler :filter-form :form
  (run [^Node root]
       (when-let [node (.lookup root "#filter-text-field")]
         (.requestFocus node))))
