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

(ns editor.editor-extensions.actions
  (:require [cljfx.api :as fx]
            [clojure.java.io :as io]
            [clojure.string :as string]
            [dynamo.graph :as g]
            [editor.defold-project :as project]
            [editor.editor-extensions.graph :as graph]
            [editor.editor-extensions.validation :as validation]
            [editor.error-reporting :as error-reporting]
            [editor.future :as future]
            [editor.lsp.async :as lsp.async]
            [editor.process :as process]
            [editor.workspace :as workspace])
  (:import [org.luaj.vm2 LuaError]))

(defmulti action->batched-executor+input (fn [action _project _evaluation-context]
                                           (:action action)))

(defn- transact! [txs _project _state]
  (let [f (future/make)
        transact (bound-fn* g/transact)]
    (fx/on-fx-thread
      (try
        (transact txs)
        (future/complete! f nil)
        (catch Exception ex (future/fail! f ex))))
    f))

(defmethod action->batched-executor+input "set" [action project evaluation-context]
  (let [node-id (graph/node-id-or-path->node-id (:node_id action) project evaluation-context)
        property (:property action)
        setter (graph/ext-value-setter node-id property project evaluation-context)]
    (if setter
      [transact! (setter (:value action))]
      (throw (LuaError.
               (format "Can't set \"%s\" property of %s"
                       property
                       (name (graph/node-id->type-keyword node-id evaluation-context))))))))

(defn- await-all-sequentially
  "Sequentially execute a collection of functions that return CompletableFutures

  Returns a CompletableFuture with the result returned by the last future. Stops
  processing on first failed CompletableFuture"
  [future-fns]
  (reduce (fn [acc future-fn]
            (future/then-async acc (fn [_] (future-fn))))
          (future/completed nil)
          future-fns))

(defn- input-stream->console [input-stream display-output type]
  (future
    (error-reporting/catch-all!
      (with-open [reader (io/reader input-stream)]
        (doseq [line (line-seq reader)]
          (display-output type line))))))

(defn- shell! [commands project state]
  (let [{:keys [can-execute? reload-resources display-output]} state
        root (lsp.async/with-auto-evaluation-context evaluation-context
               (-> project
                   (project/workspace evaluation-context)
                   (workspace/project-path evaluation-context)))]
    (-> (await-all-sequentially
          (eduction
            (map
              (fn [cmd+args]
                #(future/then
                   (can-execute? cmd+args)
                   (fn [can-execute]
                     (if can-execute
                       (let [process (doto (apply process/start! {:dir root} cmd+args)
                                       (-> process/out (input-stream->console display-output :out))
                                       (-> process/err (input-stream->console display-output :err)))
                             exit-code (process/await-exit-code process)]
                         (when-not (zero? exit-code)
                           (throw (ex-info (str "Command \""
                                                (string/join " " cmd+args)
                                                "\" exited with code "
                                                exit-code)
                                           {:cmd cmd+args
                                            :exit-code exit-code}))))
                       (throw (ex-info (str "Command \"" (string/join " " cmd+args) "\" aborted") {:cmd cmd+args})))))))
            commands))
        (future/then (fn [_] (reload-resources))))))

(defmethod action->batched-executor+input "shell" [action _ _]
  [shell! (:command action)])

(defn perform!
  "Perform a list of editor script actions

  Returns a future that will complete once all actions are executed"
  [actions project state evaluation-context]
  (validation/ensure ::validation/actions actions)
  (await-all-sequentially
    (eduction (map #(action->batched-executor+input % project evaluation-context))
              (partition-by first)
              (map (juxt ffirst #(mapv second %)))
              (map (fn [[executor inputs]]
                     #(executor inputs project state)))
              actions)))