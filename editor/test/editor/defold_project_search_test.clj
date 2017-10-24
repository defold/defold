(ns editor.defold-project-search-test
  (:require [clojure.test :refer :all]
            [editor.defold-project-search :as project-search]
            [editor.resource :as resource]
            [integration.test-util :as test-util]
            [support.test-support :refer [with-clean-system]]
            [util.thread-util :as thread-util])
  (:import (java.util.concurrent LinkedBlockingQueue)))

(def ^:const search-project-path "test/resources/search_project")
(def ^:const timeout-ms 5000)

(defn- make-consumer [report-error!]
  (atom {:consumed [] :future nil :report-error! report-error!}))

(defn- consumer-start! [consumer-atom poll-fn!]
  (swap! consumer-atom
         (fn [consumer]
           (when (:future consumer)
             (future-cancel (:future consumer)))
           (-> consumer
               (assoc :consumed [])
               (assoc :future (future
                                (try
                                  (loop [last-response-time (System/nanoTime)]
                                    (thread-util/throw-if-interrupted!)
                                    (let [poll-time (System/nanoTime)
                                          entry (poll-fn!)]
                                      (if (some? entry)
                                        (if (not= ::project-search/done entry)
                                          (do (swap! consumer-atom update :consumed conj entry)
                                              (recur poll-time)))
                                        (if (< (- poll-time last-response-time) (* 1000000 timeout-ms))
                                          (recur last-response-time)))))
                                  (catch InterruptedException _
                                    nil)
                                  (catch Throwable error
                                    ((:report-error! consumer) error)
                                    nil)))))))
  consumer-atom)

(defn- consumer-stop! [consumer-atom]
  (swap! consumer-atom
         (fn [consumer]
           (when (:future consumer)
             (future-cancel (:future consumer)))
           consumer))
  nil)

(defn- consumer-started? [consumer-atom]
  (-> consumer-atom deref :future some?))

(defn- consumer-finished? [consumer-atom]
  (if-let [pending-future (-> consumer-atom deref :future)]
    (and (future-done? pending-future)
         (not (future-cancelled? pending-future)))
    false))

(defn- consumer-stopped? [consumer-atom]
  (if-let [pending-future (-> consumer-atom deref :future)]
    (or (future-cancelled? pending-future)
        (future-done? pending-future))
    true))

(defn- consumer-consumed [consumer-atom]
  (-> consumer-atom deref :consumed))

(defn- match-strings-by-proj-path [consumed]
  (mapv (fn [{:keys [resource matches]}]
          [(resource/proj-path resource) (mapv :match matches)])
        consumed))

