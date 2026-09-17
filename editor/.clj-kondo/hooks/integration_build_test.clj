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

(ns hooks.integration-build-test
  (:require [clj-kondo.hooks-api :as api]
            [hooks.integration-test-util :as test-util]))

(def implicit-build-results-bindings
  '#{build-artifacts build-results content-by-source content-by-target path project resource-node workspace})

(defn- value-node [sym path-node]
  (case sym
    path path-node
    content-by-source (api/map-node [])
    content-by-target (api/map-node [])
    build-results (api/map-node [])
    build-artifacts (api/vector-node [])
    (api/token-node nil)))

(defn- binding-nodes [path-node body]
  (let [body-symbols (test-util/free-symbols-in-nodes body)]
    (into []
          (mapcat (fn [sym]
                    (when (contains? body-symbols sym)
                      [(api/token-node sym)
                       (value-node sym path-node)])))
          implicit-build-results-bindings)))

(defn with-build-results [{:keys [node]}]
  (let [[_ path-node & body] (:children node)
        binding-nodes (binding-nodes path-node body)
        path-bound? (some #(= 'path (api/sexpr %)) binding-nodes)]
    {:node
     (test-util/let-node
       binding-nodes
       [(test-util/body-with-preserved-nodes (if path-bound? [] [path-node]) body)])}))
