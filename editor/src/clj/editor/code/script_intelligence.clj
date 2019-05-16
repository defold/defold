(ns editor.code.script-intelligence
  (:require [dynamo.graph :as g]
            [editor.graph-util :as gu]
            [schema.core :as s]))

(g/deftype ScriptCompletions {s/Str [{s/Keyword s/Any}]})

(g/defnk produce-lua-completions
  [lua-completions]
  (reduce merge {} lua-completions))

(g/defnode ScriptIntelligenceNode
  (input lua-completions ScriptCompletions :array :substitute gu/array-subst-remove-errors)
  (input build-errors g/Any :array)
  (output lua-completions ScriptCompletions :cached produce-lua-completions)
  (output build-errors g/Any (g/fnk [build-errors] build-errors)))

(defn lua-completions
  [script-intelligence]
  (g/node-value script-intelligence :lua-completions))

