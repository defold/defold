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

(ns editor.debugging.variable-hover
  "Resolves and renders the value of an identifier or dotted identifier chain
  (e.g. \"self.field\") hovered in the code editor while the debugger is
  suspended, and builds a hoverable region for it. Walking a LuaStructure
  value can fetch unserialized content from the debugger, so callers run
  these functions off the JavaFX thread."
  (:require [clojure.string :as string]
            [editor.code.data :as data]
            [editor.debugging.mobdebug :as mobdebug]))

(set! *warn-on-reflection* true)

(def ^:private undefined
  "Sentinel value distinguishing a missing key from one whose value is nil."
  (Object.))

(def ^:private max-hover-lines 40)
(def ^:private max-hover-line-length 200)

(defn resolve-value
  "Resolves `expr-text` (e.g. \"self.field\") against `suspension-variables`,
  a map of the shape {:file .. :locals .. :upvalues ..} produced from the top
  stack frame of a suspended debug session. The head segment is looked up by
  string key in :locals, then :upvalues; each remaining '.'-separated segment
  is then walked with `get`, which both plain Clojure maps and
  `editor.debugging.mobdebug/LuaStructure` support.

  Returns {:value v} when the full chain resolves, where `v` may itself be
  nil (a Lua variable holding nil), or nil when any segment along the chain
  does not resolve."
  [suspension-variables expr-text]
  (let [[head & segments] (string/split expr-text #"\.")
        locals (:locals suspension-variables)
        upvalues (:upvalues suspension-variables)]
    (loop [value (get locals head (get upvalues head undefined))
           segments segments]
      (cond
        (identical? undefined value)
        nil

        (empty? segments)
        {:value value}

        (nil? value)
        nil

        :else
        (recur (get value (first segments) undefined) (next segments))))))

(defn render-value
  [expr-text value]
  (str expr-text " = " (mobdebug/lua-value->structure-string value)))

(defn truncate-hover-text
  "Caps `s` to `max-hover-lines` lines of at most `max-hover-line-length`
  characters each, appending an ellipsis where content was cut."
  [s]
  (let [lines (string/split-lines s)
        truncated-lines? (> (count lines) max-hover-lines)
        capped-lines (cond-> (into [] (take max-hover-lines) lines)
                       truncated-lines? (conj "..."))]
    (->> capped-lines
         (map (fn [line]
                (if (> (count line) max-hover-line-length)
                  (str (subs line 0 max-hover-line-length) "...")
                  line)))
         (string/join "\n"))))

(defn evaluated-region
  "Returns a hoverable :debug-variable region presenting `value` as the debug
  value of the identifier expression `expr-text` spanning `cursor-range`."
  [expr-text value cursor-range]
  (data/map->CursorRange
    {:from (:from cursor-range)
     :to (:to cursor-range)
     :type :debug-variable
     :hoverable true
     :expr-text expr-text
     :value value
     :content {:type :plaintext
               :value (truncate-hover-text (render-value expr-text value))}}))

(defn variable-hover-region
  "Returns a hoverable region for the debug value of `expr`, an identifier
  expression from `editor.code.data/identifier-expression-at-cursor`, or nil
  when the expression does not resolve or `proj-path` is not the file
  execution is suspended in."
  [suspension-variables proj-path expr]
  (when (some? suspension-variables)
    (when (= proj-path (:file suspension-variables))
      (when-some [{:keys [text cursor-range]} expr]
        (when-some [{:keys [value]} (resolve-value suspension-variables text)]
          (evaluated-region text value cursor-range))))))
