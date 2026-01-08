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

(ns editor.code.script-annotations
  (:require [clojure.java.io :as io]
            [dynamo.graph :as g]
            [editor.code.data :as data]
            [editor.fs :as fs]
            [editor.graph-util :as gu]
            [editor.resource :as resource]
            [editor.types :as types]
            [schema.core :as s]
            [util.coll :as coll]
            [util.eduction :as e])
  (:import [java.io IOException]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(g/deftype ResourceWithLines
  {:resource (s/protocol resource/Resource)
   :lines types/TLines})

(g/defnk produce-sync-hash [_node-id root script-annotations]
  (try
    (let [root-path (fs/path root ".internal" "lua-annotations")
          path->mtime+lines (coll/pair-map-by
                              #(.normalize (.resolve root-path (fs/without-leading-slash (resource/proj-path (:resource %)))))
                              (coll/pair-fn #(fs/path-last-modified-time (fs/path (io/as-file (:resource %))))
                                            :lines)
                              script-annotations)]
      ;; Remove files that are no longer relevant
      (when (fs/path-exists? root-path)
        (->> root-path (fs/path-walker) (e/remove path->mtime+lines) (run! fs/delete-path-file!)))
      ;; Write changed files
      (run!
        (fn [[path [mtime lines]]]
          (when-not (and (fs/path-exists? path)
                         (= mtime (fs/path-last-modified-time path)))
            (fs/create-path-parent-directories! path)
            (with-open [writer (io/writer path)]
              (.write writer "---@meta\n")
              (io/copy (data/lines-reader lines) writer))
            (fs/set-path-last-modified-time! path mtime)))
        path->mtime+lines)
      (hash path->mtime+lines))
    (catch IOException e
      (g/->error _node-id :sync-hash :fatal nil (.getMessage e)))))

(g/defnode ScriptAnnotations
  (input root g/Str)
  (input script-annotations ResourceWithLines :array :substitute gu/array-subst-remove-errors)
  (output sync-hash g/Int :cached produce-sync-hash))

(defn sync-hash [script-annotations evaluation-context]
  (g/node-value script-annotations :sync-hash evaluation-context))
