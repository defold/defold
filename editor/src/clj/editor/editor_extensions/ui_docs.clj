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

(ns editor.editor-extensions.ui-docs
  (:require [clojure.spec.alpha :as s]
            [clojure.string :as string]
            [editor.editor-extensions.coerce :as coerce]
            [editor.lua-completion :as lua-completion]
            [editor.util :as util]
            [util.coll :as coll]
            [util.eduction :as e]
            [util.fn :as fn])
  (:import [com.defold.editor.localization MessagePattern]))

(def message-pattern-coercer
  ;; We don't use `localization/message-pattern?` because we can't depend on
  ;; localization ns from here since this ns is used for generating docs before
  ;; bob is built, while localization ns transitively depends on bob classes
  (coerce/wrap-with-pred coerce/userdata #(instance? MessagePattern %) "is not a localization message"))

(def string-or-message-pattern-coercer
  (coerce/one-of coerce/string message-pattern-coercer))

;; region make-prop

;; Components are created with a table of props. The editor defines prop maps
;; both for runtime behavior (coercion) and autocomplete/doc generation.

(s/def :editor.editor-extensions.ui-docs.prop/name simple-keyword?)
(s/def ::coerce ifn?)
(s/def ::required boolean?)
(s/def ::types (s/coll-of string? :min-count 1 :distinct true))
(s/def ::doc string?)
(s/def ::prop
  (s/keys :req-un [:editor.editor-extensions.ui-docs.prop/name ::coerce ::required ::types ::doc]))

(defn- group-doc-types [types]
  (if (= 1 (count types))
    (types 0)
    (str "("
         (string/join " | " types)
         ")")))

(defn- infer-doc-types-from-coercer-schema [schema]
  (case (:type schema)
    :boolean ["boolean"]
    :string ["string"]
    :function ["function"]
    :integer ["integer"]
    :number ["number"]
    :table ["table"]
    :any ["any"]
    :array (when-let [item-types (infer-doc-types-from-coercer-schema (:item schema))]
             [(str (group-doc-types item-types) "[]")])
    nil))

(defn ^{:arglists '([name & {:keys [coerce required types doc]}])} make-prop
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
      (if-let [inferred-types (infer-doc-types-from-coercer-schema (coerce/schema (:coerce m)))]
        (assoc m :types inferred-types)
        m))))

;; endregion

;; region enums

;; Enums are defined separately so that the editor can define a runtime constant
;; and documentation for each enum constant.

(def enums
  {:alignment [:top-left :top :top-right :left :center :right :bottom-left :bottom :bottom-right]
   :padding [:none :small :medium :large]
   :spacing [:none :small :medium :large]
   :text-alignment [:left :center :right :justify]
   :icon [:open-resource :plus :minus :clear]
   :orientation [:vertical :horizontal]
   :color [:text :hint :override :warning :error]
   :heading-style [:h1 :h2 :h3 :h4 :h5 :h6 :dialog :form]
   :issue-severity [:warning :error]})

(def ^:private ^{:arglists '([enum-id])} get-enum-coercer
  (fn/make-case-fn (coll/pair-map-by key #(apply coerce/enum (val %)) enums)))

(def ^{:arglists '([keyword])} ->screaming-snake-case
  (fn/memoize #(string/replace (util/upper-case* (name %)) "-" "_")))

(defn- enum-doc-options [enum]
  {:pre [(contains? enums enum)]}
  (let [enum-module (->screaming-snake-case enum)]
    (map #(format "<code>editor.ui.%s.%s</code>" enum-module (->screaming-snake-case %)) (enums enum))))

(defn doc-with-ul-options [doc options]
  (str doc
       "; either:\n<ul>"
       (string/join (map #(format "<li>%s</li>" %) options))
       "</ul>"))

(defn- enum-prop [name & {:keys [enum doc] :as props}]
  (apply make-prop name (mapcat identity (cond-> (assoc props :coerce (get-enum-coercer enum)
                                                              :types ["string"])
                                                 doc
                                                 (update :doc doc-with-ul-options (enum-doc-options enum))))))

;; endregion

;; region props

(def ^:private read-only-common-props
  (let [grid-cell-span-coercer (coerce/wrap-with-pred coerce/integer pos? "is not positive")]
    [(make-prop :grow
                :coerce coerce/boolean
                :doc "determines if the component should grow to fill available space in a <code>horizontal</code> or <code>vertical</code> layout container")
     (make-prop :row_span
                :coerce grid-cell-span-coercer
                :doc "how many rows the component spans inside a grid container, must be positive. This prop is only useful for components inside a <code>grid</code> container.")
     (make-prop :column_span
                :coerce grid-cell-span-coercer
                :doc "how many columns the component spans inside a grid container, must be positive. This prop is only useful for components inside a <code>grid</code> container.")]))

(def read-only-props-coercer
  (coerce/hash-map :opt (coll/pair-map-by :name :coerce read-only-common-props)))

(def ^:private common-props
  (into [(enum-prop :alignment :enum :alignment :doc "alignment of the component content within its assigned bounds, defaults to <code>editor.ui.ALIGNMENT.TOP_LEFT</code>")]
        read-only-common-props))

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
                             "empty space between child components, defaults to <code>editor.ui.SPACING.MEDIUM</code>"
                             (concat
                               (enum-doc-options :spacing)
                               ["non-negative number, pixels"])))]
          common-props)))

(def component-coercer (coerce/wrap-with-pred coerce/userdata #(= :component (:type (meta %))) "is not a UI component"))
(def ^:private absent-coercer (coerce/wrap-with-pred coerce/to-boolean false? "is present (not nil or false)"))
(def ^:private child-coercer (coerce/one-of component-coercer absent-coercer))
(def ^:private children-coercer (coerce/vector-of child-coercer))

(def ^:private list-props
  (into [(make-prop :children
                    :coerce children-coercer
                    :types ["component[]"]
                    :doc "array of child components")]
        multi-child-layout-container-props))

(def ^:private grid-props
  (let [grid-constraints-coercer (coerce/vector-of
                                   (coerce/one-of
                                     (coerce/hash-map :opt {:grow coerce/boolean})
                                     absent-coercer))
        constraint-doc #(format "array of %s option tables, separate configuration for each %s:<dl><dt><code>grow <small>boolean</small></code></dt><dd>determines if the %s should grow to fill available space</dd></dl>"
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

(def ^:private separator-props
  (into [(enum-prop :orientation :enum :orientation :doc "separator line orientation, <code>editor.ui.ORIENTATION.VERTICAL</code> or <code>editor.ui.ORIENTATION.HORIZONTAL</code>")]
        common-props))

(def ^:private scroll-props
  (into [(make-prop :content
                    :coerce component-coercer
                    :required true
                    :types ["component"]
                    :doc "content component")]
        read-only-common-props))

(def ^:private label-without-color-specific-props
  [(make-prop :text :types ["string" "message"] :coerce string-or-message-pattern-coercer :doc "the text, either a string or a localization message")
   (enum-prop :text_alignment :enum :text-alignment :doc "text alignment within paragraph bounds")])

(def ^:private label-specific-props
  (conj label-without-color-specific-props
        (enum-prop :color :enum :color :doc "semantic color, defaults to <code>editor.ui.COLOR.TEXT</code>")))

(def ^:private icon-specific-props
  [(enum-prop :icon :enum :icon :required true :doc "predefined icon name")])

(def ^:private tooltip-prop
  (make-prop :tooltip :types ["string" "message"] :coerce string-or-message-pattern-coercer :doc "tooltip message shown on hover; either a string or a localization message"))

(def ^:private label-props
  (-> label-specific-props
      (conj tooltip-prop)
      (into common-props)))

(def ^:private typography-specific-props
  (conj label-specific-props
        (make-prop :word_wrap
                   :coerce coerce/boolean
                   :doc "determines if the lines of text are word-wrapped when they don't fit in the assigned bounds, defaults to true")))

(def ^:private paragraph-props
  (-> typography-specific-props
      (into common-props)))

(def ^:private heading-props
  (-> typography-specific-props
      (conj (enum-prop :style :enum :heading-style :doc "heading style, defaults to <code>editor.ui.HEADING_STYLE.H3</code>"))
      (into common-props)))

(def ^:private icon-props
  (into icon-specific-props common-props))

(def ^:private common-input-props
  (into [(make-prop :enabled
                    :coerce coerce/boolean
                    :doc "determines if the input component can be interacted with")]
        common-props))

(def ^:private button-specific-props
  [(make-prop :on_pressed
              :coerce coerce/function
              :doc "button press callback, will be invoked without arguments when the user presses the button")])

(def ^:private button-props
  (into [] cat [button-specific-props
                label-without-color-specific-props
                [(enum-prop :icon :enum :icon :doc "predefined icon name")]
                common-input-props]))

(def ^:private input-with-issue-props
  (into [(make-prop :issue
                    :coerce (coerce/one-of
                              (coerce/hash-map :req {:severity (coerce/enum :error :warning)
                                                     :message string-or-message-pattern-coercer})
                              absent-coercer)
                    :types ["table"]
                    :doc (str "issue related to the input; table with the following keys (all required):"
                              (lua-completion/args-doc-html
                                [{:name "severity"
                                  :types ["string"]
                                  :doc "either <code>editor.ui.ISSUE_SEVERITY.WARNING</code> or <code>editor.ui.ISSUE_SEVERITY.ERROR</code>"}
                                 {:name "message"
                                  :types ["string" "message"]
                                  :doc "issue message that will be shown in a tooltip; either a string or a localization message"}])))
         tooltip-prop]
        common-input-props))

(def ^:private check-box-specific-props
  [(make-prop :value :coerce coerce/boolean :doc "determines if the checkbox should appear checked")
   (make-prop :on_value_changed :coerce coerce/function :doc "change callback, will receive the new value")])

(def ^:private check-box-props
  (into [] cat [check-box-specific-props label-without-color-specific-props input-with-issue-props]))

(def ^:private select-box-specific-props
  [(make-prop :value :coerce coerce/untouched :doc "selected value")
   (make-prop :on_value_changed :coerce coerce/function :doc "change callback, will receive the selected value")
   (make-prop :options :coerce (coerce/vector-of coerce/untouched) :doc "array of selectable options")
   (make-prop :to_string :coerce coerce/function :doc "function that converts an item to a string (or a localization message); defaults to <code>tostring</code>")])

(def ^:private select-box-props
  (into select-box-specific-props input-with-issue-props))

(def ^:private text-field-specific-props
  [(make-prop :text :coerce coerce/string :doc "text")
   (make-prop :on_text_changed :coerce coerce/function :doc "text change callback, will receive the new text")])

(def ^:private text-field-props
  (into text-field-specific-props input-with-issue-props))

(def ^:private value-field-specific-props
  [(make-prop :value :coerce coerce/untouched :doc "value")
   (make-prop :on_value_changed :coerce coerce/function :doc "value change callback, will receive the new value")])

(def ^:private convertible-value-field-specific-props
  (into [(make-prop :to_value :coerce coerce/function :required true
                    :doc "convert the string to a value; should return the converted value or <code>nil</code> if not convertible")
         (make-prop :to_string :coerce coerce/function :required true
                    :doc "convert the value to a string (or a localization message)")]
        value-field-specific-props))

(def ^:private generic-value-field-props
  (into convertible-value-field-specific-props input-with-issue-props))

(def ^:private custom-value-field-props
  (into value-field-specific-props input-with-issue-props))

(def ^:private dialog-button-props
  [(make-prop :text
              :coerce string-or-message-pattern-coercer
              :required true
              :types ["string" "message"]
              :doc "button text, either a string or a localization message")
   (make-prop :result
              :coerce coerce/untouched
              :doc "value returned by <code>editor.ui.show_dialog(...)</code> if this button is pressed")
   (make-prop :default
              :coerce coerce/boolean
              :doc "if set, pressing <code>Enter</code> in the dialog will trigger this button")
   (make-prop :cancel
              :coerce coerce/boolean
              :doc "if set, pressing <code>Escape</code> in the dialog will trigger this button")
   (make-prop :enabled
              :coerce coerce/boolean
              :doc "determines if the button can be interacted with")])

(def ^:private dialog-props
  [(make-prop :title
              :coerce string-or-message-pattern-coercer
              :required true
              :types ["string" "message"]
              :doc "OS dialog window title, either a string or a localization message")
   (make-prop :header
              :coerce child-coercer
              :types ["component"]
              :doc "top part of the dialog, defaults to <code>editor.ui.heading({text = props.title})</code>")
   (make-prop :content
              :coerce child-coercer
              :types ["component"]
              :doc "content of the dialog")
   (make-prop :buttons
              :coerce children-coercer
              :types ["component[]"]
              :doc "array of <code>editor.ui.dialog_button(...)</code> components, footer of the dialog. Defaults to a single Close button")])

(def ^:private external-file-dialog-title-doc
  "OS window title, either a string or a localization message")

(def ^:private external-file-dialog-filters-doc
  (str "File filters, an array of filter tables, where each filter has following keys:"
       (lua-completion/args-doc-html
         [{:name "description"
           :types ["string" "message"]
           :doc "text explaining the filter, either a literal string like <code>\"Text files (*.txt)\"</code> or a localization message"}
          {:name "extensions"
           :types ["string[]"]
           :doc "array of file extension patterns, e.g. <code>\"*.txt\"</code>, <code>\"*.*\"</code> or <code>\"game.project\"</code>"}])))

(def external-file-dialog-filters-coercer
  (coerce/vector-of
    (coerce/hash-map
      :req {:description string-or-message-pattern-coercer
            :extensions (coerce/vector-of
                          coerce/string
                          :min-count 1
                          :distinct true)})
    :min-count 1))

(def ^:private external-file-field-props
  (into [(make-prop :value :coerce coerce/string :doc "file or directory path; resolved against project root if relative")
         (make-prop :on_value_changed :coerce coerce/function :doc "value change callback, will receive the absolute path of a selected file/folder or nil if the field was cleared; even though the selector dialog allows selecting only files, it's possible to receive directories and non-existent file system entries using text field input")
         (make-prop :title :types ["string" "message"] :coerce string-or-message-pattern-coercer :doc external-file-dialog-title-doc)
         (make-prop :filters :coerce external-file-dialog-filters-coercer :doc external-file-dialog-filters-doc)]
        input-with-issue-props))
;; endregion

;; region component definitions

(defn props-doc-html [props]
  (lua-completion/args-doc-html
    (map #(update % :name name) props)))

(s/def :editor.editor-extensions.ui-docs.component/name string?)
(s/def ::props (s/coll-of ::prop))
(s/def ::description string?)
(s/def ::component
  (s/keys :req-un [:editor.editor-extensions.ui-docs.component/name ::props ::description]))

(defn ^{:arglists '([name & {:keys [props description]}])} component
  "Construct a component definition map

  Args:
    name    string name of the component, should be snake_cased

  Kv-args:
    :props          required, a list of props of the component
    :description    required, markdown documentation string

  Returns a component definition map with :name, :props and :description keys"
  [name & {:as m}]
  {:post [(s/assert ::component %)]}
  (assoc m :name name))

(def horizontal-component
  (component
    "horizontal"
    :description "Layout container that places its children in a horizontal row one after another"
    :props list-props))

(def vertical-component
  (component
    "vertical"
    :description "Layout container that places its children in a vertical column one after another"
    :props list-props))

(def grid-component
  (component
    "grid"
    :description "Layout container that places its children in a 2D grid"
    :props grid-props))

(def separator-component
  (component
    "separator"
    :description "Thin line for visual content separation, by default horizontal and aligned to center"
    :props separator-props))

(def scroll-component
  (component
    "scroll"
    :description "Layout container that optionally shows scroll bars if child contents overflow the assigned bounds"
    :props scroll-props))

(def label-component
  (component
    "label"
    :description "Label intended for use with input components"
    :props label-props))

(def paragraph-component
  (component
    "paragraph"
    :description "A paragraph of text"
    :props paragraph-props))

(def heading-component
  (component
    "heading"
    :description "A text heading"
    :props heading-props))

(def icon-component
  (component
    "icon"
    :description "An icon from a predefined set"
    :props icon-props))

(def button-component
  (component
    "button"
    :props button-props
    :description "Button with a label and/or an icon"))

(def check-box-component
  (component
    "check_box"
    :props check-box-props
    :description "Check box with a label"))

(def select-box-component
  (component
    "select_box"
    :props select-box-props
    :description "Dropdown select box with an array of options"))

(def text-field-component
  (component
    "text_field"
    :props text-field-props
    :description "Single-line text field, reports changes on typing"))

(def value-field-component
  (component
    "value_field"
    :props generic-value-field-props
    :description "Input component based on a text field, reports changes on commit (<code>Enter</code> or focus loss).\n\nSee also: <code>string_field</code>, <code>number_field</code> and <code>integer_field</code>"))

(def string-field-component
  (component
    "string_field"
    :props custom-value-field-props
    :description "String input component based on a text field, reports changes on commit (<code>Enter</code> or focus loss)"))

(def integer-field-component
  (component
    "integer_field"
    :props custom-value-field-props
    :description "Integer input component based on a text field, reports changes on commit (<code>Enter</code> or focus loss)"))

(def number-field-component
  (component
    "number_field"
    :props custom-value-field-props
    :description "Number input component based on a text field, reports changes on commit (<code>Enter</code> or focus loss)"))

(def dialog-button-component
  (component
    "dialog_button"
    :props dialog-button-props
    :description "Dialog button shown in the footer of a dialog"))

(def dialog-component
  (component
    "dialog"
    :props dialog-props
    :description "Dialog component, a top-level window component that can't be used as a child of other components"))

;; endregion

;; region docs

(defn- component->script-doc [{:keys [name props description]}]
  (let [[req opt] (coll/separate-by :required props)]
    {:name name
     :type :function
     :description description
     :parameters [{:name "props"
                   :types ["table"]
                   :doc (str (when-not (coll/empty? req)
                               (str "Required props:\n"
                                    (props-doc-html req)
                                    "\n\n"))
                             "Optional props:\n"
                             (props-doc-html opt))}]
     :returnvalues [{:name "value"
                     :types ["component"]
                     :doc "UI component"}]}))

(def show-dialog-doc
  {:name "show_dialog"
   :type :function
   :description "Show a modal dialog and await a result"
   :parameters [{:name "dialog"
                 :types ["component"]
                 :doc "a component that resolves to <code>editor.ui.dialog(...)</code>"}]
   :returnvalues [{:name "value"
                   :types ["any"]
                   :doc "dialog result, the value used as a <code>result</code> prop in a <code>editor.ui.dialog_button({...})</code> selected by the user, or <code>nil</code> if the dialog was closed and there was no <code>cancel = true</code> dialog button with <code>result</code> prop set"}]})

(def function-component-doc
  {:name "component"
   :type :function
   :description (str "Convert a function to a UI component.\n\nThe wrapped function may call any hooks functions (`editor.ui.use_*`), but on any function invocation, the hooks calls must be the same, and in the same order. This means that hooks should not be used inside loops and conditions or after a conditional return statement.\n\nThe following props are supported automatically:"
                     (props-doc-html read-only-common-props))
   :parameters [{:name "fn"
                 :types ["function"]
                 :doc "function, will receive a single table of props when called"}]
   :returnvalues [{:name "value"
                   :types ["function"]
                   :doc "decorated component function that may be invoked with a props table create component"}]})

(def show-external-file-dialog-doc
  {:name "show_external_file_dialog"
   :type :function
   :description "Show a modal OS file selection dialog and await a result"
   :parameters [{:name "[opts]"
                 :types ["table"]
                 :doc (lua-completion/args-doc-html
                        [{:name "path"
                          :types ["string"]
                          :doc "initial file or directory path used by the dialog; resolved against project root if relative"}
                         {:name "title"
                          :types ["string" "message"]
                          :doc external-file-dialog-title-doc}
                         {:name "filters"
                          :types ["table[]"]
                          :doc external-file-dialog-filters-doc}])}]
   :returnvalues [{:name "value"
                   :types ["string" "nil"]
                   :doc "either absolute file path or nil if user canceled file selection"}]})

(def show-external-directory-dialog-doc
  {:name "show_external_directory_dialog"
   :type :function
   :description "Show a modal OS directory selection dialog and await a result"
   :parameters [{:name "[opts]"
                 :types ["table"]
                 :doc (lua-completion/args-doc-html
                        [{:name "path"
                          :types ["string"]
                          :doc "initial file or directory path used by the dialog; resolved against project root if relative"}
                         {:name "title"
                          :types ["string" "message"]
                          :doc external-file-dialog-title-doc}])}]
   :returnvalues [{:name "value"
                   :types ["string" "nil"]
                   :doc "either absolute directory path or nil if user canceled directory selection"}]})

(def ^:private resource-dialog-extensions-doc-string
  "if specified, restricts selectable resources in the dialog to specified file extensions; e.g. <code>{\"collection\", \"go\"}</code>")

(def ^:private resource-dialog-title-doc-string
  "dialog title, either a string or a localization message, defaults to <code>localization.message(\"dialog.select-resource.title\")</code>")

(def show-resource-dialog-doc
  {:name "show_resource_dialog"
   :type :function
   :description "Show a modal resource selection dialog and await a result"
   :parameters [{:name "[opts]"
                 :types ["table"]
                 :doc (lua-completion/args-doc-html
                        [{:name "extensions"
                          :types ["string[]"]
                          :doc resource-dialog-extensions-doc-string}
                         {:name "selection"
                          :types ["string"]
                          :doc "either <code>\"single\"</code> or <code>\"multiple\"</code>, defaults to <code>\"single\"</code>"}
                         {:name "title"
                          :types ["string" "message"]
                          :doc resource-dialog-title-doc-string}])}]
   :returnvalues [{:name "value"
                   :types ["string" "string[]" "nil"]
                   :doc "if user made no selection, returns <code>nil</code>. Otherwise, if selection mode is <code>\"single\"</code>, returns selected resource path; otherwise returns a non-empty array of selected resource paths."}]})

(def use-state-doc
  {:name "use_state"
   :type :function
   :description "A hook that adds local state to the component.\n\nSee `editor.ui.component` for hooks caveats and rules. If any of the arguments to `use_state` change during a component refresh (checked with `==`), the current state will be reset to the initial one."
   :parameters [{:name "init"
                 :types ["any" "function"]
                 :doc "local state initializer, either initial data structure or function that produces the data structure"}
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
end)</code></pre>"})

(def use-memo-doc
  {:name "use_memo"
   :type :function
   :description "A hook that caches the result of a computation between re-renders.\n\nSee `editor.ui.component` for hooks caveats and rules. If any of the arguments to `use_memo` change during a component refresh (checked with `==`), the value will be recomputed."
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
end)</code></pre>"})

(def ^:private external-file-field-doc
  (component->script-doc
    {:name "external_file_field"
     :description "Input component for selecting files from the file system"
     :props external-file-field-props}))

(def ^:private resource-field-doc
  (component->script-doc
    {:name "resource_field"
     :description "Input component for selecting project resources"
     :props (into [(make-prop :value :coerce coerce/string :doc "resource path (must start with <code>/</code>)")
                   (make-prop :on_value_changed :coerce coerce/function :doc "value change callback, will receive either resource path of a selected resource or nil when the field is cleared; even though the resource selector dialog allows filtering on resource extensions, it's possible to receive resources with other extensions and non-existent resources using text field input")
                   (make-prop :title :types ["string" "message"] :coerce string-or-message-pattern-coercer :doc resource-dialog-title-doc-string)
                   (make-prop :extensions :coerce (coerce/vector-of coerce/string :min-count 1) :doc resource-dialog-extensions-doc-string)]
                  input-with-issue-props)}))

(defn script-docs
  "Returns a vector with all ui-related script doc maps"
  []
  (let [add-ns-prefix #(str "editor.ui." %)]
    (->> (e/concat
           (e/map
             component->script-doc
             [horizontal-component
              vertical-component
              grid-component
              separator-component
              scroll-component
              label-component
              paragraph-component
              heading-component
              icon-component
              button-component
              check-box-component
              select-box-component
              ;; We don't need these components for the initial release, but we
              ;; might want to add them later on:
              #_text-field-component
              #_value-field-component
              string-field-component
              integer-field-component
              number-field-component
              dialog-button-component
              dialog-component])
           [show-dialog-doc
            function-component-doc
            external-file-field-doc
            resource-field-doc
            show-external-file-dialog-doc
            show-external-directory-dialog-doc
            show-resource-dialog-doc
            use-state-doc
            use-memo-doc])
         (e/map #(update % :name add-ns-prefix))
         vec)))
;; endregion
