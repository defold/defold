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

(ns hooks.support-dap-util
  (:require [clj-kondo.hooks-api :as api]
            [hooks.integration-test-util :as test-util]))

(defn- with-implicit-bindings [setup bindings body]
  (let [body-symbols (test-util/free-symbols-in-nodes body)
        binding-nodes (into []
                            (comp (filter (fn [[name _]] (contains? body-symbols name)))
                                  (mapcat (fn [[name init]] [(api/token-node name) init])))
                            bindings)]
    {:node
     (test-util/body-with-preserved-nodes
       setup
       [(test-util/let-node binding-nodes body)])}))

(defn with-server [{:keys [node]}]
  (let [[_ handler & body] (:children node)]
    (with-implicit-bindings
      [handler]
      {'port (api/token-node 0)
       'requests (api/list-node [(api/token-node 'atom) (api/vector-node [])])}
      body)))

(defn with-adapter [{:keys [node]}]
  (let [[_ callbacks handler & body] (:children node)]
    (with-implicit-bindings
      [callbacks handler]
      {'port (api/token-node 0)
       'requests (api/list-node [(api/token-node 'atom) (api/vector-node [])])
       'session (api/list-node [(api/token-node 'atom) (api/map-node [])])
       'events (api/list-node [(api/token-node 'java.util.concurrent.LinkedBlockingQueue.)])}
      body)))