(deftest mock-consumer-test
  (let [report-error! (test-util/make-call-logger)
        consumer (make-consumer report-error!)
        queue (LinkedBlockingQueue. 4)
        poll-fn #(.poll queue)]
    (is (false? (consumer-started? consumer)))
    (is (false? (consumer-finished? consumer)))
    (is (true? (consumer-stopped? consumer)))

    (consumer-start! consumer poll-fn)
    (is (true? (consumer-started? consumer)))
    (is (false? (consumer-finished? consumer)))
    (is (false? (consumer-stopped? consumer)))
    (future (.put queue ::project-search/done))
    (is (true? (test-util/block-until true? timeout-ms consumer-finished? consumer)))
    (is (true? (consumer-started? consumer)))
    (is (true? (consumer-stopped? consumer)))

    (consumer-start! consumer poll-fn)
    (consumer-stop! consumer)
    (is (true? (consumer-started? consumer)))
    (is (false? (consumer-finished? consumer)))
    (is (true? (consumer-stopped? consumer)))))

(deftest compile-find-in-files-regex-test
  (is (= "(?i)^(.*)(\\Qfoo\\E)(.*)$" (str (project-search/compile-find-in-files-regex "foo"))))
  (testing "* is handled correctly"
    (is (= "(?i)^(.*)(\\Qfoo\\E.*\\Qbar\\E)(.*)$" (str (project-search/compile-find-in-files-regex "foo*bar")))))
  (testing "other wildcard chars are quoted"
    (is (= "(?i)^(.*)(\\Qfoo\\E.*\\Qbar[]().$^\\E)(.*)$" (str (project-search/compile-find-in-files-regex "foo*bar[]().$^")))))
  (testing "case insensitive search strings"
    (let [pattern (project-search/compile-find-in-files-regex "fOoO")]
      (is (= "fooo" (first (re-matches pattern "fooo")))))))

(deftest make-file-resource-save-data-future-test
  (test-util/with-loaded-project search-project-path
    (test-util/with-ui-run-later-rebound
      (let [report-error! (test-util/make-call-logger)
            save-data-future (project-search/make-file-resource-save-data-future report-error! project)
            proj-paths (->> save-data-future deref (map :resource) (map resource/proj-path))
            contents (->> save-data-future deref (map :content))]
        (is (= ["/game.project"
                "/modules/colors.lua"
                "/scripts/actors.script"
                "/scripts/apples.script"] proj-paths))
        (is (every? string? contents))
        (is (= [] (test-util/call-logger-calls report-error!)))))))

(deftest file-searcher-results-test
  (test-util/with-loaded-project search-project-path
    (test-util/with-ui-run-later-rebound
      (let [report-error! (test-util/make-call-logger)
            consumer (make-consumer report-error!)
            start-consumer! (partial consumer-start! consumer)
            stop-consumer! consumer-stop!
            save-data-future (project-search/make-file-resource-save-data-future report-error! project)
            {:keys [start-search! abort-search!]} (project-search/make-file-searcher save-data-future start-consumer! stop-consumer! report-error!)
            perform-search! (fn [term exts]
                              (start-search! term exts)
                              (is (true? (test-util/block-until true? timeout-ms consumer-finished? consumer)))
                              (-> consumer consumer-consumed match-strings-by-proj-path))]
        (is (= [] (perform-search! nil nil)))
        (is (= [] (perform-search! "" nil)))
        (is (= [] (perform-search! nil "")))
        (is (= [["/modules/colors.lua" ["red = {255, 0, 0},"]]
                ["/scripts/apples.script" ["\"Red Delicious\","]]] (perform-search! "red" nil)))
        (is (= [["/modules/colors.lua" ["red = {255, 0, 0},"
                                        "green = {0, 255, 0},"
                                        "blue = {0, 0, 255}"]]] (perform-search! "255" nil)))
        (is (= [["/scripts/actors.script" ["\"Will Smith\""]]
                ["/scripts/apples.script" ["\"Granny Smith\""]]] (perform-search! "smith" "script")))
        (is (= [["/modules/colors.lua" ["return {"]]
                ["/scripts/actors.script" ["return {"]]
                ["/scripts/apples.script" ["return {"]]] (perform-search! "return" "lua, script")))
        (abort-search!)
        (is (true? (test-util/block-until true? timeout-ms consumer-stopped? consumer)))
        (is (= [] (test-util/call-logger-calls report-error!)))))))

(deftest file-searcher-abort-test
  (test-util/with-loaded-project search-project-path
    (test-util/with-ui-run-later-rebound
      (let [report-error! (test-util/make-call-logger)
            consumer (make-consumer report-error!)
            start-consumer! (partial consumer-start! consumer)
            stop-consumer! consumer-stop!
            save-data-future (project-search/make-file-resource-save-data-future report-error! project)
            {:keys [start-search! abort-search!]} (project-search/make-file-searcher save-data-future start-consumer! stop-consumer! report-error!)]
        (start-search! "*" nil)
        (is (true? (consumer-started? consumer)))
        (abort-search!)
        (is (true? (test-util/block-until true? timeout-ms consumer-stopped? consumer)))
        (is (= [] (test-util/call-logger-calls report-error!)))))))

(deftest file-searcher-file-extensions-test
  ;; Regular project path
  (test-util/with-loaded-project
    (test-util/with-ui-run-later-rebound
      (let [report-error! (test-util/make-call-logger)
            consumer (make-consumer report-error!)
            start-consumer! (partial consumer-start! consumer)
            stop-consumer! consumer-stop!
            save-data-future (project-search/make-file-resource-save-data-future report-error! project)
            {:keys [start-search! abort-search!]} (project-search/make-file-searcher save-data-future start-consumer! stop-consumer! report-error!)
            search-string "peaNUTbutterjellytime"
            perform-search! (fn [term exts]
                              (start-search! term exts)
                              (is (true? (test-util/block-until true? timeout-ms consumer-finished? consumer)))
                              (-> consumer consumer-consumed match-strings-by-proj-path))]
        (are [expected-count exts]
            (= expected-count (count (perform-search! search-string exts)))
          1 "g"
          1 "go"
          1 ".go"
          1 "*.go"
          2 "go,sCR"
          2 nil
          2 ""
          0 "lua"
          1 "lua,go"
          1 " lua,  go"
          1 " lua,  GO"
          1 "script")
        (abort-search!)
        (is (true? (test-util/block-until true? timeout-ms consumer-stopped? consumer)))
        (is (= [] (test-util/call-logger-calls report-error!)))))))
