(ns editor.analytics-test
  (:require [clojure.data.json :as json]
            [clojure.java.io :as io]
            [clojure.test :refer :all]
            [editor.analytics :as analytics]
            [editor.fs :as fs]
            [editor.system :as sys])
  (:import (java.net HttpURLConnection)))

(set! *warn-on-reflection* true)

(defonce ^:private validation-server-url "https://www.google-analytics.com/debug/collect")
(defonce ^:private mock-cid "7f208496-5029-4d4b-b530-e3c089980548")
(defonce ^:private mock-uid "63AD229B66CB4EFB")
(defonce ^:private mock-config {:cid mock-cid :uid mock-uid})
(defonce ^:private shutdown-timeout 4000)

(defonce ^:private get-temp-config-file
  (memoize (fn make-temp-config-file []
             (let [config-file (fs/create-temp-file! "defold-analytics")]
               (with-redefs [analytics/config-file (constantly config-file)]
                 (#'analytics/write-config! mock-config))
               config-file))))

(defn- get-response-code-and-append-hit-parsing-results! [hit-parsing-results-atom ^HttpURLConnection connection]
  ;; See https://developers.google.com/analytics/devguides/collection/protocol/v1/validating-hits
  ;; for a description of the validation-result JSON data format.
  (let [validation-result (with-open [stream (.getInputStream connection)
                                      reader (io/reader stream)]
                            (json/read reader))]
    (swap! hit-parsing-results-atom into (get validation-result "hitParsingResult"))
    (.getResponseCode connection)))

(deftest not-signed-in-test
  (let [hit-parsing-results (let [hit-parsing-results-atom (atom [])]
                              (with-redefs [analytics/config-file get-temp-config-file
                                            analytics/enabled? (constantly true)
                                            analytics/get-response-code! (partial get-response-code-and-append-hit-parsing-results! hit-parsing-results-atom)
                                            sys/defold-version (constantly "0.1.234")]
                                ;; Ensure user is signed out.
                                (analytics/set-uid! nil)

                                ;; Enqueue tracking events prior to starting analytics
                                ;; so that we will send them all in one batch.
                                (analytics/track-event! "test-category" "test-action")
                                (analytics/track-event! "test-category" "test-action" "test-label")
                                (analytics/track-exception! (NullPointerException.))
                                (analytics/track-screen! "test-screen-name")

                                ;; Submit all the above events in one batch to
                                ;; the validation server and await the response.
                                (analytics/start! validation-server-url false)
                                (analytics/shutdown! shutdown-timeout)
                                @hit-parsing-results-atom))]
    (is (= 4 (count hit-parsing-results)))
    (is (every? #(true? (get % "valid")) hit-parsing-results))
    (is (= ["/debug/collect?v=1&tid=UA-83690-7&cid=7f208496-5029-4d4b-b530-e3c089980548&t=event&ec=test-category&ea=test-action"
            "/debug/collect?v=1&tid=UA-83690-7&cid=7f208496-5029-4d4b-b530-e3c089980548&t=event&ec=test-category&ea=test-action&el=test-label"
            "/debug/collect?v=1&tid=UA-83690-7&cid=7f208496-5029-4d4b-b530-e3c089980548&t=exception&exd=NullPointerException"
            "/debug/collect?v=1&tid=UA-83690-7&cid=7f208496-5029-4d4b-b530-e3c089980548&t=screenview&an=defold&av=0.1.234&cd=test-screen-name"]
           (mapv #(get % "hit") hit-parsing-results)))))

(deftest signed-in-test
  (let [hit-parsing-results (let [hit-parsing-results-atom (atom [])]
                              (with-redefs [analytics/config-file get-temp-config-file
                                            analytics/enabled? (constantly true)
                                            analytics/get-response-code! (partial get-response-code-and-append-hit-parsing-results! hit-parsing-results-atom)
                                            sys/defold-version (constantly "0.1.234")]
                                ;; Ensure user is signed in.
                                (analytics/set-uid! mock-uid)

                                ;; Enqueue tracking events prior to starting analytics
                                ;; so that we will send them all in one batch.
                                (analytics/track-event! "test-category" "test-action")
                                (analytics/track-event! "test-category" "test-action" "test-label")
                                (analytics/track-exception! (NullPointerException.))
                                (analytics/track-screen! "test-screen-name")

                                ;; Submit all the above events in one batch to
                                ;; the validation server and await the response.
                                (analytics/start! validation-server-url true)
                                (analytics/shutdown! shutdown-timeout)
                                @hit-parsing-results-atom))]
    (is (= 4 (count hit-parsing-results)))
    (is (every? #(true? (get % "valid")) hit-parsing-results))
    (is (= ["/debug/collect?v=1&tid=UA-83690-7&cid=7f208496-5029-4d4b-b530-e3c089980548&uid=63AD229B66CB4EFB&t=event&ec=test-category&ea=test-action"
            "/debug/collect?v=1&tid=UA-83690-7&cid=7f208496-5029-4d4b-b530-e3c089980548&uid=63AD229B66CB4EFB&t=event&ec=test-category&ea=test-action&el=test-label"
            "/debug/collect?v=1&tid=UA-83690-7&cid=7f208496-5029-4d4b-b530-e3c089980548&uid=63AD229B66CB4EFB&t=exception&exd=NullPointerException"
            "/debug/collect?v=1&tid=UA-83690-7&cid=7f208496-5029-4d4b-b530-e3c089980548&uid=63AD229B66CB4EFB&t=screenview&an=defold&av=0.1.234&cd=test-screen-name"]
           (mapv #(get % "hit") hit-parsing-results)))))
