(ns editor.code.resource
  (:require [clojure.string :as string]
            [dynamo.graph :as g]
            [editor.code.data :as data]
            [editor.code.util :as util]
            [editor.graph-util :as gu]
            [editor.resource-node :as resource-node]
            [editor.workspace :as workspace]
            [schema.core :as s])
  (:import (editor.code.data CursorRange)))

(g/deftype BreakpointRows (sorted-set s/Num))
(g/deftype CursorRanges [CursorRange])
(g/deftype InvalidatedRows [Long])
(g/deftype Lines [String])

(defn- read-fn [resource]
  (util/split-lines (slurp resource)))

(defn- write-fn [lines]
  (string/join "\n" lines))

(defn- load-fn [_project self resource]
  (g/set-property self :lines (read-fn resource)))

(g/defnk produce-breakpoint-rows [regions]
  (into (sorted-set)
        (comp (filter data/breakpoint-region?)
              (map data/breakpoint-row))
        regions))

(g/defnode CodeEditorResourceNode
  (inherits resource-node/ResourceNode)

  (property cursor-ranges CursorRanges (default [data/document-start-cursor-range]) (dynamic visible (g/constantly false)))
  (property invalidated-rows InvalidatedRows (default []) (dynamic visible (g/constantly false)))
  (property lines Lines (default []) (dynamic visible (g/constantly false)))
  (property regions CursorRanges (default []) (dynamic visible (g/constantly false)))

  (output breakpoint-rows BreakpointRows :cached produce-breakpoint-rows)
  (output code g/Str :cached (g/fnk [lines] (write-fn lines)))
  (output save-value Lines (gu/passthrough lines)))

(defn register-code-resource-type [workspace & {:keys [ext node-type icon view-types view-opts tags tag-opts label] :as args}]
  (let [args (assoc args :load-fn load-fn
                         :read-fn read-fn
                         :write-fn write-fn
                         :textual? true)]
    (apply workspace/register-resource-type workspace (mapcat identity args))))
