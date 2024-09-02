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

(ns editor.editor-extensions.ui-components
  (:require [cljfx.api :as fx]
            [cljfx.component :as fx.component]
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
            [dynamo.graph :as g]
            [editor.dialogs :as dialogs]
            [editor.editor-extensions.coerce :as coerce]
            [editor.editor-extensions.runtime :as rt]
            [editor.error-reporting :as error-reporting]
            [editor.field-expression :as field-expression]
            [editor.fs :as fs]
            [editor.future :as future]
            [editor.fxui :as fxui]
            [editor.icons :as icons]
            [editor.lua-completion :as lua-completion]
            [editor.ui :as ui]
            [editor.util :as util]
            [internal.util :as iutil]
            [util.coll :as coll :refer [pair]])
  (:import [com.defold.control DefoldStringConverter]
           [com.defold.editor.luart DefoldVarArgFn]
           [java.nio.file Path]
           [java.util Collection List]
           [javafx.animation SequentialTransition TranslateTransition]
           [javafx.beans Observable]
           [javafx.beans.binding Bindings]
           [javafx.beans.property ReadOnlyProperty]
           [javafx.beans.value ChangeListener]
           [javafx.event Event]
           [javafx.scene Node]
           [javafx.scene.control CheckBox ComboBox ScrollPane TextField]
           [javafx.scene.control.skin ScrollPaneSkin]
           [javafx.scene.input KeyCode KeyEvent]
           [javafx.scene.layout Region]
           [javafx.stage DirectoryChooser FileChooser FileChooser$ExtensionFilter]
           [javafx.util Duration]
           [org.luaj.vm2 LuaError LuaValue]))

(set! *warn-on-reflection* true)

;; See also:
;; - Components doc: https://docs.google.com/document/d/1e6kmVLspQEoe17Ys1nmbbf54cqUn2fNPZowIe7JBwl4/edit
;; - Reactive UI doc: https://docs.google.com/document/d/1cgqFPLUc3YQih2tsEA067Pz9Q0nWlKoVflqLEVX8CPY/edit

;; Components are created with a table of props. The editor defines prop maps
;; both for runtime behavior (coercion) and autocomplete generation.

(s/def :editor.editor-extensions.components.prop/name simple-keyword?)
(s/def ::coerce ifn?)
(s/def ::required boolean?)
(s/def ::types (s/coll-of string? :min-count 1 :distinct true))
(s/def ::doc string?)
(s/def ::prop
  (s/keys :req-un [:editor.editor-extensions.components.prop/name ::coerce ::required ::types ::doc]))

