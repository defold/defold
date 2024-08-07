;; Copyright 2020-2024 The Defold Foundation
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

(ns editor.editor-extensions.components
  (:require [cljfx.api :as fx]
            [cljfx.fx.button :as fx.button]
            [cljfx.fx.check-box :as fx.check-box]
            [cljfx.fx.column-constraints :as fx.column-constraints]
            [cljfx.fx.combo-box :as fx.combo-box]
            [cljfx.fx.grid-pane :as fx.grid-pane]
            [cljfx.fx.h-box :as fx.h-box]
            [cljfx.fx.image-view :as fx.image-view]
            [cljfx.fx.label :as fx.label]
            [cljfx.fx.region :as fx.region]
            [cljfx.fx.row-constraints :as fx.row-constraints]
            [cljfx.fx.scroll-pane :as fx.scroll-pane]
            [cljfx.fx.stage :as fx.stage]
            [cljfx.fx.text-field :as fx.text-field]
            [cljfx.fx.v-box :as fx.v-box]
            [cljfx.lifecycle :as fx.lifecycle]
            [cljfx.mutator :as fx.mutator]
            [cljfx.prop :as fx.prop]
            [clojure.spec.alpha :as s]
            [clojure.string :as string]
            [editor.dialogs :as dialogs]
            [editor.editor-extensions.coerce :as coerce]
            [editor.editor-extensions.runtime :as rt]
            [editor.future :as future]
            [editor.fxui :as fxui]
            [editor.icons :as icons]
            [editor.util :as util]
            [internal.util :as iutil]
            [util.coll :as coll])
  (:import [com.defold.control DefoldStringConverter]
           [com.sun.javafx.scene.control.skin Utils]
           [javafx.beans Observable]
           [javafx.beans.binding Bindings]
           [javafx.scene.control ComboBox ScrollPane TextField]
           [javafx.scene.control.skin ScrollPaneSkin]
           [javafx.scene.input KeyCode KeyEvent]
           [javafx.scene.layout Region]))

;; See also:
;; - Components doc: https://docs.google.com/document/d/1e6kmVLspQEoe17Ys1nmbbf54cqUn2fNPZowIe7JBwl4/edit
;; - Reactive UI doc: https://docs.google.com/document/d/1cgqFPLUc3YQih2tsEA067Pz9Q0nWlKoVflqLEVX8CPY/edit

;; Components are created with a table of props. The editor defines prop maps
;; both for runtime behavior (coercion) and autocomplete generation.

;; TODO components:
;;  - value_field
;;  - value_field's friends:
;;    - string_field
;;    - number_field
;;    - integer_field
;;  - external_file_field
;;  - resource_field

(s/def :editor.editor-extensions.components.prop/name simple-keyword?)
(s/def ::coerce ifn?)
(s/def ::required boolean?)
(s/def ::types (s/coll-of string? :min-count 1 :distinct true))
(s/def ::doc string?)
(s/def ::prop
  (s/keys :req-un [:editor.editor-extensions.components.prop/name ::coerce ::required ::types ::doc]))

(defn- make-prop
  "Construct a prop definition map

  Args:
    name    keyword name of a prop, should be snake_cased

  Kv-args:
    :coerce      required, coercer function
    :required    optional, boolean, whether the prop is required to be present
    :types       required if it can't be inferred from coercer, coll of string
                 type names (union) for documentation
    :doc         required, html documentation string"
  [name & {:keys [coerce required types doc]
           :or {required false}}]
  {:post [(s/assert ::prop %)]}
  {:name name
   :coerce coerce
   :required required
   :types (or types
              (condp = coerce
                coerce/boolean ["boolean"]
                coerce/string ["string"]
                coerce/untouched ["any"]
                coerce/function ["function"]
                nil))
   :doc doc})

(s/def :editor.editor-extensions.components.component/name string?)
(s/def ::props (s/coll-of ::prop))
(s/def ::fn ifn?)
(s/def ::description string?)
(s/def ::component
  (s/keys :req-un [:editor.editor-extensions.components.component/name ::props ::fn ::description]))

(defn- make-component
  "Construct a component definition map

  Args:
    name    string name of the component, should be snake_cased

  Kv-args:
    :props          required, a list of props of the component
    :fn             required, fn that converts a prop map to a cljfx component
    :description    required, markdown documentation string"
  [name & {:keys [props description fn]}]
  {:post [(s/assert ::component %)]}
  {:name name :props props :fn fn :description description})

;; region enums

;; Enums are defined separately so that the editor can define a runtime constant
;; and documentation for each enum constant.

(def enums
  {:alignment [:top-left :top :top-right :left :center :right :bottom-left :bottom :bottom-right]
   :padding [:small :medium :large]
   :spacing [:small :medium :large]
   :text_alignment [:left :center :right :justify]
   :text_variant [:default :hint :warning :error]
   :icon_name [:open-resource :plus :minus :clear]
   :orientation [:vertical :horizontal]
   :input_variant [:default :warning :error]})

