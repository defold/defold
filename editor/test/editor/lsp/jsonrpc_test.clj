(ns editor.lsp.jsonrpc-test
  (:require [clojure.test :refer :all]
            [support.async-support :as async-support]
            [editor.lsp.jsonrpc :as lsp.jsonrpc]
            [clojure.core.async :as a :refer [<!]]
            [clojure.data.json :as json]))

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
  (let [jsonrpc (make-jsonrpc-with-test-server {} {"initialize" (constantly true)
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