(defn- ^{:arglists '([name & {:keys [coerce required types doc]}])} make-prop
  "Construct a prop definition map

  Args:
    name    keyword name of a prop, should be snake_cased

  Kv-args:
    :coerce      required, coercer function
    :required    optional, boolean, whether the prop is required to be present
    :types       required if it can't be inferred from coercer, coll of string
                 type names (union) for documentation
    :doc         required, html documentation string"
  [name & {:as m}]
  {:post [(s/assert ::prop %)]}
  (let [m (-> m
              (assoc :name name)
              (update :required boolean))]
    (if (contains? m :types)
      m
      (if-let [inferred-types (condp = (:coerce m)
                                coerce/boolean ["boolean"]
                                coerce/string ["string"]
                                coerce/untouched ["any"]
                                coerce/function ["function"]
                                nil)]
        (assoc m :types inferred-types)
        m))))

(s/def :editor.editor-extensions.components.component/name string?)
(s/def ::props (s/coll-of ::prop))
(s/def ::fn ifn?)
(s/def ::description string?)
(s/def ::component
  (s/keys :req-un [:editor.editor-extensions.components.component/name ::props ::fn ::description]))

(defn- ^{:arglists '([name & {:keys [props description fn]}])} make-component
  "Construct a component definition map

  Args:
    name    string name of the component, should be snake_cased

  Kv-args:
    :props          required, a list of props of the component
    :fn             required, fn that converts a prop map to a cljfx component
    :description    required, markdown documentation string"
  [name & {:as m}]
  {:post [(s/assert ::component %)]}
  (assoc m :name name))

(defn- props-doc-html [props]
  (lua-completion/args-doc-html
    (map #(update % :name name) props)))

(defn- component->completion [{:keys [props] :as component}]
  (let [{req true opt false} (group-by :required props)]
    (lua-completion/make
      (assoc component
        :type :function
        :parameters [{:name "props"
                      :types ["table"]
                      :doc (str (when req
                                  (str "Required props:\n"
                                       (props-doc-html req)
                                       "\n\n"))
                                "Optional props:\n"
                                (props-doc-html opt))}]
        :returnvalues [{:name "value" :types ["component"]}]))))

(s/def ::create ifn?)
(s/def ::advance ifn?)
(s/def ::return ifn?)
(s/def ::hook
  (s/keys :req-un [:editor.editor-extensions.components.component/name ::create ::advance ::return]))

(defn- ^{:arglists '([name & {:keys [create advance return]}])} make-hook
  "Construct a hook definition map

  Args:
    name    string name of the function, should start with \"use_\" and be
            snake_cased

  Kv-args:
    :create          required, hook function that is executed when the hook is
                     created, it receives following arguments:
                       component-atom        atom of the component, can be
                                             modified using `update-hook-state!`
                       component-state       component atom's state at the point
                                             when the hook is initialized, i.e.
                                             it has a valid :current-hook-index
                                             key that needs to be passed to the
                                             `update-hook-state!` function
                       evaluation-context    evaluation context for the
                                             invocation
                       ...rest of lua args passed by the editor script
                     the function returns its state, any value
    :advance         required, hook function that is invoked on subsequent UI
                     updates, it receives following arguments:
                       component-state       component atom's state as in
                                             :create
                       old-hook-state        previously returned hook state
                       evaluation-context    evaluation context for the
                                             invocation
                       ...rest of lua args passed by the editor script
                     the function returns its new state, any value
    :return          a function from hook state to lua value that is then
                     returned to the calling editor script"
  [name & {:as m}]
  {:post [(s/assert ::hook %)]}
  (assoc m :name name))

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

(defn- component->lua-fn [{:keys [name props fn]}]
  (let [string-representation (str "editor.ui." name "(...)")
        {req true opt false} (iutil/group-into {} {} :required (coll/pair-fn :name :coerce) props)
        props-coercer (coerce/hash-map :req req :opt opt)]
    (rt/lua-fn component-factory [{:keys [rt]} lua-props]
      (let [props (rt/->clj rt props-coercer lua-props)]
        (-> (fn (assoc props :rt rt))
            (vary-meta assoc :type :component :props props)
            (rt/wrap-userdata string-representation))))))

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
;; - all components have `alignment` property that configures how the
;;   component content is aligned within its assigned bounds (default: top-left)
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

(defn- make-list-view-fn [fx-type expand-key]
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
            alignment (apply-alignment alignment))))

(def ^:private horizontal-view (make-list-view-fn fx.h-box/lifecycle :h-box/hgrow))

(def ^:private vertical-view (make-list-view-fn fx.v-box/lifecycle :v-box/vgrow))

(def ^:private layout-components
  [(make-component
     "horizontal"
     :description "Layout container that places its children in a horizontal row one after another"
     :props list-props
     :fn horizontal-view)
   (make-component
     "vertical"
     :description "Layout container that places its children in a vertical column one after another"
     :props list-props
     :fn vertical-view)
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
     :fn scroll-view)])

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

(defn- report-error-to-script [rt label ex]
  (.println (rt/stderr rt)
            (str label " failed: " (or (ex-message ex) (.getSimpleName (class ex))))))

(defn- report-async-error-to-script [future rt label]
  (future/catch future #(report-error-to-script rt label %)))

(def ^:private ^:dynamic *owner-window* nil)

(defn- current-owner-window []
  (or *owner-window* (ui/main-stage)))

(defn- invoke-event-handler
  ([owner-window rt label lua-fn]
   (binding [*owner-window* owner-window]
     (report-async-error-to-script (rt/invoke-suspending-1 rt lua-fn) rt label)))
  ([owner-window rt label lua-fn value]
   (binding [*owner-window* owner-window]
     (report-async-error-to-script (rt/invoke-suspending-1 rt lua-fn (rt/->lua value)) rt label))))

(defn- make-event-handler-0 [event->owner-window rt label lua-fn]
  (fn event-handler-0 [e]
    (invoke-event-handler (event->owner-window e) rt label lua-fn)))

(defn- make-event-handler-1 [event->owner-window event->value rt label lua-fn]
  (fn event-handler-1 [e]
    (invoke-event-handler (event->owner-window e) rt label lua-fn (event->value e))))

;; region button

(def ^:private button-specific-props
  [(make-prop :on_pressed
              :coerce coerce/function
              :doc "button press callback")])

(defn- event-source->owner-window [^Event e]
  (.getWindow (.getScene ^Node (.getSource e))))

(defn- button-view [{:keys [rt on_pressed disabled alignment style-class child]
                     :or {disabled false}}]
  {:pre [(string? style-class)]}
  (-> {:fx/type fx.button/lifecycle
       :disable disabled
       :focus-traversable false
       :style-class ["ext-button" style-class]
       :graphic child}
      (cond-> on_pressed
              (assoc :on-action (make-event-handler-0 event-source->owner-window rt "on_pressed" on_pressed)))
      (wrap-in-alignment-container alignment)))

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

(def ^:private property-change-listener-with-source-owner-window-lifecycle
  (fx.lifecycle/wrap-coerce
    fx.lifecycle/event-handler
    (fn [f]
      (reify ChangeListener
        (changed [_ observable-value _ v]
          (f {:window (.getWindow (.getScene ^Node (.getBean ^ReadOnlyProperty observable-value)))
              :value v}))))))

(def ^:private ext-with-extra-check-box-props
  (fx/make-ext-with-props
    {:on-selected-changed (fx.prop/make
                            (fx.mutator/property-change-listener #(.selectedProperty ^CheckBox %))
                            property-change-listener-with-source-owner-window-lifecycle)}))

(defn- check-box-view [{:keys [rt alignment variant disabled value on_value_changed child]
                        :or {disabled false
                             value false
                             variant :default}}]
  (wrap-in-alignment-container
    {:fx/type ext-with-extra-check-box-props
     :desc (cond-> {:fx/type fx.check-box/lifecycle
                    :style-class ["check-box" "ext-check-box"]
                    :alignment :top-left
                    :pseudo-classes #{variant}
                    :disable disabled
                    :selected value}
                   child (assoc :graphic {:fx/type fx.v-box/lifecycle
                                          :style-class "ext-check-box-child"
                                          :children [child]}))
     :props (cond-> {} on_value_changed (assoc :on-selected-changed (make-event-handler-1 :window :value rt "on_value_changed" on_value_changed)))}
    alignment))

(defn- text-check-box-view [props]
  (check-box-view (assoc props :child (label-view (dissoc props :alignment :variant)))))

;; endregion

;; region select_box

(def ^:private select-box-specific-props
  [(make-prop :value :coerce coerce/untouched :doc "selected value")
   (make-prop :on_value_changed :coerce coerce/function :doc "change callback, will receive the selected value")
   (make-prop :options :coerce (coerce/vector-of coerce/untouched) :doc "array of selectable options" :types ["any[]"])
   (make-prop :to_string :coerce coerce/function :doc "function that converts an item to string, defaults to <code>tostring</code>")])

(defn- stringify-lua-value
  ([rt maybe-lua-value]
   (if (nil? maybe-lua-value)
     ""
     (rt/->clj rt coerce/to-string maybe-lua-value)))
  ([rt to_string maybe-lua-value]
   (if (nil? maybe-lua-value)
     ""
     (rt/->clj rt coerce/to-string (rt/invoke-immediate-1 rt to_string maybe-lua-value)))))

(defn- create-select-box-string-converter [rt to_string]
  ;; Note: if a combo box has no value provided, the default is JVM null
  (DefoldStringConverter.
    (if to_string
      #(stringify-lua-value rt to_string %)
      #(stringify-lua-value rt %))))

(defn- on-select-box-key-pressed [^KeyEvent event]
  (when (= KeyCode/SPACE (.getCode event))
    (.show ^ComboBox (.getSource event))
    (.consume event)))

(def ^:private ext-with-extra-combo-box-props
  (fx/make-ext-with-props
    {:on-value-changed (fx.prop/make
                         (fx.mutator/property-change-listener #(.valueProperty ^ComboBox %))
                         property-change-listener-with-source-owner-window-lifecycle)}))

(defn- select-box-view [{:keys [rt alignment variant disabled value options on_value_changed to_string]
                         :or {options []
                              disabled false
                              variant :default}}]
  (wrap-in-alignment-container
    {:fx/type ext-with-extra-combo-box-props
     :desc {:fx/type fxui/ext-memo
            :fn create-select-box-string-converter
            :args [rt to_string]
            :key :converter
            :desc {:fx/type fx.combo-box/lifecycle
                   :value value
                   :pseudo-classes #{variant}
                   :on-key-pressed on-select-box-key-pressed
                   :items options
                   :disable disabled}}
     :props (cond-> {} on_value_changed (assoc :on-value-changed (make-event-handler-1 :window :value rt "on_value_changed" on_value_changed)))}
    alignment
    true))

;; endregion

;; region text field

(def ^:private text-field-specific-props
  [(make-prop :text :coerce coerce/string :doc "text")
   (make-prop :on_text_changed :coerce coerce/function :doc "text change callback, will receive the new text")])

(def ^:private ext-with-extra-text-field-props
  (fx/make-ext-with-props
    {:on-text-changed (fx.prop/make
                        (fx.mutator/property-change-listener #(.textProperty ^TextField %))
                        property-change-listener-with-source-owner-window-lifecycle)}))

(defn- text-field-view [{:keys [rt text on_text_changed variant disabled alignment]
                         :or {text "" variant :default disabled false}}]
  (wrap-in-alignment-container
    {:fx/type ext-with-extra-text-field-props
     :desc {:fx/type fx.text-field/lifecycle
            :style-class ["text-field" "ext-text-field"]
            :pseudo-classes #{variant}
            :disable disabled
            :text text}
     :props (cond-> {} on_text_changed (assoc :on-text-changed (make-event-handler-1 :window :value rt "on_text_changed" on_text_changed)))}
    alignment true))

;; endregion

;; region value field

(def ^:private value-field-specific-props
  [(make-prop :value :coerce coerce/untouched :doc "value")
   (make-prop :on_value_changed :coerce coerce/function :doc "value change callback, will receive the new value")])

(def ^:private convertible-value-field-specific-props
  (into [(make-prop :to_value :coerce coerce/function :required true
                    :doc "covert string to value, should return converted value or <code>nil</code> if not convertible")
         (make-prop :to_string :coerce coerce/function :required true
                    :doc "covert value to string, should always return string")]
        value-field-specific-props))

(def ^:private value-field-value-coercer
  ;; if field value is null, coerce it to JVM null (for special handling in
  ;; to-string part of the converter)
  (coerce/one-of coerce/null coerce/untouched))

(defn- notify-value-field-change [owner-window rt on_value_changed old-maybe-lua-value new-lua-value]
  (let [notify (or (nil? old-maybe-lua-value)
                   (not (rt/eq? rt old-maybe-lua-value new-lua-value)))]
    (when (and notify on_value_changed)
      (invoke-event-handler owner-window rt "on_value_changed" on_value_changed new-lua-value))
    notify))

(defn- meaningful-value-field-edit? [edit maybe-lua-value text]
  (and (some? edit)
       (or (nil? maybe-lua-value)
           (not= edit text))))

(defn- on-value-field-key-pressed [rt maybe-lua-value text on_value_changed to_value edit swap-state ^KeyEvent e]
  (condp = (.getCode e)
    KeyCode/ENTER
    (when (meaningful-value-field-edit? edit maybe-lua-value text)
      (let [maybe-new-lua-value (rt/->clj rt value-field-value-coercer (rt/invoke-immediate-1 rt to_value (rt/->lua edit)))]
        ;; nil result from `to_value` means couldn't convert => keep editing
        (if (nil? maybe-new-lua-value)
          (let [^TextField text-field (.getSource e)
                anim (SequentialTransition. text-field)]
            (doto (.getChildren anim)
              (.add (doto (TranslateTransition. (Duration. 30.0)) (.setByX 4.0)))
              (.add (doto (TranslateTransition. (Duration. 30.0)) (.setByX -8.0)))
              (.add (doto (TranslateTransition. (Duration. 30.0)) (.setByX 7.0)))
              (.add (doto (TranslateTransition. (Duration. 30.0)) (.setByX -4.0)))
              (.add (doto (TranslateTransition. (Duration. 30.0)) (.setByX 1.0))))
            (.play anim)
            (.consume e))
          ;; new value!
          (do (swap-state #(-> % (assoc :value maybe-new-lua-value) (dissoc :edit)))
              (when (notify-value-field-change (event-source->owner-window e) rt on_value_changed maybe-lua-value maybe-new-lua-value)
                (.consume e))))))

    KeyCode/ESCAPE
    (when edit
      (swap-state dissoc :edit)
      (.consume e))

    nil))

(defn- on-value-field-focus-changed [rt maybe-lua-value text on_value_changed to_value edit swap-state {focused :value owner-window :window}]
  (when (and (not focused)
             (meaningful-value-field-edit? edit maybe-lua-value text))
    (let [maybe-new-lua-value (rt/->clj rt value-field-value-coercer (rt/invoke-immediate-1 rt to_value (rt/->lua edit)))]
      (if (nil? maybe-new-lua-value)
        (swap-state dissoc :edit)
        (do (swap-state #(-> % (assoc :value maybe-new-lua-value) (dissoc :edit)))
            (notify-value-field-change owner-window rt on_value_changed maybe-lua-value maybe-new-lua-value))))))

(def ^:private ext-with-extra-value-field-props
  (fx/make-ext-with-props
    {:text (fx.prop/make
             (fx.mutator/setter
               (fn [^TextField text-field text]
                 (when-not (= text (.getText text-field))
                   (.setText text-field text)
                   (.selectAll text-field))))
             fx.lifecycle/scalar)
     :on-focused-changed (fx.prop/make
                           (fx.mutator/property-change-listener #(.focusedProperty ^TextField %))
                           property-change-listener-with-source-owner-window-lifecycle)}))

(defn- value-field-view-impl-2
  [{:keys [text                                             ;; text of the current value
           state swap-state                                 ;; local state, may contain :value and :edit
           rt
           value                                            ;; current (maybe) lua value, either value from local state or from editor script
           on_value_changed
           to_value
           #_to_string                                      ;; the key is here, but unused, see value-field-view-impl-1
           variant
           disabled
           alignment]
    :or {variant :default
         disabled false}}]
  (let [{:keys [edit]} state]
    (wrap-in-alignment-container
      {:fx/type ext-with-extra-value-field-props
       :props {:text (or edit text)
               :on-focused-changed #(on-value-field-focus-changed rt value text on_value_changed to_value edit swap-state %)}
       :desc {:fx/type fx.text-field/lifecycle
              :style-class ["text-field" "ext-text-field"]
              :disable disabled
              :pseudo-classes #{variant}
              :on-text-changed #(swap-state assoc :edit %)
              :on-key-pressed #(on-value-field-key-pressed rt value text on_value_changed to_value edit swap-state %)}}
      alignment
      true)))

(defn- value-field-view-impl-1 [{:keys [state rt value to_string]
                                 :as props}]
  ;; overrides :value to point to the current value of the control, which might
  ;; differ from the one assigned from outside.
  ;; adds value's :text to props
  (let [current-value (:value state value)]
    {:fx/type fxui/ext-memo
     :fn stringify-lua-value
     :args [rt to_string current-value]
     :key :text
     :desc (assoc props :fx/type value-field-view-impl-2 :value current-value)}))

(defn- value-field-view [props]
  ;; adds :state and :swap-state to props
  {:fx/type fx/ext-state
   ;; we put value as a "key" of the initial state so that when value changes, the
   ;; local state will reset. Other keys used by this local state are:
   ;;   :value    last entered value, never lua nil
   ;;   :edit     currently edited text, a string
   :initial-state {:key (:value props)}
   :desc (assoc props :fx/type value-field-view-impl-1)})

;; endregion

;; regions value_field's friends

(def ^:private lua-to-string-fn
  (DefoldVarArgFn.
    (fn to-string [^LuaValue arg]
      ;; translated from package-private org.luaj.vm2.lib.BaseLib$tostring
      (let [h (.metatag arg LuaValue/TOSTRING)]
        (if-not (.isnil h)
          (.call h arg)
          (let [v (.tostring arg)]
            (if-not (.isnil v)
              v
              (LuaValue/valueOf (.tojstring arg)))))))))

(defn- string-field-view [props]
  (assoc props :fx/type value-field-view
               :to_string lua-to-string-fn
               :to_value lua-to-string-fn))

(def ^:private lua-to-integer-fn
  (DefoldVarArgFn.
    (fn to-lua-integer [^LuaValue arg]
      (rt/->lua (field-expression/to-long (.tojstring arg))))))

(defn- integer-field-view [props]
  (assoc props :fx/type value-field-view
               :to_string lua-to-string-fn
               :to_value lua-to-integer-fn))

(def ^:private lua-to-number-fn
  (DefoldVarArgFn.
    (fn to-lua-number [^LuaValue arg]
      (rt/->lua (field-expression/to-double (.tojstring arg))))))

(defn- number-field-view [props]
  (assoc props :fx/type value-field-view
               :to_string lua-to-string-fn
               :to_value lua-to-number-fn))

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
     :description "Single-line text field, reports changes on typing"
     :fn text-field-view)
   (make-component
     "value_field"
     :props (into convertible-value-field-specific-props input-with-variant-props)
     :description "Input component based on a text field, reports changes on commit (<code>Enter</code> or focus loss). See also: <code>string_field</code>, <code>number_field</code> and <code>integer_field</code>"
     :fn value-field-view)
   (make-component
     "string_field"
     :props (into value-field-specific-props input-with-variant-props)
     :description "String input component based on a text field, reports changes on commit (<code>Enter</code> or focus loss)"
     :fn string-field-view)
   (make-component
     "integer_field"
     :props (into value-field-specific-props input-with-variant-props)
     :description "Integer input component based on a text field, reports changes on commit (<code>Enter</code> or focus loss)"
     :fn integer-field-view)
   (make-component
     "number_field"
     :props (into value-field-specific-props input-with-variant-props)
     :description "Number input component based on a text field, reports changes on commit (<code>Enter</code> or focus loss)"
     :fn number-field-view)])

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

;; region functions

;; The value gets bound to a volatile with evaluation context. When the context
;; becomes invalid (fills the cache after use), the volatile is set to nil to
;; signal that this context can't be used anymore. The default value is a
;; reduced instance to support `deref`, but not `vreset!`
(def ^:private ^:dynamic *lifecycle-evaluation-context-ideref* (reduced nil))

(defn- lifecycle-evaluation-context []
  {:post [(some? %)]}
  @*lifecycle-evaluation-context-ideref*)

(defmacro ^:private with-nestable-evaluation-context [& body]
  `(if @*lifecycle-evaluation-context-ideref*
     (do ~@body)
     (g/with-auto-evaluation-context ec#
       (let [v# (volatile! ec#)]
         (binding [*lifecycle-evaluation-context-ideref* v#]
           (let [ret# (do ~@body)]
             (vreset! v# nil)
             ret#))))))

(def evaluation-context-lifecycle
  "Root lifecycle that must be used to re-use evaluation context in cljfx tree

  This lifecycle shares the same evaluation context along the whole cljfx tree
  during cljfx lifecycle updates.

  Expected keys:
    :desc    child description that will be able to access a shared evaluation
             context using lifecycle-evaluation-context fn"
  (reify fx.lifecycle/Lifecycle
    (create [_ {:keys [desc]} opts]
      (with-nestable-evaluation-context
        (fx.lifecycle/create fx.lifecycle/dynamic desc opts)))
    (advance [_ component {:keys [desc]} opts]
      (with-nestable-evaluation-context
        (fx.lifecycle/advance fx.lifecycle/dynamic component desc opts)))
    (delete [_ component opts]
      (with-nestable-evaluation-context
        (fx.lifecycle/delete fx.lifecycle/dynamic component opts)))))

;; region show_dialog

(def ^:private ext-with-stage-props
  (fx/make-ext-with-props fx.stage/props))

(defn- show-dialog-wrapper-view [{:keys [desc] :as props}]
  {:fx/type ext-with-stage-props
   :props {:showing (fxui/dialog-showing? props)}
   :desc desc})

(def ^:private show-dialog-completion
  (lua-completion/make
    {:name "show_dialog"
     :type :function
     :description "Show a dialog and await for result"
     :parameters [{:name "dialog"
                   :types ["component"]
                   :doc "A component that resolves to <code>editor.ui.dialog(...)</code>"}]
     :returnvalues [{:name "value"
                     :types ["any"]
                     :doc "dialog result"}]}))

(def ^:private lua-show-dialog-fn
  (rt/suspendable-lua-fn show-dialog [{:keys [rt]} lua-dialog-component]
    (let [desc {:fx/type show-dialog-wrapper-view
                :desc {:fx/type evaluation-context-lifecycle
                       :desc (rt/->clj rt component-coercer lua-dialog-component)}}
          f (future/make)]
      (fx/run-later
        (future/complete!
          f
          (fxui/show-dialog-and-await-result!
            :event-handler #(assoc %1 ::fxui/result (:result %2))
            :error-handler #(future/fail! f %)
            :description desc)))
      f)))

;; endregion

;; region show_external_file_dialog

(def ^:private external-file-dialog-title-doc
  "OS window title")

(def ^:private external-file-dialog-filters-doc-types
  ["string[]"])

(def ^:private external-file-dialog-filters-doc
  (str "File filters, an array of filter tables, where each filter has following keys:"
       (lua-completion/args-doc-html
         [{:name "description"
           :types ["string"]
           :doc "string explaining the filter, e.g. <code>\"Text files (*.txt)\"</code>"}
          {:name "extensions"
           :types external-file-dialog-filters-doc-types
           :doc "array of file extension patterns, e.g. <code>\"*.txt\"</code>, <code>\"*.*\"</code> or <code>\"game.project\"</code>"}])))

(def ^:private external-file-dialog-filters-coercer
  (coerce/vector-of
    (coerce/hash-map
      :req {:description coerce/string
            :extensions (coerce/vector-of
                          coerce/string
                          :min-count 1
                          :distinct true)})
    :min-count 1))

(def ^:private show-external-file-dialog-opts-coercer
  (coerce/hash-map
    :opt {:title coerce/string
          :path coerce/string
          :filters external-file-dialog-filters-coercer}))

(def ^:private show-external-file-dialog-completion
  (lua-completion/make
    {:name "show_external_file_dialog"
     :type :function
     :description "Show OS file selection dialog and await for result"
     :parameters [{:name "[opts]"
                   :types ["table"]
                   :doc (lua-completion/args-doc-html
                          [{:name "path"
                            :types ["string"]
                            :doc "initial file or directory path used by the dialog; resolved against project root if relative"}
                           {:name "title"
                            :types ["string"]
                            :doc external-file-dialog-title-doc}
                           {:name "filters"
                            :types ["table[]"]
                            :doc external-file-dialog-filters-doc}])}]
     :returnvalues [{:name "value"
                     :types ["string" "nil"]
                     :doc "Either absolute file path or nil if user canceled file selection"}]}))

(defn- make-show-external-file-dialog-lua-fn [^Path project-path]
  (rt/suspendable-lua-fn select-external-file
    ([ctx]
     (select-external-file ctx nil))
    ([{:keys [rt]} maybe-lua-opts]
     (let [opts (when maybe-lua-opts
                  (rt/->clj rt show-external-file-dialog-opts-coercer maybe-lua-opts))
           {:keys [title ^String path filters]
            :or {title "Select File" path "."}} opts
           initial-path (.normalize (.resolve project-path path))
           [^Path initial-directory-path initial-file-name]
           (if (fs/path-exists? initial-path)
             (if (fs/path-is-directory? initial-path)
               [initial-path nil]
               [(.getParent initial-path) (str (.getFileName initial-path))])
             (let [parent-path (.getParent initial-path)]
               (if (fs/path-is-directory? parent-path)
                 [parent-path (str (.getFileName initial-path))]
                 [project-path nil])))
           dialog (doto (FileChooser.)
                    (.setTitle title)
                    (.setInitialDirectory (.toFile initial-directory-path)))]
       (when filters
         (.addAll (.getExtensionFilters dialog)
                  ^Collection
                  (mapv (fn [{:keys [^String description extensions]}]
                          (FileChooser$ExtensionFilter.
                            description
                            ^List extensions))
                        filters)))
       (when initial-file-name
         (.setInitialFileName dialog initial-file-name))
       (let [f (future/make)
             owner-window (current-owner-window)]
         (fx/run-later
           (try
             (future/complete! f (some-> (.showOpenDialog dialog owner-window) str))
             (catch Throwable e
               (future/fail! f e))))
         f)))))

;; endregion

;; region show_external_directory_dialog

(def ^:private show-external-directory-dialog-opts-coercer
  (coerce/hash-map
    :opt {:title coerce/string
          :path coerce/string}))

(def ^:private show-external-directory-dialog-completion
  (lua-completion/make
    {:name "show_external_directory_dialog"
     :type :function
     :description "Show OS directory selection dialog and await for result"
     :parameters [{:name "[opts]"
                   :types ["table"]
                   :doc (lua-completion/args-doc-html
                          [{:name "path"
                            :types ["string"]
                            :doc "initial file or directory path used by the dialog; resolved against project root if relative"}
                           {:name "title"
                            :types ["string"]
                            :doc external-file-dialog-title-doc}])}]
     :returnvalues [{:name "value"
                     :types ["string" "nil"]
                     :doc "Either absolute directory path or nil if user canceled directory selection"}]}))

(defn- make-show-external-directory-dialog-lua-fn [^Path project-path]
  (rt/suspendable-lua-fn show-external-directory-dialog
    ([ctx]
     (show-external-directory-dialog ctx nil))
    ([{:keys [rt]} maybe-lua-opts]
     (let [opts (when maybe-lua-opts
                  (rt/->clj rt show-external-directory-dialog-opts-coercer maybe-lua-opts))
           {:keys [title ^String path]
            :or {title "Select Directory" path "."}} opts
           resolved-path (.normalize (.resolve project-path path))
           ^Path initial-path (cond
                                (not (fs/path-exists? resolved-path)) project-path
                                (fs/path-is-directory? resolved-path) resolved-path
                                :else (.getParent resolved-path))
           dialog (doto (DirectoryChooser.)
                    (.setTitle title)
                    (.setInitialDirectory (.toFile initial-path)))
           f (future/make)]
       (fx/run-later
         (try
           (future/complete! f (some-> (.showDialog dialog (current-owner-window)) str))
           (catch Throwable e (future/fail! f e))))
       f))))

;; endregion

;; region external_file_field

;; The implementation is in prelude.lua!

(def ^:private external-file-field-props
  (into [(make-prop :value :coerce coerce/string :doc "file or directory path; resolved against project root if relative")
         (make-prop :on_value_changed :coerce coerce/function :doc "value change callback, will receive the new file path")
         (make-prop :title :coerce coerce/string :doc external-file-dialog-title-doc)
         (make-prop :filters :coerce external-file-dialog-filters-coercer :doc external-file-dialog-filters-doc :types external-file-dialog-filters-doc-types)]
        input-with-variant-props))

(def ^:private external-file-field-completion
  (component->completion
    {:name "external_file_field"
     :props external-file-field-props}))

;; endregion

;; region component

;; This dynamic makes the hooks work. It's bound when component lua function is
;; invoked, and hooks use the component atom for setting up their behavior.
;; Component atom contains a map with following keys:
;;   :hooks                 a vector of maps with following keys:
;;                            :hook    the hook map, created with make-hook
;;                            :state   the hook state
;;   :current-hook-index    integer identifying the current hook in :hooks. when
;;                          component function is called, it's zero, and then it
;;                          gets incremented after every hook use. After the
;;                          component function returns, it's used to verify that
;;                          there was an expected number of hooks calls.
;;   :mode                  either :create, :advance or :delete. When the
;;                          component instance is created in cljfx tree, mode is
;;                          set to :create, then it's set to :advance on
;;                          updates, and finally it's set to :delete on cljfx
;;                          component delete. Mode affects how hooks work, and
;;                          whether the re-renders are scheduled
;;   :rt                    editor extensions runtime
;;   :lua-fn                the component lua function
;;   :lua-props             the component lua props table
;;   :opts                  cljfx opts
;;   :render-counter        render counter. atom has a watcher that schedules
;;                          the view refresh when counter is set from 0 to 1.
;;                          Hooks might increment the counter, which will
;;                          schedule a refresh. Since JavaFX is mutable, cljfx
;;                          updates cannot be retried. The watcher performs a
;;                          refresh on the UI thread with a particular counter
;;                          in mind, and then updates the atom, either resetting
;;                          the counter back to zero if the counter didn't
;;                          change, or scheduling another refresh if it changed
;;                          during the update.
(def ^:private ^:dynamic *current-component-atom* nil)

;; used in `swap!`
(defn- complete-component-rendering [component-state desc child render-counter]
  (-> component-state
      (assoc :desc desc :child child)
      (cond-> (= render-counter (:render-counter component-state))
              (assoc :render-counter 0))))

;; used in `swap!`
(defn- complete-component-advance [component-state lua-props desc child opts render-counter]
  (-> component-state
      (assoc :lua-props lua-props :opts opts)
      (complete-component-rendering desc child render-counter)))

;; used in `swap!`
(defn- complete-hook-triggered-advance [component-state desc child render-counter]
  (complete-component-rendering component-state desc child render-counter))

;; not used in `swap!` since fx.lifecycle/advance cannot be retried. returns new child
(defn- advance-component-child [old-child desc opts]
  (let [old-instance (fx.component/instance old-child)
        new-child (fx.lifecycle/advance fx.lifecycle/root old-child desc opts)
        new-instance (fx.component/instance new-child)]
    ;; It's not allowed to change the final JavaFX node instance on a component atom
    ;; because the change will not be picked up by cljfx. In cljfx, components are
    ;; considered immutable descriptions of mutable objects. Re-assigning the node
    ;; happens if node instance of old component is not the same as node instance of
    ;; the new component. But since function component stays the same — an atom —
    ;; both old and new instances are going to be the same. In practice, this is not
    ;; an issue, since function components typically return the same root instance,
    ;; and the function is only needed for local state that affects behavior.
    (when-not (= old-instance new-instance)
      (throw (LuaError. "component function returned a different UI component instance, which is not allowed")))
    new-child))

(defn- invoke-with-hooks [component-atom rt lua-fn lua-props]
  (binding [*current-component-atom* component-atom]
    (let [ret (rt/invoke-immediate-1 rt lua-fn lua-props (lifecycle-evaluation-context))
          {:keys [hooks current-hook-index]} @component-atom]
      (swap! component-atom assoc :current-hook-index 0)
      (when (< current-hook-index (count hooks))
        ;; the check if there were more hooks than necessary is in the main hook
        ;; dispatcher: see make-hook-lua-fn
        (throw (LuaError. "component function used fewer hooks than expected")))
      ret)))

(defn- perform-hook-triggered-render [component-atom]
  (try
    (let [{:keys [mode render-counter lua-fn lua-props rt child opts]} @component-atom]
      (when (and (not= :delete mode) (pos? render-counter))
        (try
          (let [new-state (with-nestable-evaluation-context
                            (let [lua-desc (invoke-with-hooks component-atom rt lua-fn lua-props)
                                  desc (rt/->clj rt component-coercer lua-desc)
                                  new-child (advance-component-child child desc opts)]
                              (swap! component-atom complete-hook-triggered-advance desc new-child render-counter)))]
            (when (and (not= :delete (:mode new-state))
                       (pos? (:render-counter new-state)))
              (fx/run-later (perform-hook-triggered-render component-atom))))
          (catch LuaError e (report-error-to-script rt (rt/->clj rt coerce/to-string lua-fn) e)))))
    (catch Throwable e (error-reporting/report-exception! e))))

(defn- component-atom-watcher [_ component-atom old-component-state new-component-state]
  (when (and (not= :delete (:mode new-component-state))
             (zero? (:render-counter old-component-state))
             (pos? (:render-counter new-component-state)))
    (fx/run-later (perform-hook-triggered-render component-atom))))

(def ^:private component-atom-meta
  {`fx.component/instance #(fx.component/instance (:child @%))})

(def ^:private lua-component-lifecycle
  (reify fx.lifecycle/Lifecycle
    (create [this {:keys [rt lua-fn lua-props]} opts]
      (let [component-atom (doto (atom {:hooks []
                                        :current-hook-index 0
                                        :mode :create       ;; or :advance, :delete
                                        :rt rt
                                        :lua-fn lua-fn
                                        :lua-props lua-props
                                        :opts opts
                                        :render-counter 0}
                                       :meta component-atom-meta)
                             (add-watch this component-atom-watcher))
            lua-desc (invoke-with-hooks component-atom rt lua-fn lua-props)
            desc (rt/->clj rt component-coercer lua-desc)]
        (doto component-atom
          (swap! assoc
                 :mode :advance
                 :desc desc
                 :child (fx.lifecycle/create fx.lifecycle/dynamic desc opts)))))
    (advance [this component-atom {:keys [rt lua-fn lua-props] :as this-desc} opts]
      (let [component-state @component-atom]
        (if (and (= (:rt component-state) rt)
                 (= (:lua-fn component-state) lua-fn))
          (do
            (try
              (let [desc (if (rt/tables-shallow-eq? rt (:lua-props component-state) lua-props)
                           (:desc component-state)
                           (rt/->clj rt component-coercer (invoke-with-hooks component-atom rt lua-fn lua-props)))
                    new-child (advance-component-child (:child component-state) desc opts)]
                (swap! component-atom complete-component-advance lua-props desc new-child opts (:render-counter component-state)))
              (catch LuaError e (report-error-to-script rt (rt/->clj rt coerce/to-string lua-fn) e))
              (catch Throwable e (error-reporting/report-exception! e)))
            component-atom)
          (do (fx.lifecycle/delete this component-atom opts)
              (fx.lifecycle/create this this-desc opts)))))
    (delete [this component-atom opts]
      (remove-watch component-atom this)
      (swap! component-atom assoc :mode :delete)
      (fx.lifecycle/delete fx.lifecycle/dynamic (:child @component-atom) opts))))

(def ^:private read-only-props-coercer
  (coerce/hash-map :opt (coll/pair-map-by :name :coerce read-only-common-props)))

(def ^:private function-component-completion
  (lua-completion/make
    {:name "component"
     :type :function
     :description (str "Convert a function to a UI component.\n\nThe wrapped function may call any hooks functions (`editor.ui.use_*`), but on any function invocation, the hooks calls must be the same, and in the same order. This means that hooks should not be used inside loops and conditions or after a conditional return statement.\n\nThe following props are supported automatically:"
                       (props-doc-html read-only-common-props))
     :parameters [{:name "fn"
                   :types ["function"]
                   :doc "Component function, will receive a single table of props when called"}]
     :returnvalues [{:name "value"
                     :types ["component"]
                     :doc "the component"}]}))

(def ^:private function-component-lua-fn
  (rt/lua-fn function-component [{:keys [rt]} lua-fn]
    (DefoldVarArgFn.
      (fn create-function-component [lua-props]
        (let [read-only-props (rt/->clj rt read-only-props-coercer lua-props)]
          (-> {:fx/type lua-component-lifecycle
               :rt rt
               :lua-fn lua-fn
               :lua-props lua-props}
              (vary-meta assoc :type :component :props read-only-props)
              (rt/wrap-userdata "editor.ui.component(...)")))))))

;; endregion

;; region hooks

;; region use_state

(defn- update-hook-state!
  "For use by the hooks to update their own state, returns new hook state"
  [component-atom hook-index request-render f & args]
  (let [new-state
        (swap!
          component-atom
          (fn [current-state]
            (cond-> (apply update-in current-state [:hooks hook-index :state] f args)
                    request-render
                    (update :render-counter inc))))]
    (-> new-state :hooks (get hook-index) :state)))

(def ^:private function-or-nothing-coercer
  (coerce/one-of coerce/function coerce/to-nothing))

(defn- vectors-with-eq-lua-values? [rt as bs]
  (let [n (count as)]
    (and (= n (count bs))
         (loop [i 0]
           (cond
             (= i n) true
             (rt/eq? rt (as i) (bs i)) (recur (inc i))
             :else false)))))

(def ^:private use-state-completion
  (lua-completion/make
    {:name "use_state"
     :type :function
     :description "A hook that adds local state to the component. See `editor.ui.component` for hooks caveats and rules. If any of the arguments to `use_state` change during a component refresh (checked with `==`), the current state will be reset to the initial one."
     :parameters [{:name "init"
                   :types ["any" "function"]
                   :doc "Local state initializer, either initial data structure or function that produces the data structure"}
                  {:name "[...]"
                   :types ["...any"]
                   :doc "used when <code>init</code> is a function, the args are passed to the initializer function"}]
     :returnvalues [{:name "state"
                     :types ["any"]
                     :doc "current local state, starts with initial state, then may be changed using the returned <code>set_state</code> function"}
                    {:name "set_state"
                     :types ["function"]
                     :doc "function that changes the local state and causes the component to refresh. The function may be used in 2 ways:
                        <ul>
                          <li>to set the state to some other data structure: pass the data structure as a value</li>
                          <li>to replace the state using updater function: pass a function to <code>set_state</code> — it will be invoked with the current state, as well as with the rest of the arguments passed to <code>set_state</code> after the updater function. The state will be set to the value returned from the updater function</lia>
                        </ul>"}]
     :examples "<pre><code>local function increment(n)
  return n + 1
end

local counter_button = editor.ui.component(function(props)
  local count, set_count = editor.ui.use_state(props.count)
  return editor.ui.text_button {
    text = tostring(count),
    on_pressed = function()
      set_count(increment)
    end
  }
end)</code></pre>"}))

(def ^:private use-state-hook
  (let [lua-nil (rt/->lua nil)]
    (letfn [(lua-args->type+dependencies [rt lua-args]
              (let [first-lua-arg (or (first lua-args) lua-nil)
                    is-first-arg-lua-fn (some? (rt/->clj rt function-or-nothing-coercer first-lua-arg))]
                (if is-first-arg-lua-fn
                  (pair :function (vec lua-args))
                  (pair :value first-lua-arg))))
            (type+dependencies->lua-value [rt type+dependencies evaluation-context]
              (case (key type+dependencies)
                :value (val type+dependencies)
                :function (let [lua-fn+lua-args (val type+dependencies)]
                            (apply rt/invoke-immediate-1 rt (lua-fn+lua-args 0) (conj (subvec lua-fn+lua-args 1) evaluation-context)))))
            (eq-type+dependencies? [rt a b]
              (let [t (key a)]
                (and (= t (key b))
                     (case t
                       :value (rt/eq? rt (val a) (val b))
                       :function (vectors-with-eq-lua-values? rt (val a) (val b))))))
            (make-setter [component-atom hook-index]
              (rt/lua-fn set-or-update-state [{:keys [rt evaluation-context]} & lua-args]
                (let [type+dependencies (lua-args->type+dependencies rt lua-args)]
                  (:current
                    (case (key type+dependencies)
                      :value
                      (update-hook-state! component-atom hook-index true assoc :current (val type+dependencies))

                      :function
                      (let [lua-fn+lua-args (val type+dependencies)
                            lua-fn (lua-fn+lua-args 0)
                            lua-args (subvec lua-fn+lua-args 1)]
                        (update-hook-state! component-atom hook-index true update :current
                                            (fn [old-lua-value]
                                              (apply rt/invoke-immediate-1 rt lua-fn
                                                     (conj (into [old-lua-value] lua-args) evaluation-context))))))))))]
      (make-hook
        (:name use-state-completion)
        :create (fn create-use-hook-state [component-atom {:keys [rt current-hook-index]} evaluation-context & lua-args]
                  (let [type+dependencies (lua-args->type+dependencies rt lua-args)]
                    {:dependencies type+dependencies
                     :current (type+dependencies->lua-value rt type+dependencies evaluation-context)
                     :setter (make-setter component-atom current-hook-index)}))
        :advance (fn advance-use-hook-state [{:keys [rt]} {:keys [dependencies] :as hook-state} evaluation-context & lua-args]
                   (let [new-type+dependencies (lua-args->type+dependencies rt lua-args)]
                     (if (eq-type+dependencies? rt dependencies new-type+dependencies)
                       hook-state
                       (assoc hook-state :dependencies new-type+dependencies
                                         :current (type+dependencies->lua-value rt new-type+dependencies evaluation-context)))))
        :return (fn return-use-hook-values [hook-state]
                  (rt/->varargs (:current hook-state) (:setter hook-state)))))))

;; endregion

;; region use_memo
(def ^:private use-memo-completion
  (lua-completion/make
    {:name "use_memo"
     :type :function
     :description "A hook that caches the result of a computation between re-renders. See `editor.ui.component` for hooks caveats and rules. If any of the arguments to `use_memo` change during a component refresh (checked with `==`), the value will be recomputed."
     :parameters [{:name "compute"
                   :types ["function"]
                   :doc "function that will be used to compute the cached value"}
                  {:name "[...]"
                   :types ["...any"]
                   :doc "args to the computation function"}]
     :returnvalues [{:name "values"
                     :types ["...any"]
                     :doc "all returned values of the compute function"}]
     :examples "<pre><code>local function increment(n)
    return n + 1
end

local function make_listener(set_count)
    return function()
        set_count(increment)
    end
end

local counter_button = editor.ui.component(function(props)
    local count, set_count = editor.ui.use_state(props.count)
    local on_pressed = editor.ui.use_memo(make_listener, set_count)
    return editor.ui.text_button {
        text = tostring(count),
        on_pressed = on_pressed
    }
end)</code></pre>"}))

(def ^:private use-memo-hook
  (make-hook
    (:name use-memo-completion)
    :create (fn create-use-memo-state [_ {:keys [rt]} evaluation-context & lua-args]
              (let [deps (vec lua-args)]
                {:dependencies deps
                 :value (apply rt/invoke-immediate rt (conj deps evaluation-context))}))
    :advance (fn advance-use-memo-state [{:keys [rt]} hook-state evaluation-context & lua-args]
               (let [new-dependencies (vec lua-args)]
                 (if (vectors-with-eq-lua-values? rt new-dependencies (:dependencies hook-state))
                   hook-state
                   {:dependencies new-dependencies
                    :value (apply rt/invoke-immediate rt (conj new-dependencies evaluation-context))})))
    :return :value))

;; endregion

(defn- make-hook-lua-fn [hook]
  (DefoldVarArgFn.
    (fn hook-fn [& lua-args]
      (let [component-atom (or *current-component-atom*
                               (throw (LuaError. (format "Cannot use '%s' hook outside of component functions" (:name hook)))))
            evaluation-context (lifecycle-evaluation-context)
            {:keys [current-hook-index mode] :as current-state} @component-atom
            new-state (case mode
                        :create
                        (let [hook-state (apply (:create hook) component-atom current-state evaluation-context lua-args)]
                          (swap! component-atom (fn [component-state]
                                                  (-> component-state
                                                      (update :hooks conj {:hook hook :state hook-state})
                                                      (update :current-hook-index inc)))))

                        :advance
                        (if-let [{hook-state :state prev-hook :hook} (get (:hooks current-state) current-hook-index)]
                          (if (identical? prev-hook hook)
                            (let [new-hook-state (apply (:advance hook) current-state hook-state evaluation-context lua-args)]
                              (swap! component-atom (fn [component-state]
                                                      (-> component-state
                                                          (assoc-in [:hooks current-hook-index :state] new-hook-state)
                                                          (update :current-hook-index inc)))))
                            (throw (LuaError. (format "Expected '%s' hook, got '%s'" (:name prev-hook) (:name hook)))))
                          (throw (LuaError. (format "Didn't expect to use '%s' hook since it wasn't used before" (:name hook)))))
                        :delete
                        (throw (LuaError. (format "Cannot use '%s' hook after component is deleted" (:name hook)))))]
        ((:return hook) (-> new-state :hooks (get current-hook-index) :state))))))

;; endregion

;; endregion

(def components
  (into [] cat [layout-components
                data-presentation-components
                input-components
                dialog-components]))

(defn completions
  "Returns reducible with all ui-related completions"
  []
  (eduction
    cat
    [(eduction (map component->completion) components)
     [show-dialog-completion
      function-component-completion
      show-external-file-dialog-completion
      show-external-directory-dialog-completion
      external-file-field-completion
      use-state-completion
      use-memo-completion]]))

(defn env [project-path]
  (into {}
        cat
        [(eduction
           (mapcat
             (fn [[k vs]]
               (eduction
                 (map (coll/pair-fn #(enum-const-name k %) coerce/enum-lua-value-cache))
                 vs)))
           enums)
         (eduction (map (coll/pair-fn :name component->lua-fn)) components)
         [[(:name show-dialog-completion) lua-show-dialog-fn]
          [(:name function-component-completion) function-component-lua-fn]
          [(:name show-external-file-dialog-completion) (make-show-external-file-dialog-lua-fn project-path)]
          [(:name show-external-directory-dialog-completion) (make-show-external-directory-dialog-lua-fn project-path)]
          [(:name use-state-completion) (make-hook-lua-fn use-state-hook)]
          [(:name use-memo-completion) (make-hook-lua-fn use-memo-hook)]]]))
