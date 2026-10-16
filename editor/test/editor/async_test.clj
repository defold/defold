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

(ns editor.async-test
  (:require [clojure.test :refer :all]
            [dynamo.graph :as g]
            [editor.async :as ea]
            [editor.ui :as ui]
            [support.test-support :refer [with-clean-system]]))

(set! *warn-on-reflection* true)

(g/defnode AsyncTestNode
  (property value g/Int (default 0)))

;; Implicit graph reads must reject background materialization commits just like
;; explicit evaluation-context merges, including the convenience node-value
;; variants.
(deftest implicit-background-commit-rejected-test
  (doseq [read-value [g/node-value g/valid-node-value g/maybe-node-value]]
    (with-clean-system {:on-system-thread? ui/on-ui-thread?}
      (let [shell (ui/run-now
                    (let [[shell] (g/take-node-ids 1)]
                      (g/transact
                        (g/add-node
                          (g/construct-shell AsyncTestNode {:_node-id shell}
                            (fn materialize-node [self _evaluation-context]
                              (g/set-property self :value 7)))))
                      shell))
            basis (g/now)
            error @(future
                     (try
                       (read-value shell :value)
                       (catch AssertionError error
                         error)))]
        (is (instance? AssertionError error))
        (is (identical? basis (g/now)))))))

;; Background materializations must only be committed on the system thread,
;; both when reusing the evaluated basis and when replaying over a newer basis.
(deftest background-commit-rejected-test
  (doseq [replay [false true]]
    (testing (str "replay: " replay)
      (with-clean-system {:on-system-thread? ui/on-ui-thread?}
        (let [[shell other]
              (ui/run-now
                (let [[shell] (g/take-node-ids 1)]
                  (g/transact
                    (g/add-node
                      (g/construct-shell AsyncTestNode {:_node-id shell}
                        (fn materialize-node [self _evaluation-context]
                          (g/set-property self :value 7)))))
                  [shell (g/make-node! AsyncTestNode)]))

              evaluation-context
              @(future
                 (let [evaluation-context (g/make-evaluation-context)]
                   (g/node-value shell :value evaluation-context)
                   evaluation-context))]
          (when replay
            (ui/run-now (g/set-property! other :value 9)))
          (let [basis (g/now)
                error @(future
                         (try
                           (g/update-system-from-evaluation-context! evaluation-context)
                           (catch AssertionError error
                             error)))]
            (is (instance? AssertionError error))
            (is (some->> error
                         ex-message
                         (re-find #"Graph changes must be committed on the system thread")))
            (is (identical? basis (g/now))))
          (ui/run-now
            (g/update-system-from-evaluation-context! evaluation-context)
            (is (= 7 (g/node-value shell :value)))
            (is (= (if replay 9 0) (g/node-value other :value)))))))))

;; Async scopes evaluate off the UI thread without blocking on its queue, then
;; merge on the UI thread while preserving edits made after their snapshot.
(deftest background-evaluation-merges-on-ui-thread-test
  (doseq [scope [:with-auto-evaluation-context :let-ec]
          replay [false true]]
    (testing (str scope ", replay: " replay)
      (with-clean-system {:on-system-thread? ui/on-ui-thread?}
        (let [materialization-count (atom 0)
              [shell other]
              (ui/run-now
                (let [[shell] (g/take-node-ids 1)]
                  (g/transact
                    (g/add-node
                      (g/construct-shell AsyncTestNode {:_node-id shell}
                        (fn materialize-node [self _evaluation-context]
                          (swap! materialization-count inc)
                          (g/set-property self :value 7)))))
                  [shell (g/make-node! AsyncTestNode)]))]
          (ui/run-now
            (let [basis (g/now)
                  result (deref (future
                                  (case scope
                                    :with-auto-evaluation-context
                                    (ea/with-auto-evaluation-context evaluation-context
                                      [(g/node-value shell :value evaluation-context)
                                       (ui/on-ui-thread?)])

                                    :let-ec
                                    (ea/let-ec [{:keys [value]} {:value (g/node-value shell :value evaluation-context)}
                                                [value on-ui-thread] [value (ui/on-ui-thread?)]]
                                      [value on-ui-thread])))
                                10000
                                ::timed-out)]
              (is (= [7 false] result))
              (is (identical? basis (g/now)))
              (when replay
                (g/set-property! other :value 9))))
          ;; The merge was enqueued before the worker completed. This action
          ;; follows it in the UI queue, without relying on a timer or sleep.
          (ui/run-now
            (is (= 7 (g/raw-property-value (g/now) shell :value)))
            (is (= (if replay 9 0) (g/raw-property-value (g/now) other :value)))
            (is (= 1 @materialization-count))))))))
