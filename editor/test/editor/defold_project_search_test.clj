(ns editor.defold-project-search-test
  (:require [clojure.test :refer :all]
            [editor.defold-project-search :as project-search]
            [editor.resource :as resource]
            [integration.test-util :as test-util]
            [support.test-support :refer [with-clean-system]]
            [util.thread-util :as thread-util]))

(def project-path "test/resources/search_project")

(deftest comple-find-in-files-regex-test
  (is (= "(?i)^(.*)(foo)(.*)$" (str (project-search/compile-find-in-files-regex "foo"))))
  (testing "* is handled correctly"
    (is (= "(?i)^(.*)(foo.*bar)(.*)$" (str (project-search/compile-find-in-files-regex "foo*bar")))))
  (testing "other wildcard chars are stripped"
    (is (= "(?i)^(.*)(foo.*bar)(.*)$" (str (project-search/compile-find-in-files-regex "foo*bar[]().$^")))))
  (testing "case insensitive search strings"
    (let [pattern (project-search/compile-find-in-files-regex "fOoO")]
      (is (= "fooo" (first (re-matches pattern "fooo")))))))

(defn- make-file-resource-save-data-future [report-error! world]
  (let [workspace (test-util/setup-workspace! world project-path)
        project (test-util/setup-project! workspace)]
    (project-search/make-file-resource-save-data-future report-error! project)))

(deftest make-file-resource-save-data-future-test
  (with-clean-system
    (let [report-error! (test-util/make-call-logger)
          save-data-future (make-file-resource-save-data-future report-error! world)
          proj-paths (->> save-data-future deref (map :resource) (map resource/proj-path))
          contents (->> save-data-future deref (map :content))]
      (is (= ["/game.project"
              "/modules/colors.lua"
              "/scripts/actors.script"
              "/scripts/apples.script"] proj-paths))
      (is (every? string? contents))
      (is (= [] (test-util/call-logger-calls report-error!))))))

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
                                        (do (swap! consumer-atom update :consumed conj entry)
                                            (recur poll-time))
                                        (if (< (- poll-time last-response-time) 100000000)
                                          (recur last-response-time)))))
                                  (catch InterruptedException _
                                    nil)
                                  (catch Throwable error
                                    ((:report-error! consumer) error)
                                    nil)))))))
  nil)

(defn- consumer-stop! [consumer-atom]
  (swap! consumer-atom
         (fn [consumer]
           (when (:future consumer)
             (future-cancel (:future consumer)))
           (assoc consumer :future nil)))
  nil)

(defn- consumer-started? [consumer-atom]
  (-> consumer-atom deref :future some?))

(defn- consumer-finished? [consumer-atom]
  (if-let [pending-future (-> consumer-atom deref :future)]
    (future-done? pending-future)
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

(deftest file-searcher-results-test
  (with-clean-system
    (let [report-error! (test-util/make-call-logger)
          consumer (make-consumer report-error!)
          start-consumer! (partial consumer-start! consumer)
          stop-consumer! (partial consumer-stop! consumer)
          save-data-future (make-file-resource-save-data-future report-error! world)
          {:keys [start-search! abort-search!]} (project-search/make-file-searcher save-data-future start-consumer! stop-consumer! report-error!)
          perform-search! (fn [term exts]
                            (start-search! term exts)
                            (test-util/block-until true? 1000 consumer-finished? consumer)
                            (-> consumer consumer-consumed match-strings-by-proj-path))]
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
      (is (true? (test-util/block-until true? 1000 consumer-stopped? consumer)))
      (is (= [] (test-util/call-logger-calls report-error!))))))

(deftest file-searcher-abort-test
  (with-clean-system
    (let [report-error! (test-util/make-call-logger)
          consumer (make-consumer report-error!)
          start-consumer! (partial consumer-start! consumer)
          stop-consumer! (partial consumer-stop! consumer)
          save-data-future (make-file-resource-save-data-future report-error! world)
          {:keys [start-search! abort-search!]} (project-search/make-file-searcher save-data-future start-consumer! stop-consumer! report-error!)]
      (start-search! "*" nil)
      (is (true? (consumer-started? consumer)))
      (abort-search!)
      (is (true? (test-util/block-until true? 1000 consumer-stopped? consumer)))
      (is (= [] (test-util/call-logger-calls report-error!))))))
