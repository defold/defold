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

(ns hooks.editor-debugging-variables-test
  (:require [clj-kondo.hooks-api :as api]
            [hooks.integration-test-util :as test-util]))

(defn with-view [{:keys [node]}]
  (let [[_ root-variables child-variables & body] (:children node)
        body-symbols (test-util/free-symbols-in-nodes body)]
    {:node
     (test-util/body-with-preserved-nodes
       [root-variables child-variables]
       [(api/list-node
          (list* (api/token-node 'fn)
                 (api/vector-node (into []
                                        (comp (filter #(contains? body-symbols %))
                                              (map api/token-node))
                                        '[view session events]))
                 body))])}))
