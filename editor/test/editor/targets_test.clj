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

(ns editor.targets-test
  (:require [clojure.test :refer :all]
            [editor.targets :as targets]
            [util.fn :as fn]))

(def ^:private local-target
  {:name "Local"
   :url "http://127.0.0.1:8001"
   :address "127.0.0.1"
   :local-address "127.0.0.1"})

(defn- make-context
  []
  {:targets-atom (atom #{local-target})
   :log-fn (fn/make-call-logger)
   :on-targets-changed-fn (fn/make-call-logger)})

(defn- make-device-info
  [id name host port local-address]
  {:id id
   :name name
   :instance-name name
   :service-name (str name "._defold._tcp.local")
   :host host
   :address host
   :local-address local-address
   :port port
   :log-port "12345"
   :txt {"id" id
         "name" name
         "log_port" "12345"}})

(def ^:private make-local-device-info (partial make-device-info "local-id" "Local" "127.0.0.1" 8001 "127.0.0.1"))
(def ^:private make-iphone-device-info (partial make-device-info "iphone-id" "iPhone" "192.168.0.10" 8001 "127.0.0.1"))
(def ^:private make-tablet-device-info (partial make-device-info "tablet-id" "Tablet" "192.168.0.11" 8001 "127.0.0.1"))

(defn- targets-hostnames
  [targets]
  (->> targets (map :address) sort vec))

(def ^:private call-count (comp count fn/call-logger-calls))

(deftest update-targets-with-new-devices
  (let [{:keys [targets-atom
                on-targets-changed-fn] :as context} (make-context)]
    (testing "iPhone joins"
      (targets/update-targets! context [(make-local-device-info) (make-iphone-device-info)])
      (is (= ["127.0.0.1" "192.168.0.10"] (targets-hostnames @targets-atom)))
      (is (= 1 (call-count on-targets-changed-fn))))
    (testing "iPhone remains, tablet joins"
      (targets/update-targets! context [(make-local-device-info) (make-iphone-device-info) (make-tablet-device-info)])
      (is (= ["127.0.0.1" "192.168.0.10" "192.168.0.11"] (targets-hostnames @targets-atom)))
      (is (= 2 (call-count on-targets-changed-fn))))))

(deftest update-targets-with-known-devices
  (let [{:keys [targets-atom
                on-targets-changed-fn] :as context} (make-context)]
    (testing "iPhone joins, then remains"
      (targets/update-targets! context [(make-local-device-info) (make-iphone-device-info)])
      (targets/update-targets! context [(make-local-device-info) (make-iphone-device-info)])
      (is (= ["127.0.0.1" "192.168.0.10"] (targets-hostnames @targets-atom)))
      (is (= 1 (call-count on-targets-changed-fn))))))

(deftest update-targets-rejects-devices-with-missing-required-fields
  (let [{:keys [targets-atom
                on-targets-changed-fn] :as context} (make-context)
        malformed-device {:id "broken" :name "Broken" :address "192.168.0.99"}]
    (targets/update-targets! context [(make-local-device-info) malformed-device])
    (is (= ["127.0.0.1"] (targets-hostnames @targets-atom)))
    (is (= 1 (call-count on-targets-changed-fn)))))
