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

(ns editor.debugging.variable-tree
  "Tree items and cell rendering for suspended-debugger variable values,
  shared between the debugger's Variables pane and the code editor's debug
  hover popup. Tree items expand lazily: children of a LuaStructure are
  materialized on first expansion, which may fetch deeper values from the
  debugger."
  (:require [cljfx.fx.button :as fx.button]
            [cljfx.fx.h-box :as fx.h-box]
            [cljfx.fx.label :as fx.label]
            [cljfx.fx.region :as fx.region]
            [editor.debugging.mobdebug :as mobdebug]
            [editor.markdown :as markdown]
            [editor.ui :as ui])
  (:import [editor.debugging.mobdebug LuaStructure]
           [java.util Collection]
           [javafx.scene.control TreeItem TreeView]
           [javafx.scene.input Clipboard ClipboardContent]))

(set! *warn-on-reflection* true)

(declare make-variable-tree-item)

(defn- make-tree-item
  ^TreeItem [variable value]
  (let [tree-item (TreeItem. variable)
        children (.getChildren tree-item)]
    (when (and (instance? LuaStructure value)
               (pos? (count value)))
      (.add children (TreeItem.))
      (ui/observe-once (.expandedProperty tree-item)
                       (fn [_ _ _]
                         (.setAll children ^Collection (map make-variable-tree-item value)))))
    tree-item))

(defn make-variable-tree-item
  ^TreeItem [[name value]]
  (make-tree-item {:name name
                   :display-name (mobdebug/lua-value->identity-string name)
                   :value value
                   :display-value (mobdebug/lua-value->identity-string value)}
                  value))

(defn make-variables-tree-item
  ^TreeItem [locals upvalues]
  (let [ret (TreeItem.)
        children (.getChildren ret)]
    (.setAll children ^Collection (map make-variable-tree-item (concat locals upvalues)))
    ret))

(defn make-expression-tree-item
  "Tree item rooting the value of an evaluated expression (e.g. a hovered
  \"self.field\"), labeled with the expression text itself."
  ^TreeItem [expr-text value]
  (make-tree-item {:name expr-text
                   :display-name expr-text
                   :value value
                   :display-value (mobdebug/lua-value->identity-string value)}
                  value))

(defn expandable-value?
  "True when `value` is a Lua table with at least one entry."
  [value]
  (and (instance? LuaStructure value)
       (pos? (count ^LuaStructure value))))

(defn describe-variables-cell [{:keys [display-name display-value]}]
  {:graphic {:fx/type fx.h-box/lifecycle
             :children [{:fx/type fx.label/lifecycle :text display-name}
                        {:fx/type fx.label/lifecycle :text " = " :style-class ["label" "equals"]}
                        {:fx/type fx.label/lifecycle :text display-value}]}})

(defn- copy-to-clipboard! [text]
  (.setContent (Clipboard/getSystemClipboard)
               (doto (ClipboardContent.)
                 (.putString ^String text))))

(defn describe-hover-variables-cell
  "Like `describe-variables-cell`, with a row-hover Copy button that puts the
  row's value (nested structure for tables) on the clipboard."
  [{:keys [display-name display-value value]}]
  {:graphic {:fx/type fx.h-box/lifecycle
             :alignment :center-left
             :children [{:fx/type fx.label/lifecycle :text display-name}
                        {:fx/type fx.label/lifecycle :text " = " :style-class ["label" "equals"]}
                        {:fx/type fx.label/lifecycle :text display-value}
                        {:fx/type fx.region/lifecycle :h-box/hgrow :always}
                        {:fx/type fx.button/lifecycle
                         :style-class ["md-code-block-button" "debug-hover-copy-button"]
                         :graphic markdown/copy-icon
                         :on-action (fn [_]
                                      (copy-to-clipboard!
                                        (mobdebug/lua-value->structure-string value)))}]}})

(def ^:private hover-tree-cell-height 24.0)

(def ^:private hover-tree-insets-height
  "Vertical space the TreeView adds around its rows."
  4.0)

(def ^:private hover-tree-max-height 600.0)

(def ^:private hover-tree-max-width 640.0)

(def ^:private hover-tree-mono-char-width
  "Average glyph advance of $default-font-mono at $md-font-size
  (base/_typography.scss). Recalibrate if either changes."
  8.2)

(def ^:private hover-tree-row-extra-width
  "Horizontal space a row needs beyond its text: disclosure arrow, one level
  of child indentation, the copy button, and view insets."
  96.0)

(defn- variable-row-text-length
  [[name value]]
  (+ (count (mobdebug/lua-value->identity-string name))
     3 ; " = "
     (count (mobdebug/lua-value->identity-string value))))

(defn make-expression-tree-view
  "A TreeView presenting `value` rooted at `expr-text` for the code editor's
  hover popup: at least `min-width` wide, sized to show the whole first level
  of the table, capped by `hover-tree-max-width`/`-height` and by
  `available-height`, the screen space the popup can grow into."
  ^TreeView [expr-text value {:keys [^double min-width ^double available-height]}]
  (let [row-count (inc (count value))
        content-height (+ hover-tree-insets-height (* hover-tree-cell-height row-count))
        height (min content-height hover-tree-max-height available-height)
        root-text-length (+ (count expr-text) 3 (count (mobdebug/lua-value->identity-string value)))
        max-text-length (transduce (map variable-row-text-length) max root-text-length (seq value))
        content-width (+ hover-tree-row-extra-width (* hover-tree-mono-char-width max-text-length))
        width (-> content-width (max min-width) (min hover-tree-max-width))]
    (doto (TreeView.)
      (.setId "debug-hover-variables")
      (ui/customize-tree-view! {:double-click-expand true})
      (.setShowRoot true)
      (.setFixedCellSize hover-tree-cell-height)
      (.setRoot (doto (make-expression-tree-item expr-text value)
                  (.setExpanded true)))
      (.setPrefWidth width)
      (.setPrefHeight height))))
