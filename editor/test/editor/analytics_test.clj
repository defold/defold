(ns editor.analytics-test
  (:require [clojure.data.json :as json]
            [clojure.java.io :as io]
            [clojure.string :as string]
            [clojure.test :refer :all]
            [editor.analytics :as analytics]
            [editor.fs :as fs]
            [editor.system :as sys]
            [integration.test-util :as test-util]
            [service.log :as log])
  (:import (java.net HttpURLConnection)
           (java.nio.charset StandardCharsets)
           (java.io FileNotFoundException)))

(set! *warn-on-reflection* true)

(defonce ^:private validation-server-url "https://www.google-analytics.com/debug/collect")
(defonce ^:private mock-cid "7f208496-5029-4d4b-b530-e3c089980548")
(defonce ^:private mock-uid "63AD229B66CB4EFB")
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
    (is (= {:cid mock-cid :uid nil} (get-config!))))

  (with-config! {:cid mock-cid :uid mock-uid}
    (is (fs/existing-file? temp-config-file))
    (is (= {:cid mock-cid :uid mock-uid} (get-config!))))

  (with-config! nil
    (is (not (fs/existing-file? temp-config-file)))
    (is (thrown? FileNotFoundException (get-config!)))))

(defn- get-response-code-and-append-hit-parsing-results! [hit-parsing-results-atom ^HttpURLConnection connection]
  ;; See https://developers.google.com/analytics/devguides/collection/protocol/v1/validating-hits
  ;; for a description of the validation-result JSON data format.
  (let [validation-result (with-open [stream (.getInputStream connection)
                                      reader (io/reader stream)]
                            (json/read reader))]
    (swap! hit-parsing-results-atom into (get validation-result "hitParsingResult"))
    (.getResponseCode connection)))

