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
            [internal.util :as util]
            [schema.core :as s]
            [util.coll :as coll]
            [util.eduction :as e]))

(g/deftype ScriptCompletions {s/Str [{s/Keyword s/Any}]})
(g/deftype LuaApiContextByModuleProjPath {s/Str (s/enum :editor :runtime)})
(g/deftype RequiredModuleInfo [(s/one s/Int "node-id")
                               (s/one s/Str "proj-path")
                               (s/one [s/Str] "required-module-proj-paths")
                               (s/one (s/maybe (s/enum :editor :runtime)) "lua-api-context")])
(g/deftype RequiringResourceInfosByModuleProjPath
  {s/Str [[(s/one s/Str "proj-path")
           (s/one s/Int "node-id")]]})

(g/defnk produce-lua-completions
  [lua-completions]
  (lua/combine-completions (apply (partial merge-with into) lua-completions)))

(g/defnk produce-lua-api-context-by-module-proj-path [required-module-infos]
  (let [required-module-proj-paths-by-requiring-proj-path
        (reduce (fn [result [_node-id proj-path required-module-proj-paths _lua-api-context]]
                  (update result proj-path (fnil into #{}) required-module-proj-paths))
                {}
                required-module-infos)
        initial-pending
        (into []
              (mapcat (fn [[_node-id _proj-path required-module-proj-paths lua-api-context]]
                        (when lua-api-context
                          (mapv #(coll/pair % lua-api-context) required-module-proj-paths))))
              required-module-infos)]
    (loop [pending initial-pending
           visited #{}
           contexts-by-module-proj-path {}]
      (if (coll/empty? pending)
        (into {}
              (map (fn [[module-proj-path contexts]]
                     (coll/pair module-proj-path
                                (if (contains? contexts :runtime)
                                  :runtime
                                  :editor))))
              contexts-by-module-proj-path)
        (let [[module-proj-path lua-api-context :as visit] (peek pending)
              pending (pop pending)]
          (if (contains? visited visit)
            (recur pending visited contexts-by-module-proj-path)
            (recur (into pending
                         (map #(coll/pair % lua-api-context))
                         (required-module-proj-paths-by-requiring-proj-path module-proj-path))
                   (conj visited visit)
                   (update contexts-by-module-proj-path module-proj-path (fnil conj #{}) lua-api-context))))))))

(g/defnode ScriptIntelligenceNode
  (input lua-completions ScriptCompletions :array :substitute gu/array-subst-remove-errors)
  (input required-module-infos RequiredModuleInfo :array :substitute gu/array-subst-remove-errors)
  (input build-errors g/Any :array :substitute gu/array-subst-remove-errors)
  (output lua-completions ScriptCompletions :cached produce-lua-completions)
  (output lua-api-context-by-module-proj-path LuaApiContextByModuleProjPath :cached produce-lua-api-context-by-module-proj-path)
  (output requiring-resource-infos-by-module-proj-path RequiringResourceInfosByModuleProjPath :cached
          (g/fnk [required-module-infos]
            (->> required-module-infos
                 (e/mapcat (fn [[node-id proj-path required-module-proj-paths _lua-api-context]]
                             (e/map #(coll/pair % [proj-path node-id])
                                    required-module-proj-paths)))
                 (util/group-into {} [] key val))))
  (output build-errors g/Any (gu/passthrough build-errors)))
