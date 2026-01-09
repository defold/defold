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

(ns editor.editor-extensions.actions
  (:require [cljfx.api :as fx]
            [clojure.java.io :as io]
            [clojure.string :as string]
            [dynamo.graph :as g]
            [editor.defold-project :as project]
            [editor.editor-extensions.coerce :as coerce]
            [editor.editor-extensions.graph :as graph]
            [editor.editor-extensions.runtime :as rt]
            [editor.error-reporting :as error-reporting]
            [editor.future :as future]
            [editor.lsp.async :as lsp.async]
            [editor.process :as process]
            [editor.workspace :as workspace])
  (:import [org.luaj.vm2 LuaError]))

(set! *warn-on-reflection* true)

(defmulti action->batched-executor+input (fn [action _rt _project _evaluation-context]
                                           (:action action)))

(defn- transact! [txs _project _state]
  (let [f (future/make)
        transact (bound-fn* g/transact)]
    (fx/on-fx-thread
      (try
        (transact txs)
        (future/complete! f nil)
        (catch Throwable ex (future/fail! f ex))))
    f))

(defmethod action->batched-executor+input :set [action rt project evaluation-context]
  (let [node-id (graph/node-id-or-path->node-id (:node_id action) project evaluation-context)
        property (:property action)
        setter (graph/ext-lua-value-setter node-id property rt project evaluation-context)]
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

(defn input-stream->console
  "Pipe the input stream as text lines to the console output

  Args
    input-stream       the InputStream to consume
    display-output!    2-arg function used to display extension-related output
                       to the user
    type               display-output!'s type argument, either :err or :out"
  [input-stream display-output! type]
  (future
    (error-reporting/catch-all!
      (with-open [reader (io/reader input-stream)]
        (doseq [line (line-seq reader)]
          (display-output! type line))))))

(defn- shell! [commands project state]
  (let [{:keys [reload-resources! display-output!]} state
        root (lsp.async/with-auto-evaluation-context evaluation-context
               (let [basis (:basis evaluation-context)
                     workspace (project/workspace project evaluation-context)]
                 (workspace/project-directory basis workspace)))]
    (-> (await-all-sequentially
          (eduction
            (map
              (fn [cmd+args]
                (fn start-async-shell-command! []
                  (let [process (doto (apply process/start! {:dir root} cmd+args)
                                  (-> process/out (input-stream->console display-output! :out))
                                  (-> process/err (input-stream->console display-output! :err)))]
                    (let [exit-code (process/await-exit-code process)]
                      (when-not (zero? exit-code)
                        (throw (ex-info (str "Command \""
                                             (string/join " " cmd+args)
                                             "\" exited with code "
                                             exit-code)
                                        {:cmd cmd+args
                                         :exit-code exit-code}))))))))
            commands))
        (future/then (fn [_] (reload-resources!))))))

(defmethod action->batched-executor+input :shell [action _ _ _]
  [shell! (:command action)])

(def actions-coercer
  (coerce/vector-of
    (coerce/by-key
      :action
      {:set (coerce/hash-map :req {:node_id graph/node-id-or-path-coercer
                                   :property coerce/string
                                   :value coerce/untouched})
       :shell (coerce/hash-map :req {:command (coerce/vector-of coerce/string :min-count 1)})})))

(defn perform!
  "Perform a list of editor script actions

  Returns a future that will complete once all actions are executed"
  [lua-actions project state evaluation-context]
  (let [{:keys [rt]} state]
    (await-all-sequentially
      (eduction (map #(action->batched-executor+input % rt project evaluation-context))
                (partition-by first)
                (map (juxt ffirst #(mapv second %)))
                (map (fn [[executor inputs]]
                       #(executor inputs project state)))
                (rt/->clj rt actions-coercer lua-actions)))))