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

(ns editor.analytics-test
  (:require [clojure.data.json :as json]
            [clojure.java.io :as io]
            [clojure.test :refer :all]
            [editor.analytics :as analytics]
            [editor.connection-properties :refer [connection-properties]]
            [editor.fs :as fs]
            [editor.system :as sys]
            [integration.test-util :as test-util]
            [service.log :as log]
            [util.fn :as fn])
  (:import [java.io FileNotFoundException]
           [java.net HttpURLConnection]))

(set! *warn-on-reflection* true)

(defonce ^:private validation-server-url (get connection-properties :analytics-validation-url))
(defonce ^:private mock-cid "7f208496-5029-4d4b-b530-e3c089980548")
(defonce ^:private send-interval 300)
(defonce ^:private shutdown-timeout 4000)
(defonce ^:private temp-config-file (fs/create-temp-file! "defold-analytics"))

(def ^:private valid-cid? #'analytics/valid-cid?)

(defn- get-config! []
  (#'analytics/read-config-from-file! temp-config-file))

(defn- with-config-fn! [config action-fn]
  (with-redefs [analytics/config-file (constantly temp-config-file)]
    (if (some? config)
      (#'analytics/write-config-to-file! config temp-config-file)
      (fs/delete-file! temp-config-file))
    (action-fn)))

(defmacro ^:private with-config! [config & body]
  `(with-config-fn! ~config (fn [] ~@body)))

(deftest with-config-test
  (with-config! {:cid mock-cid}
    (is (fs/existing-file? temp-config-file))
    (is (= {:cid mock-cid} (get-config!))))

  (with-config! nil
    (is (not (fs/existing-file? temp-config-file)))
    (is (thrown? FileNotFoundException (get-config!)))))

(defn- get-response-code-and-append-validation-messages! [validation-messages-atom ^HttpURLConnection connection]
  ;; See https://developers.google.com/analytics/devguides/collection/protocol/ga4/validating-events?client_type=gtag
  ;; for a description of the validation-result JSON data format.
  (let [validation-result (with-open [stream (.getInputStream connection)
                                      reader (io/reader stream)]
                            (json/read reader))]
    (swap! validation-messages-atom into (get validation-result "validationMessages"))
    (.getResponseCode connection)))

(deftest events-test
  ;; NOTE: This test posts to Google validation servers.
  (let [validation-messages (let [validation-messages-atom (atom [])]
                              (with-redefs [analytics/get-response-code! (partial get-response-code-and-append-validation-messages! validation-messages-atom)
                                            sys/defold-version (constantly "0.1.234")]
                                (log/without-logging
                                  (with-config! {:cid mock-cid}
                                    (analytics/start! validation-server-url test-util/localization send-interval)
                                    (analytics/track-event! "test-category" "test-action")
                                    (analytics/track-event! "test-category" "test-action" "test-label")
                                    (analytics/track-exception! (NullPointerException.))
                                    (analytics/track-screen! "test-screen-name")
                                    (analytics/shutdown! shutdown-timeout)
                                    @validation-messages-atom))))]
    (is (= 0 (count validation-messages)))))

(deftest send-error-test
  (with-redefs [sys/defold-version (constantly "0.1.234")]

    (testing "Bad response."
      (let [post-call-logger (fn/make-call-logger)]
        (with-redefs [analytics/get-response-code! (constantly 404)
                      analytics/post! post-call-logger]
          (log/without-logging
            (analytics/start! "https://not-used" test-util/localization 10)
            (analytics/track-event! "test-category" "test-action")
            (Thread/sleep 1000))

          ;; Shuts down after 5 retries.
          (is (= 5 (count (fn/call-logger-calls post-call-logger))))
          (is (not (analytics/enabled?))))))

    (testing "Exception during post."
      (let [post-call-logger (fn/make-call-logger (fn [_ _ _] (throw (ex-info "Post failed" {}))))]
        (with-redefs [analytics/get-response-code! (constantly 200)
                      analytics/post! post-call-logger]
          (log/without-logging
            (analytics/start! "https://not-used" test-util/localization 10)
            (analytics/track-event! "test-category" "test-action")
            (Thread/sleep 1000))

          ;; Shuts down after 5 retries.
          (is (= 5 (count (fn/call-logger-calls post-call-logger))))
          (is (not (analytics/enabled?))))))))