(deftest not-signed-in-events-test
  ;; NOTE: This test posts to Google validation servers.
  (let [hit-parsing-results (let [hit-parsing-results-atom (atom [])]
                              (with-redefs [analytics/get-response-code! (partial get-response-code-and-append-hit-parsing-results! hit-parsing-results-atom)
                                            sys/defold-version (constantly "0.1.234")]
                                (with-config! {:cid mock-cid}
                                  (analytics/start! validation-server-url send-interval false)
                                  (analytics/track-event! "test-category" "test-action")
                                  (analytics/track-event! "test-category" "test-action" "test-label")
                                  (analytics/track-exception! (NullPointerException.))
                                  (analytics/track-screen! "test-screen-name")
                                  (analytics/shutdown! shutdown-timeout)
                                  @hit-parsing-results-atom)))]
    (is (= 4 (count hit-parsing-results)))
    (is (every? #(true? (get % "valid")) hit-parsing-results))
    (is (= ["/debug/collect?v=1&tid=UA-83690-7&cid=7f208496-5029-4d4b-b530-e3c089980548&t=event&ec=test-category&ea=test-action"
            "/debug/collect?v=1&tid=UA-83690-7&cid=7f208496-5029-4d4b-b530-e3c089980548&t=event&ec=test-category&ea=test-action&el=test-label"
            "/debug/collect?v=1&tid=UA-83690-7&cid=7f208496-5029-4d4b-b530-e3c089980548&t=exception&exd=NullPointerException"
            "/debug/collect?v=1&tid=UA-83690-7&cid=7f208496-5029-4d4b-b530-e3c089980548&t=screenview&an=defold&av=0.1.234&cd=test-screen-name"]
           (mapv #(get % "hit") hit-parsing-results)))))

(deftest signed-in-events-test
  ;; NOTE: This test posts to Google validation servers.
  (let [hit-parsing-results (let [hit-parsing-results-atom (atom [])]
                              (with-redefs [analytics/get-response-code! (partial get-response-code-and-append-hit-parsing-results! hit-parsing-results-atom)
                                            sys/defold-version (constantly "0.1.234")]
                                (with-config! {:cid mock-cid :uid mock-uid}
                                  (analytics/start! validation-server-url send-interval false)
                                  (analytics/track-event! "test-category" "test-action")
                                  (analytics/track-event! "test-category" "test-action" "test-label")
                                  (analytics/track-exception! (NullPointerException.))
                                  (analytics/track-screen! "test-screen-name")
                                  (analytics/shutdown! shutdown-timeout)
                                  @hit-parsing-results-atom)))]
    (is (= 4 (count hit-parsing-results)))
    (is (every? #(true? (get % "valid")) hit-parsing-results))
    (is (= ["/debug/collect?v=1&tid=UA-83690-7&cid=7f208496-5029-4d4b-b530-e3c089980548&uid=63AD229B66CB4EFB&t=event&ec=test-category&ea=test-action"
            "/debug/collect?v=1&tid=UA-83690-7&cid=7f208496-5029-4d4b-b530-e3c089980548&uid=63AD229B66CB4EFB&t=event&ec=test-category&ea=test-action&el=test-label"
            "/debug/collect?v=1&tid=UA-83690-7&cid=7f208496-5029-4d4b-b530-e3c089980548&uid=63AD229B66CB4EFB&t=exception&exd=NullPointerException"
            "/debug/collect?v=1&tid=UA-83690-7&cid=7f208496-5029-4d4b-b530-e3c089980548&uid=63AD229B66CB4EFB&t=screenview&an=defold&av=0.1.234&cd=test-screen-name"]
           (mapv #(get % "hit") hit-parsing-results)))))

(defn- payload->lines [^bytes payload]
  (let [text (String. payload StandardCharsets/UTF_8)]
    (string/split-lines text)))

(deftest config-effects-test
  (let [get-hits! (let [hits-atom (atom nil)]
                    (fn [invalidate-uid?]
                      (reset! hits-atom [])
                      (with-redefs [analytics/get-response-code! (constantly 200)
                                    analytics/post! (fn [_ _ payload] (swap! hits-atom into (payload->lines payload)))
                                    sys/defold-version (constantly "0.1.234")]
                        (analytics/start! "https://not-used" send-interval invalidate-uid?)
                        (analytics/track-event! "test-category" "test-action")
                        (analytics/shutdown! shutdown-timeout)
                        @hits-atom)))]

    (testing "No config file."
      (with-config! nil
        (let [hits (get-hits! false)
              config (get-config!)]
          (is (valid-cid? (:cid config)))
          (is (nil? (:uid config)))
          (is (= [(str "v=1&tid=UA-83690-7&cid=" (:cid config) "&t=event&ec=test-category&ea=test-action")]
                 hits)))))

    (testing "Corrupt config file."
      (with-redefs [analytics/config-file (constantly temp-config-file)]
        (spit temp-config-file "asdf")
        (let [hits (log/without-logging (get-hits! false))
              config (get-config!)]
          (is (valid-cid? (:cid config)))
          (is (nil? (:uid config)))
          (is (= [(str "v=1&tid=UA-83690-7&cid=" (:cid config) "&t=event&ec=test-category&ea=test-action")]
                 hits)))))

    (testing "Old-format config file with cid, but without uid."
      (with-config! {:cid mock-cid}
        (is (= [(str "v=1&tid=UA-83690-7&cid=" mock-cid "&t=event&ec=test-category&ea=test-action")]
               (get-hits! false)))))

    (testing "Config file with cid and uid."
      (with-config! {:cid mock-cid :uid mock-uid}
        (is (= [(str "v=1&tid=UA-83690-7&cid=" mock-cid "&uid=" mock-uid "&t=event&ec=test-category&ea=test-action")]
               (get-hits! false)))
        (is (= {:cid mock-cid :uid mock-uid}
               (get-config!)))))

    (testing "Config file with cid and uid, invalidate uid."
      (with-config! {:cid mock-cid :uid mock-uid}
        (is (= [(str "v=1&tid=UA-83690-7&cid=" mock-cid "&t=event&ec=test-category&ea=test-action")]
               (get-hits! true)))
        (is (= {:cid mock-cid :uid nil}
               (get-config!)))))

    (testing "Sign in"
      (with-config! {:cid mock-cid}
        (analytics/set-uid! mock-uid)
        (is (= [(str "v=1&tid=UA-83690-7&cid=" mock-cid "&uid=" mock-uid "&t=event&ec=test-category&ea=test-action")]
               (get-hits! false)))
        (is (= {:cid mock-cid :uid mock-uid}
               (get-config!)))))

    (testing "Sign out"
      (with-config! {:cid mock-cid :uid mock-uid}
        (analytics/set-uid! nil)
        (is (= [(str "v=1&tid=UA-83690-7&cid=" mock-cid "&t=event&ec=test-category&ea=test-action")]
               (get-hits! false)))
        (is (= {:cid mock-cid :uid nil}
               (get-config!)))))))

(deftest send-error-test
  (with-redefs [sys/defold-version (constantly "0.1.234")]

    (testing "Bad response."
      (let [post-call-logger (test-util/make-call-logger)]
        (with-redefs [analytics/get-response-code! (constantly 404)
                      analytics/post! post-call-logger]
          (log/without-logging
            (analytics/start! "https://not-used" 10 false)
            (analytics/track-event! "test-category" "test-action")
            (Thread/sleep 1000))

          ;; Shuts down after 5 retries.
          (is (= 5 (count (test-util/call-logger-calls post-call-logger))))
          (is (not (analytics/enabled?))))))

    (testing "Exception during post."
      (let [post-call-logger (test-util/make-call-logger (fn [_ _ _] (throw (ex-info "Post failed" {}))))]
        (with-redefs [analytics/get-response-code! (constantly 200)
                      analytics/post! post-call-logger]
          (log/without-logging
            (analytics/start! "https://not-used" 10 false)
            (analytics/track-event! "test-category" "test-action")
            (Thread/sleep 1000))

          ;; Shuts down after 5 retries.
          (is (= 5 (count (test-util/call-logger-calls post-call-logger))))
          (is (not (analytics/enabled?))))))))