(def ^:private get-enum-coercer
  (let [m (coll/pair-map-by key #(apply coerce/enum (val %)) enums)]
    (fn get-enum-coercer [kw]
      {:post [(some? %)]}
      (m kw))))

(defn- enum-doc-options [enum]
  {:post [(contains? enums enum)]}
  (map #(format "<code>\"%s\"</code>" (name %)) (enums enum)))

(defn doc-with-ul-options [doc options]
  (str doc
       "; either:\n<ul>"
       (string/join (map #(format "<li>%s</li>" %) options))
       "</ul>"))

(defn- enum-prop [name & {:keys [enum doc] :as props}]
  (let [enum (or enum name)]
    (apply make-prop name (mapcat identity (cond-> (assoc props :coerce (get-enum-coercer enum)
                                                                :types ["string"])
                                                   doc
                                                   (update :doc doc-with-ul-options (enum-doc-options enum)))))))

(defn- ->screaming-snake-case [kw]
  (string/replace (util/upper-case* (name kw)) "-" "_"))

(defn enum-const-name [enum-id enum-value]
  (str (->screaming-snake-case enum-id) "_" (->screaming-snake-case enum-value)))

;; endregion

;; Component and prop definitions

(def ^:private read-only-common-props
  (let [grid-cell-span-coercer (coerce/wrap-with-pred coerce/integer pos? "is not positive")]
    [(make-prop :expand
                :coerce coerce/boolean
                :doc "determines if the component should expand to fill available space in a <code>horizontal</code> or <code>vertical</code> layout container")
     (make-prop :row_span
                :coerce grid-cell-span-coercer
                :types ["integer"]
                :doc "how many rows the component spans inside a grid container, must be positive. This prop is only useful for components inside a <code>grid</code> container.")
     (make-prop :column_span
                :coerce grid-cell-span-coercer
                :types ["integer"]
                :doc "how many columns the component spans inside a grid container, must be positive. This prop is only useful for components inside a <code>grid</code> container.")]))

(def ^:private common-props
  (into [(enum-prop :alignment :doc "alignment of the component content within its assigned bounds, defaults to <code>\"top-left\"</code>")]
        read-only-common-props))

(def ^:private component-coercer (coerce/wrap-with-pred coerce/userdata #(= :component (:type (meta %))) "is not a UI component"))
(def ^:private absent-coercer (coerce/wrap-with-pred coerce/to-boolean false? "is present (not nil or false)"))
(def ^:private child-coercer (coerce/one-of component-coercer absent-coercer))
(def ^:private children-coercer (coerce/vector-of child-coercer))

;; region layout components

;; General approach to layout can be described as follows:
;; - components typically have `alignment` property that configures how the
;;   component content is aligned with its assigned bounds (default: top-left)
;; - leaf components take as much space as available to them
;; - layout components have different approach to giving space to their children:
;;   - `horizontal` gives as much space as possible on a vertical axis, but not
;;     on a horizontal axis. Child components may use `expand` prop to expand on
;;     the horizontal axis.
;;   - Same applies to `vertical`, but with the axes switched
;;   - `grid` does not expand its children, unless `expand` row or column
;;     setting is used.

(def ^:private multi-child-layout-container-props
  (let [non-negative-number (coerce/wrap-with-pred coerce/number (complement neg?) "is negative")
        spacing (coerce/one-of (get-enum-coercer :spacing) non-negative-number)
        padding (coerce/one-of (get-enum-coercer :padding) non-negative-number)]
    (into [(make-prop :padding
                      :coerce padding
                      :types ["string" "number"]
                      :doc (doc-with-ul-options
                             "empty space from the edges of the container to its children"
                             (concat
                               (enum-doc-options :padding)
                               ["non-negative number, pixels"])))
           (make-prop :spacing
                      :coerce spacing
                      :types ["string" "number"]
                      :doc (doc-with-ul-options
                             "empty space between child components"
                             (concat
                               (enum-doc-options :spacing)
                               ["non-negative number, pixels"])))]
          common-props)))

(def ^:private list-props
  (into [(make-prop :children
                    :coerce children-coercer
                    :types ["component[]"]
                    :doc "array of child components")]
        multi-child-layout-container-props))

(def ^:private ^{:arglists '([props padding])} apply-spacing
  (let [spacing->style-class (coll/pair-map-by identity #(str "ext-spacing-" (name %)) (:spacing enums))]
    (fn apply-spacing [props spacing]
      {:pre [(some? spacing)]}
      (if-let [style-class (spacing->style-class spacing)]
        (fxui/add-style-classes props style-class)
        (if (number? spacing)
          (assoc props :spacing spacing)
          (throw (AssertionError. (str "Invalid spacing: " spacing))))))))

(def ^:private ^{:arglists '([props padding])} apply-padding
  (let [padding->style-class (coll/pair-map-by identity #(str "ext-padding-" (name %)) (:padding enums))]
    (fn apply-padding [props padding]
      {:pre [(some? padding)]}
      (if-let [style-class (padding->style-class padding)]
        (fxui/add-style-classes props style-class)
        (if (number? padding)
          (assoc props :padding padding)
          (throw (AssertionError. (str "Invalid padding: " padding))))))))

(defn- apply-alignment [props alignment]
  {:pre [(some? alignment)]}
  (case alignment
    (:top-left :top-right :center :bottom-left :bottom-right) (assoc props :alignment alignment)
    :top (assoc props :alignment :top-center)
    :left (assoc props :alignment :center-left)
    :right (assoc props :alignment :center-right)
    :bottom (assoc props :alignment :bottom-center)))

(def ^:private ext-with-expanded-scroll-pane-content-props
  (fx/make-ext-with-props
    {:content (fx.prop/make
                (fx.mutator/adder-remover
                  (fn add-scroll-pane-content [^ScrollPane scroll-pane ^Region content]
                    (let [^Region scroll-bar (.lookup scroll-pane ".ext-scroll-pane>.scroll-bar:horizontal")]
                      (.setContent scroll-pane content)
                      (.bind (.minHeightProperty content)
                             (Bindings/createDoubleBinding
                               (fn []
                                 (cond-> (.getHeight scroll-pane)
                                         (.isVisible scroll-bar)
                                         (- (.getHeight scroll-bar))))
                               (into-array
                                 Observable
                                 [(.heightProperty scroll-pane)
                                  (.visibleProperty scroll-bar)
                                  (.heightProperty scroll-bar)])))))
                  (fn remove-scroll-pane-content [_ ^Region content]
                    (.unbind (.minHeightProperty content))))
                fx.lifecycle/dynamic)}))

(def ^:private ext-with-scroll-pane-skin-props
  (fx/make-ext-with-props
    {:skin (fx.prop/make
             (fx.mutator/adder-remover
               (fn add-skin [^ScrollPane instance flag]
                 {:pre [(true? flag)]}
                 (.setSkin instance (ScrollPaneSkin. instance)))
               (fn remove-skin [_ _]
                 (throw (AssertionError. "Can't remove skin!"))))
             fx.lifecycle/scalar)}))

(defn- scroll-view [{:keys [child]}]
  ;; We need to set ScrollPane skin, so it creates ScrollBars, so we can expand
  ;; the content to fill the ScrollPane
  {:fx/type ext-with-expanded-scroll-pane-content-props
   :props {:content child}
   :desc {:fx/type ext-with-scroll-pane-skin-props
          :props {:skin true}
          :desc {:fx/type fx.scroll-pane/lifecycle
                 :style-class "ext-scroll-pane"
                 :fit-to-width true}}})

(def ^:private grid-props
  (let [grid-constraints-coercer (coerce/vector-of
                                   (coerce/one-of
                                     (coerce/hash-map :opt {:expand coerce/boolean})
                                     absent-coercer))
        constraint-doc #(format "array of %s option tables, separate configuration for each %s:<dl><dt><code>expand <small>boolean</small></code></dt><dd>determines if the %s should expand to fill available space</dd></dl>"
                                % % %)]
    (into [(make-prop :children
                      :coerce (coerce/vector-of (coerce/one-of children-coercer absent-coercer))
                      :types ["component[][]"]
                      :doc "array of arrays of child components")
           (make-prop :rows
                      :coerce grid-constraints-coercer
                      :types ["table[]"]
                      :doc (constraint-doc "row"))
           (make-prop :columns
                      :coerce grid-constraints-coercer
                      :types ["table[]"]
                      :doc (constraint-doc "column"))]
          multi-child-layout-container-props)))

(defn- apply-constraints [props lifecycle expand-key constraints]
  (assoc props :column-constraints (mapv (fn [maybe-column]
                                           (let [ret {:fx/type lifecycle}]
                                             (if maybe-column
                                               (let [{:keys [expand]} maybe-column]
                                                 (cond-> ret expand (assoc expand-key :always)))
                                               ret)))
                                         constraints)))

(def ^:private ^{:arglists '([props padding])} apply-grid-spacing
  (let [spacing->style-class (coll/pair-map-by identity #(str "ext-grid-spacing-" (name %)) (:spacing enums))]
    (fn apply-grid-spacing [props spacing]
      {:pre [(some? spacing)]}
      (if-let [style-class (spacing->style-class spacing)]
        (fxui/add-style-classes props style-class)
        (if (number? spacing)
          (assoc props :hgap spacing :vgap spacing)
          (throw (AssertionError. (str "Invalid spacing: " spacing))))))))

(defn- grid-view [{:keys [children rows columns alignment padding spacing]}]
  (cond-> {:fx/type fx.grid-pane/lifecycle}
          children (assoc :children
                          (into []
                                (comp
                                  (map-indexed
                                    (fn [row row-children]
                                      (eduction
                                        (keep-indexed
                                          (fn [column child]
                                            (when child
                                              (let [{:keys [row_span column_span]
                                                     :or {row_span 1 column_span 1}} (:props (meta child))]
                                                (assoc child :grid-pane/row row
                                                             :grid-pane/column column
                                                             :grid-pane/row-span row_span
                                                             :grid-pane/column-span column_span)))))
                                        row-children)))
                                  cat)
                                children))
          rows (apply-constraints fx.row-constraints/lifecycle :vgrow rows)
          columns (apply-constraints fx.column-constraints/lifecycle :hgrow columns)
          spacing (apply-grid-spacing spacing)
          padding (apply-padding padding)
          alignment (apply-alignment alignment)))

(defn- separator-view [{:keys [alignment orientation]
                        :or {alignment :center
                             orientation :horizontal}}]
  (apply-alignment
    {:fx/type fx.v-box/lifecycle
     :children [{:fx/type fx.region/lifecycle
                 :v-box/vgrow :always
                 :style-class ["ext-separator"]
                 :pseudo-classes #{orientation}}]}
    alignment))

(def ^:private required-child-prop
  (make-prop :child
             :coerce component-coercer
             :required true
             :types ["component"]
             :doc "child component"))

(def ^:private layout-components
  (letfn [(make-list-view-fn [fx-type expand-key]
            (fn list-view [{:keys [children padding spacing alignment]}]
              (cond-> {:fx/type fx-type
                       :children (into []
                                       (keep-indexed
                                         (fn [i desc]
                                           (when desc
                                             (-> desc
                                                 (assoc :fx/key i)
                                                 (cond-> (:expand (:props (meta desc)))
                                                         (assoc expand-key :always))))))
                                       children)}
                      spacing (apply-spacing spacing)
                      padding (apply-padding padding)
                      alignment (apply-alignment alignment))))]
    [(make-component
       "horizontal"
       :description "Layout container that places its children in a horizontal row one after another"
       :props list-props
       :fn (make-list-view-fn fx.h-box/lifecycle :h-box/hgrow))
     (make-component
       "vertical"
       :description "Layout container that places its children in a vertical column one after another"
       :props list-props
       :fn (make-list-view-fn fx.v-box/lifecycle :v-box/vgrow))
     (make-component
       "grid"
       :description "Layout container that places its children in a 2D grid"
       :props grid-props
       :fn grid-view)
     (make-component
       "separator"
       :description "Thin line for visual content separation, by default horizontal and aligned to center"
       :props (into [(enum-prop :orientation :doc "separator line orientation, vertical or horizontal")]
                    common-props)
       :fn separator-view)
     (make-component
       "scroll"
       :description "Layout container that optionally shows scroll bars if child contents overflow the assigned bounds"
       :props (into [required-child-prop] read-only-common-props)
       :fn scroll-view)]))

;; endregion

;; region data presentation components

(def ^:private ^{:arglists '([props text-variant])} apply-text-variant
  (let [m (coll/pair-map-by identity #(str "ext-text-variant-" (name %)) (:text_variant enums))]
    (fn apply-text-variant [props text-variant]
      {:pre [(some? text-variant)]}
      (if-let [style-class (m text-variant)]
        (fxui/add-style-classes props style-class)
        (throw (AssertionError. (str "Invalid text variant: " text-variant)))))))

(def ^:private label-without-variant-specific-props
  [(make-prop :text :coerce coerce/string :doc "the text")
   (enum-prop :text_alignment :doc "text alignment within paragraph bounds")])

(def ^:private label-specific-props
  (conj label-without-variant-specific-props
        (enum-prop :variant :enum :text_variant :doc "semantic view variant")))

(defn- label-view [{:keys [alignment text text_alignment variant]
                    :or {alignment :top-left}}]
  (-> {:fx/type fx.label/lifecycle
       :style-class ["label"]
       :min-width :use-pref-size
       :min-height :use-pref-size
       :max-width Double/MAX_VALUE
       :max-height Double/MAX_VALUE}
      (apply-alignment alignment)
      (cond-> text (assoc :text text)
              text_alignment (assoc :text-alignment text_alignment)
              variant (apply-text-variant variant))))

(defn- text-view [{:keys [alignment text text_alignment variant word_wrap]
                   :or {alignment :top-left
                        word_wrap true}}]
  (-> {:fx/type fx.label/lifecycle
       :style-class ["label"]
       :max-width Double/MAX_VALUE
       :max-height Double/MAX_VALUE
       :wrap-text word_wrap}
      (apply-alignment alignment)
      (cond-> text (assoc :text text)
              text_alignment (assoc :text-alignment text_alignment)
              variant (apply-text-variant variant))))

(def ^:private icon-specific-props
  [(enum-prop :name :enum :icon_name :required true :doc "predefined icon name")])

(defn- wrap-in-alignment-container
  "Wrapper for components that don't specify alignment

  Will keep the child as small as possible by default"
  ([desc maybe-alignment]
   (wrap-in-alignment-container desc maybe-alignment false))
  ([desc maybe-alignment fill-horizontal]
   (cond-> {:fx/type fx.v-box/lifecycle
            :fill-width fill-horizontal
            :children [desc]}
           maybe-alignment
           (apply-alignment maybe-alignment))))

(defn- icon-view [{:keys [name alignment]}]
  (let [fit-size (case name
                   :open-resource 22.0
                   16.0)]
    (wrap-in-alignment-container
      {:fx/type fx.v-box/lifecycle
       :style-class "ext-icon-container"
       :alignment :center
       :children [{:fx/type fx.image-view/lifecycle
                   :image (icons/get-image
                            (case name
                              :open-resource "icons/32/Icons_S_14_linkarrow.png"
                              :plus "icons/32/Icons_M_07_plus.png"
                              :minus "icons/32/Icons_M_11_minus.png"
                              :clear "icons/32/Icons_S_02_Reset.png"))
                   :fit-width fit-size
                   :fit-height fit-size}]}
      alignment)))

(def ^:private data-presentation-components
  (let [text-specific-props (conj label-specific-props
                                  (make-prop :word_wrap
                                             :coerce coerce/boolean
                                             :doc "determines if the lines of text are word-wrapped when they don't fit in the assigned bounds, defaults to true"))]
    [(make-component
       "label"
       :description "Non-resizeable text label"
       :props (into label-specific-props common-props)
       :fn label-view)
     (make-component
       "text"
       :description "Resizeable text label"
       :props (into text-specific-props common-props)
       :fn text-view)
     (make-component
       "icon"
       :description "An icon of a fixed size"
       :props (into icon-specific-props common-props)
       :fn icon-view)]))

;; endregion

;; region input components

(def ^:private common-input-props
  (into [(make-prop :disabled
                    :coerce coerce/boolean
                    :doc "determines if the input component can be interacted with")]
        common-props))

(defn- report-async-error-to-script [future rt label]
  (future/catch
    future
    (fn report-exception [ex]
      (.println (rt/stderr rt)
                (str label " failed: " (or (ex-message ex) (.getSimpleName (class ex))))))))

(defn- make-event-handler-0 [rt label lua-fn]
  (fn event-handler-0 [_]
    (report-async-error-to-script (rt/invoke-suspending rt lua-fn) rt label)))

(defn- make-event-handler-1 [rt label lua-fn]
  (fn event-handler-1 [value]
    (report-async-error-to-script (rt/invoke-suspending rt lua-fn (rt/->lua value)) rt label)))

;; region button

(def ^:private button-specific-props
  [(make-prop :on_pressed
              :coerce coerce/function
              :doc "button press callback")])

(defn- button-view-impl [{:keys [rt on_pressed disabled alignment style-class child]
                          :or {disabled false}}]
  {:pre [(string? style-class)]}
  (-> {:fx/type fx.button/lifecycle
       :disable disabled
       :focus-traversable false
       :style-class ["ext-button" style-class]
       :graphic child}
      (cond-> on_pressed
              (assoc :on-action (make-event-handler-0 rt "on_pressed" on_pressed)))
      (wrap-in-alignment-container alignment)))

(defn- button-view [props]
  {:fx/type fx/ext-get-env :env [:rt] :desc (assoc props :fx/type button-view-impl)})

(defn- icon-button-view [props]
  (button-view (assoc props :style-class "ext-button-icon" :child (icon-view (dissoc props :alignment)))))

(defn- text-button-view [props]
  (button-view (assoc props :style-class "ext-button-text" :child (label-view (dissoc props :alignment)))))

(defn- default-button-view [props]
  (button-view (assoc props :style-class "ext-button-default")))

;; endregion

(def ^:private input-with-variant-props
  (into [(enum-prop :variant :enum :input_variant :doc "input variant")]
        common-input-props))

;; region check_box

(def ^:private check-box-specific-props
  [(make-prop :value :coerce coerce/boolean :doc "checked value")
   (make-prop :on_value_changed :coerce coerce/function :doc "change callback, will receive the new value")])

(defn- check-box-view-impl [{:keys [rt alignment variant disabled value on_value_changed child]
                             :or {disabled false
                                  value false
                                  variant :default}}]
  (-> {:fx/type fx.check-box/lifecycle
       :style-class ["check-box" "ext-check-box"]
       :alignment :top-left
       :pseudo-classes #{variant}
       :disable disabled
       :selected value}
      (cond-> child (assoc :graphic {:fx/type fx.v-box/lifecycle
                                     :style-class "ext-check-box-child"
                                     :children [child]})
              on_value_changed (assoc :on-selected-changed (make-event-handler-1 rt "on_value_changed" on_value_changed)))
      (wrap-in-alignment-container alignment)))

(defn- check-box-view [props]
  {:fx/type fx/ext-get-env :env [:rt] :desc (assoc props :fx/type check-box-view-impl)})

(defn- text-check-box-view [props]
  (check-box-view (assoc props :child (label-view (dissoc props :alignment :variant)))))

;; endregion

;; region select_box

(def ^:private select-box-specific-props
  [(make-prop :value :coerce coerce/untouched :doc "selected value")
   (make-prop :on_value_changed :coerce coerce/function :doc "change callback, will receive the selected value")
   (make-prop :options :coerce (coerce/vector-of coerce/untouched) :doc "array of selectable options" :types ["any[]"])
   (make-prop :to_string :coerce coerce/function :doc "function that converts an item to string, defaults to <code>tostring</code>")])

(defn- create-select-box-string-converter [rt to_string]
  ;; if a combo box has no value provided, the default is JVM null
  (if to_string
    (DefoldStringConverter.
      (fn user-provided-to-string [maybe-lua-value]
        (if (nil? maybe-lua-value)
          ""
          (rt/->clj rt coerce/to-string (rt/invoke-immediate rt to_string maybe-lua-value)))))
    (DefoldStringConverter.
      (fn default-to-string [maybe-lua-value]
        (if (nil? maybe-lua-value)
          ""
          (rt/->clj rt coerce/to-string maybe-lua-value))))))

(defn- on-select-box-key-pressed [^KeyEvent event]
  (when (= KeyCode/SPACE (.getCode event))
    (.show ^ComboBox (.getSource event))
    (.consume event)))

(defn- select-box-view-impl [{:keys [rt alignment variant disabled value options on_value_changed to_string]
                              :or {options []
                                   disabled false
                                   variant :default}}]
  (wrap-in-alignment-container
    {:fx/type fxui/ext-memo
     :fn create-select-box-string-converter
     :args [rt to_string]
     :key :converter
     :desc (cond-> {:fx/type fx.combo-box/lifecycle
                    :value value
                    :pseudo-classes #{variant}
                    :on-key-pressed on-select-box-key-pressed
                    :items options
                    :disable disabled}
                   on_value_changed (assoc :on-value-changed (make-event-handler-1 rt "on_value_changed" on_value_changed)))}
    alignment
    true))

(defn- select-box-view [props]
  {:fx/type fx/ext-get-env :env [:rt] :desc (assoc props :fx/type select-box-view-impl)})

;; endregion

;; region text field

(def ^:private text-field-specific-props
  [(make-prop :text :coerce coerce/string :doc "text")
   (make-prop :on_text_changed :coerce coerce/function :doc "text change callback, will receive the new text")])

(def ^:private ext-with-text-field-pref-width-bound-props
  (fx/make-ext-with-props
    {:bind (fx.prop/make
             (fx.mutator/adder-remover
               (fn bind-text-field-pref-width [^TextField text-field min-width]
                 (.bind
                   (.prefWidthProperty text-field)
                   (Bindings/createDoubleBinding
                     (fn []
                       (let [padding (.getPadding text-field)]
                         (max
                           min-width
                           (Math/ceil
                             (+ 1.0                          ;; for cursor
                                (.getLeft padding)
                                (Utils/computeTextWidth (.getFont text-field) (.getText text-field) -1.0)
                                (.getRight padding))))))
                     (into-array
                       Observable
                       [(.fontProperty text-field)
                        (.textProperty text-field)
                        (.paddingProperty text-field)]))))
               (fn unbind-text-field-pref-height [^TextField text-field _]
                 (.unbind (.prefWidthProperty text-field))))
             fx.lifecycle/scalar)}))

(defn- text-field-view-impl [{:keys [rt text on_text_changed variant disabled alignment]
                              :or {text "" variant :default disabled false}}]
  (wrap-in-alignment-container
    {:fx/type ext-with-text-field-pref-width-bound-props
     :props {:bind 120.0}
     :desc (cond-> {:fx/type fx.text-field/lifecycle
                    :style-class ["text-field" "ext-text-field"]
                    :pseudo-classes #{variant}
                    :disable disabled
                    :text text}
                   on_text_changed (assoc :on-text-changed (make-event-handler-1 rt "on_text_changed" on_text_changed)))}
    alignment
    true))

(defn- text-field-view [props]
  {:fx/type fx/ext-get-env :env [:rt] :desc (assoc props :fx/type text-field-view-impl)})

;; endregion

(def ^:private input-components
  [(make-component
     "icon_button"
     :props (into [] cat [icon-specific-props button-specific-props common-input-props])
     :description "Button with an icon"
     :fn icon-button-view)
   (make-component
     "text_button"
     :props (into [] cat [label-without-variant-specific-props button-specific-props common-input-props])
     :description "Button with a label"
     :fn text-button-view)
   (make-component
     "button"
     :props (into [] cat [button-specific-props [required-child-prop] common-input-props])
     :description "Generic button with a required child component"
     :fn default-button-view)
   (make-component
     "text_check_box"
     :props (into [] cat [label-without-variant-specific-props check-box-specific-props input-with-variant-props])
     :description "Check box with a label"
     :fn text-check-box-view)
   (make-component
     "check_box"
     :props (into [] cat [check-box-specific-props [(assoc required-child-prop :required false)] input-with-variant-props])
     :description "Generic check box with an optional child component"
     :fn check-box-view)
   (make-component
     "select_box"
     :props (into select-box-specific-props input-with-variant-props)
     :description "Dropdown select box with an array of options"
     :fn select-box-view)
   (make-component
     "text_field"
     :props (into text-field-specific-props input-with-variant-props)
     :description "Single-line text input"
     :fn text-field-view)])

;; endregion

;; region dialog components

(defn- dialog-button-view [{:keys [disabled text result cancel default]
                            :or {disabled false cancel false default false}}]
  (let [button {:fx/type fxui/button
                :disable disabled
                :variant (if default :primary :secondary)
                :cancel-button cancel
                :default-button default
                :text text
                :on-action {:result result}}]
    (if default
      {:fx/type fxui/ext-focused-by-default :desc button}
      button)))

(defn- dialog-view [{:keys [title header content buttons]}]
  (let [header (or header {:fx/type fxui/label :text title :variant :header})
        buttons (into []
                      (keep-indexed
                        (fn [i desc]
                          (when desc
                            (assoc desc :fx/key i))))
                      buttons)
        cancel-result (:result (some (fn [desc]
                                       (let [{:keys [props]} (meta desc)]
                                         (when (:cancel props) props)))
                                     buttons))
        footer {:fx/type dialogs/dialog-buttons
                :children (if (coll/empty? buttons)
                            [{:fx/type dialog-button-view :text "Close" :cancel true :default true}]
                            buttons)}]
    (cond-> {:fx/type dialogs/dialog-stage
             :title title
             :on-close-request {:result cancel-result}
             :header header
             :footer footer}
            content
            (assoc :content content))))

(def ^:private dialog-components
  [(make-component
     "dialog_button"
     :props [(make-prop :text
                        :coerce coerce/string
                        :required true
                        :doc "button text")
             (make-prop :result
                        :coerce coerce/untouched
                        :doc "value returned by <code>editor.ui.show_dialog(...)</code> if this button is pressed")
             (make-prop :default
                        :coerce coerce/boolean
                        :doc "if set, pressing <code>Enter</code> in the dialog will trigger this button")
             (make-prop :cancel
                        :coerce coerce/boolean
                        :doc "if set, pressing <code>Escape</code> in the dialog will trigger this button")
             (make-prop :disabled
                        :coerce coerce/boolean
                        :doc "determines if the button is disabled or not")]
     :description "Dialog button shown in the footer of a dialog"
     :fn dialog-button-view)
   (make-component
     "dialog"
     :props [(make-prop :title
                        :coerce coerce/string
                        :required true
                        :doc "OS dialog window title")
             (make-prop :header
                        :coerce child-coercer
                        :types ["component"]
                        :doc "top part of the dialog")
             (make-prop :content
                        :coerce child-coercer
                        :types ["component"]
                        :doc "content of the dialog")
             (make-prop :buttons
                        :coerce children-coercer
                        :types ["component[]"]
                        :doc "array of <code>editor.ui.dialog_button(...)</code> components, footer of the dialog. Defaults to a single Close button")]
     :description "Dialog component, a special component that can't be used as a child of other components"
     :fn dialog-view)])

;; endregion

;; Important: This view allows event handlers to access the editor script runtime.
;; It should be used as a root view everywhere where editor script UI may be displayed
(defn- ext-root-view [{:keys [rt desc]}]
  {:fx/type fx/ext-set-env
   :env {:rt rt}
   :desc desc})

;; region functions

(def ^:private ext-with-stage-props
  (fx/make-ext-with-props fx.stage/props))

(defn- show-dialog-wrapper-view [{:keys [desc] :as props}]
  {:fx/type ext-with-stage-props
   :props {:showing (fxui/dialog-showing? props)}
   :desc desc})

(def functions
  ;; todo function constructor fn?
  [{:name "show_dialog"
    :parameters [{:name "dialog"
                  :types ["component"]
                  :doc "A component that resolves to <code>editor.ui.dialog(...)</code>"}]
    :description "Show a dialog and await for result"
    :fn (rt/suspendable-lua-fn show-dialog [{:keys [rt]} lua-dialog-component]
          (let [desc {:fx/type show-dialog-wrapper-view
                      :desc {:fx/type ext-root-view
                             :rt rt
                             :desc (rt/->clj rt component-coercer lua-dialog-component)}}
                f (future/make)]
            (fx/run-later
              (future/complete!
                f
                (fxui/show-dialog-and-await-result!
                  :event-handler #(assoc %1 ::fxui/result (:result %2))
                  :error-handler #(future/fail! f %)
                  :description desc)))
            f))}])

;; endregion

(def components
  (into [] cat [layout-components
                data-presentation-components
                input-components
                dialog-components]))

(defn env []
  (into {}
        cat
        [(eduction
           (mapcat
             (fn [[k vs]]
               (eduction
                 (map (coll/pair-fn #(enum-const-name k %) coerce/enum-lua-value-cache))
                 vs)))
           enums)
         (eduction
           (map (fn [{:keys [name props fn]}]
                  (let [string-representation (str "editor.ui." name "(...)")
                        {req true opt false} (iutil/group-into {} {} :required (coll/pair-fn :name :coerce) props)
                        props-coercer (coerce/hash-map :req req :opt opt)]
                    [name
                     (rt/lua-fn component-factory [{:keys [rt]} lua-props]
                       (let [props (rt/->clj rt props-coercer lua-props)]
                         (-> (fn props)
                             (vary-meta assoc :type :component :props props)
                             (rt/wrap-userdata string-representation))))])))
           components)
         (eduction
           (map (coll/pair-fn :name :fn))
           functions)]))


(comment
  (fx/on-fx-thread
    (fx/create-component
      {:fx/type :stage
       :showing true
       :scene {:fx/type :scene
               :stylesheets [(str (clojure.java.io/resource "dialogs.css"))]
               :root {:fx/type :h-box
                      :style-class "dialog-body"
                      :padding 50
                      :spacing 10
                      :children [(-> {:name :open-resource :alignment :center}
                                     (icon-view)
                                     #_(assoc :style {:-fx-background-color "#880000"}))
                                 (-> {:name :minus :alignment :center}
                                     (icon-view)
                                     #_(assoc :style {:-fx-background-color "#880000"}))
                                 (-> {:text "Hello!" :alignment :center}
                                     (label-view)
                                     #_(assoc :style {:-fx-background-color "#008800"}))
                                 (-> {:name :clear}
                                     (icon-button-view))
                                 (-> {:name :open-resource :disabled true}
                                     (icon-button-view))
                                 (-> {:text "Hello world!"}
                                     (text-button-view))
                                 (-> {:text "Oh..." :disabled true}
                                     (text-button-view))]}}})))


;(def input-variant-coercer
;  (coerce/enum :default :warning :error))
;
;(def input-props
;  (assoc common-props
;    :disabled coerce/boolean
;    :variant input-variant-coercer))
;
;(def check-box
;  (create-component-factory
;    :check-box
;    (coerce/hash-map
;      :opt (assoc input-props
;             :value coerce/boolean
;             :on_value_changed coerce/function
;             :child maybe-child-coercer))))
;
;(def select-box
;  (create-component-factory
;    :select-box
;    (coerce/hash-map
;      :opt (assoc input-props
;             :value coerce/untouched
;             :on_value_changed coerce/function
;             :options (coerce/vector-of
;                        (coerce/one-of
;                          (coerce/tuple coerce/untouched coerce/string)
;                          coerce/untouched))))))
;
;(def text-field
;  (create-component-factory
;    :text-field
;    (coerce/hash-map
;      :opt (assoc input-props
;             :text coerce/string
;             :on_text_changed coerce/function))))
;
;(def value-field-opt-props
;  (assoc input-props
;    :value coerce/untouched
;    :on_value_changed coerce/function))
;
;(def value-field
;  (create-component-factory
;    :value-field
;    (coerce/hash-map
;      :req {:to_string coerce/function
;            :to_value coerce/function}
;      :opt value-field-opt-props)))
;
;(def string-field
;  (create-component-factory
;    :string-field
;    (coerce/hash-map :opt value-field-opt-props)))
;
;(def number-field
;  (create-component-factory
;    :number-field
;    (coerce/hash-map :opt value-field-opt-props)))
;
;(def integer-field
;  (create-component-factory
;    :integer-field
;    (coerce/hash-map :opt value-field-opt-props)))
