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

(ns editor.lsp.jsonrpc-test
  (:require [clojure.core.async :as a :refer [<!]]
            [clojure.data.json :as json]
            [clojure.test :refer :all]
            [editor.lsp.jsonrpc :as lsp.jsonrpc]
            [support.async-support :as async-support]
            [util.fn :as fn]))

(set! *warn-on-reflection* true)

(defn- make-test-server [request-handlers]
  (let [base-sink (a/chan 1 (map #(json/read-str % :key-fn keyword)))
        base-source (a/chan 1 (map json/write-str))
        respond (fn respond [{:keys [id method params] :as in}]
                  {:pre [method]}
                  (conj {:jsonrpc "2.0"
                         :id id}
                        (try
                          [:result ((request-handlers method) params)]
                          (catch Throwable e
                            [:error {:code -32603
                                     :message (ex-message e)
                                     :data (Throwable->map e)}]))))]
    (a/go-loop []
      (when-some [v (<! base-sink)]
        (a/take! (a/thread (respond v)) #(a/put! base-source %))
        (recur)))
    [base-source base-sink]))

(defn make-jsonrpc-with-test-server [jsonrpc-request-handlers test-request-handlers]
  (let [[source sink] (make-test-server test-request-handlers)]
    (let [jsonrpc (lsp.jsonrpc/make jsonrpc-request-handlers source sink)]
      jsonrpc)))

(deftest requests-test
  (let [jsonrpc (make-jsonrpc-with-test-server {} {"initialize" fn/constantly-true
                                                   "initialize_slow" (fn [_] (Thread/sleep 1000))
                                                   "initialize_broken" (fn [_] (throw (Exception. "BROKEN")))})]
    (is (= {:result true} (async-support/eventually (lsp.jsonrpc/request! jsonrpc "initialize" 100))))
    (is (thrown-with-msg? Throwable #"timeout"
                          (lsp.jsonrpc/unwrap-response
                            (async-support/eventually
                              (lsp.jsonrpc/request! jsonrpc "initialize_slow" 100)))))
    (is (thrown-with-msg? Throwable #"BROKEN"
                          (lsp.jsonrpc/unwrap-response
                            (async-support/eventually (lsp.jsonrpc/request! jsonrpc "initialize_broken" 100)))))))