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

(ns editor.code.script-intelligence
  (:require [dynamo.graph :as g]
            [editor.graph-util :as gu]
            [editor.lua :as lua]
            [schema.core :as s]))

(g/deftype ScriptCompletions {s/Str [{s/Keyword s/Any}]})

(g/defnk produce-lua-completions
  [lua-completions]
  (lua/combine-completions (apply (partial merge-with into) lua-completions)))

(g/defnode ScriptIntelligenceNode
  (input lua-completions ScriptCompletions :array :substitute gu/array-subst-remove-errors)
  (input build-errors g/Any :array :substitute gu/array-subst-remove-errors)
  (output lua-completions ScriptCompletions :cached produce-lua-completions)
  (output build-errors g/Any (gu/passthrough build-errors)))

