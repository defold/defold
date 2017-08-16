(ns editor.code.resource
  (:require [clojure.string :as string]
            [dynamo.graph :as g]
            [editor.code.data :as data]
            [editor.graph-util :as gu]
            [editor.resource-node :as resource-node]
            [editor.workspace :as workspace])
  (:import (clojure.lang IPersistentVector)))

(g/deftype CursorRanges IPersistentVector)
(g/deftype Lines IPersistentVector)
(g/deftype SyntaxInfo IPersistentVector)

(defn- read-fn [resource]
  (string/split-lines (slurp resource)))

(defn- write-fn [lines]
  (string/join "\n" lines))

(defn- load-fn [_project self resource]
  (g/set-property self :lines (read-fn resource)))

(g/defnode CodeEditorResourceNode
  (inherits resource-node/ResourceNode)

  (property lines Lines (default []) (dynamic visible (g/constantly false)))
  (property cursor-ranges CursorRanges (default [data/default-cursor-range]) (dynamic visible (g/constantly false)))
  (property syntax-info SyntaxInfo (default []) (dynamic visible (g/constantly false)))

  (output code g/Str :cached (g/fnk [lines] (write-fn lines)))
  (output save-value g/Str (gu/passthrough code)))

(defn register-code-resource-type [workspace & {:keys [ext node-type icon view-types view-opts tags tag-opts label] :as args}]
  (let [args (assoc args :load-fn load-fn
                         :read-fn read-fn
                         :write-fn write-fn
                         :textual? true)]
    (apply workspace/register-resource-type workspace (mapcat identity args))))
