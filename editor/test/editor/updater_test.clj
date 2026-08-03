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

(ns editor.updater-test
  (:require [clojure.java.io :as io]
            [clojure.test :refer :all]
            [editor.fs :as fs]
            [editor.prefs :as prefs]
            [editor.updater :as updater]
            [util.http-server :as http-server])
  (:import [ch.qos.logback.classic Level Logger]
           [com.dynamo.bob Platform]
           [java.io File]
           [java.util Timer]
           [org.slf4j LoggerFactory]
           [util.http_server ServerWithHandler]))

(set! *warn-on-reflection* true)

(def ^:private test-port
  23232)

(defn- error-log-level-fixture [f]
  (let [root-logger ^Logger (LoggerFactory/getLogger Logger/ROOT_LOGGER_NAME)
        level (.getLevel root-logger)]
    (.setLevel root-logger Level/ERROR)
    (f)
    (.setLevel root-logger level)))

(defn- test-urls-fixture [f]
  (with-redefs [updater/download-url
                (fn [archive-domain sha1 channel ^Platform platform]
                  (format "http://localhost:%s/archive/%s/%s/editor2/Defold-%s.zip"
                          test-port sha1 channel (.getPair platform)))

                updater/update-url
                (fn [archive-domain channel]
                  (format "http://localhost:%s/editor2/channels/%s/update-v4.json"
                          test-port channel))

                updater/release-notes-manifest-url
                (fn [archive-domain channel]
                  (format "http://localhost:%s/editor2/channels/%s/release-notes/manifest.json"
                          test-port channel))

                updater/release-notes-version-url
                (fn [archive-domain channel version]
                  (format "http://localhost:%s/editor2/channels/%s/release-notes/%s.json"
                          test-port channel version))]
    (f)))

(use-fixtures :once error-log-level-fixture test-urls-fixture)

(defn make-handler-resources [channel sha1]
  {(format "/editor2/channels/%s/update-v4.json" channel)
   (http-server/json-response {:sha1 sha1})

   (format "/archive/%s/%s/editor2/Defold-%s.zip" sha1 channel (.getPair (Platform/getHostPlatform)))
   (http-server/response 200 (io/resource "test-update.zip"))

   (format "/editor2/channels/%s/release-notes/manifest.json" channel)
   (http-server/json-response [])})

(defn- make-resource-handler [channel sha1]
  (let [resources (make-handler-resources channel sha1)]
    (fn [request]
      (get resources (:path request) http-server/not-found))))

(defn- list-files [dir]
  (->> (file-seq (io/file dir))
       (filter #(.isFile ^File %))
       (map #(.getName ^File %))
       (apply hash-set)))

(defn- start-update-server!
  ^ServerWithHandler [channel sha]
  (http-server/start! (make-resource-handler channel sha) :port test-port))

(defn- make-temp-prefs []
  (prefs/make :scopes {:global (fs/create-temp-file! "updater-test" ".editor_settings")}
              :schemas [:default]))

(defn make-updater
  ([channel editor-sha1]
   (make-updater channel editor-sha1 editor-sha1 (make-temp-prefs)))
  ([channel editor-sha1 prefs]
   (make-updater channel editor-sha1 editor-sha1 prefs))
  ([channel editor-sha1 downloaded-sha1 prefs]
   (#'updater/make-updater
     channel
     editor-sha1
     downloaded-sha1
     prefs
     (Platform/getHostPlatform)
     (io/file ".")
     (io/file "no-launcher")
     [])))

(deftest no-update-on-client-when-no-update-on-server
  (with-open [_ (start-update-server! "test" "1")]
    (let [updater (make-updater "test" "1")]
      (#'updater/check! updater)
      (is (false? (updater/can-download-update? updater)))
      (#'updater/check! updater)
      (is (false? (updater/can-download-update? updater))))))

(deftest has-update-on-client-when-has-update-on-server
  (with-open [_ (start-update-server! "test" "2")]
    (let [updater (make-updater "test" "1")]
      (#'updater/check! updater)
      (is (true? (updater/can-download-update? updater))))))

(deftest no-update-on-client-when-server-has-update-on-different-channel
  (with-open [_ (start-update-server! "alpha" "2")]
    (let [updater (make-updater "beta" "1")]
      (#'updater/check! updater)
      (is (false? (updater/can-download-update? updater))))))

(deftest can-download-and-extract-update
  (with-open [_ (start-update-server! "test" "2")]
    (let [updater (make-updater "test" "1")
          ^File update-sha1-file @#'updater/update-sha1-file
          ^File update-dir @#'updater/update-dir]
      (fs/delete-directory! update-dir)
      (fs/delete! update-sha1-file)
      (#'updater/check! updater)
      @(updater/download-and-extract! updater)
      (is (.exists update-dir))
      (is (.exists update-sha1-file))
      (is (= #{"extracted-file.txt"} (list-files update-dir)))
      (fs/delete-directory! update-dir)
      (fs/delete! update-sha1-file))))

(deftest throws-if-zip-is-missing-on-server
  (with-open [_ (start-update-server! "test" "2")]
    (let [updater (#'updater/make-updater
                    "test"
                    "1"
                    "1"
                    (make-temp-prefs)
                    Platform/WasmWeb
                    (io/file ".")
                    (io/file "no-launcher")
                    [])]
      (#'updater/check! updater)
      (is (true? (updater/can-download-update? updater)))
      (is (false? @(updater/download-and-extract! updater))))))

(deftest client-has-update-after-check-when-update-appears-on-server
  (let [updater (make-updater "test" "1")]
    (with-open [_ (start-update-server! "test" "1")]
      (#'updater/check! updater)
      (is (false? (updater/can-download-update? updater))))
    (with-open [_ (start-update-server! "test" "2")]
      (#'updater/check! updater)
      (is (true? (updater/can-download-update? updater))))))

(deftest no-new-update-is-reported-after-extracting
  (let [updater (make-updater "test" "1")]
    (with-open [_ (start-update-server! "test" "2")]
      (#'updater/check! updater)
      (is (true? (updater/can-download-update? updater)))
      @(updater/download-and-extract! updater)
      (#'updater/check! updater)
      (is (false? (updater/can-download-update? updater))))))

(deftest update-timer-performs-checks-automatically
  (let [updater (make-updater "test" "1")
        timer ^Timer (#'updater/start-timer! updater 10 10)]
    (try
      (with-open [_ (start-update-server! "test" "1")]
        (Thread/sleep 1000)
        (is (false? (updater/can-download-update? updater))))
      (with-open [_ (start-update-server! "test" "2")]
        (Thread/sleep 1000)
        (is (true? (updater/can-download-update? updater))))
      (finally
        (.cancel timer)
        (.purge timer)))))

(deftest skipped-update-persists
  (let [prefs-file (fs/create-temp-file! "updater-test" ".editor_settings")
        first-prefs (prefs/make :scopes {:global prefs-file}
                                :schemas [:default])
        first-updater (make-updater "alpha" "A" first-prefs)]
    (swap! (:state-atom first-updater) assoc :server-sha1 "B")
    (updater/skip-update! first-updater "B")
    (prefs/sync!)
    (let [second-prefs (prefs/make :scopes {:global prefs-file}
                                   :schemas [:default])
          second-updater (make-updater "alpha" "A" second-prefs)]
      (swap! (:state-atom second-updater) assoc :server-sha1 "B")
      (is (= {"alpha" "B"}
             (prefs/get second-prefs [:versioning :skipped-update-sha1s])))
      (is (false? (updater/update-advertised? second-updater))))))

(deftest skipped-updates-are-scoped-by-channel
  (let [prefs (make-temp-prefs)
        alpha-updater (make-updater "alpha" "A" prefs)
        beta-updater (make-updater "beta" "A" prefs)]
    (swap! (:state-atom alpha-updater) assoc :server-sha1 "B")
    (swap! (:state-atom beta-updater) assoc :server-sha1 "B")
    (updater/skip-update! alpha-updater "B")
    (is (= {"alpha" "B"}
           (prefs/get prefs [:versioning :skipped-update-sha1s])))
    (is (false? (updater/update-advertised? alpha-updater)))
    (is (true? (updater/update-advertised? beta-updater)))
    (updater/skip-update! beta-updater "B")
    (is (= {"alpha" "B" "beta" "B"}
           (prefs/get prefs [:versioning :skipped-update-sha1s])))
    (is (false? (updater/update-advertised? beta-updater)))))

(deftest skipped-update-remains-hidden-after-same-sha-check
  (with-open [_ (start-update-server! "test" "B")]
    (with-redefs [updater/fetch-release-notes! (constantly nil)]
      (let [updater (make-updater "test" "A")]
        (#'updater/check! updater)
        (updater/skip-update! updater "B")
        (#'updater/check! updater)
        (is (false? (updater/update-advertised? updater)))))))

(deftest new-update-is-visible-after-skipping-previous-sha
  (let [updater (make-updater "test" "A")]
    (with-redefs [updater/fetch-release-notes! (constantly nil)]
      (with-open [_ (start-update-server! "test" "B")]
        (#'updater/check! updater)
        (updater/skip-update! updater "B")
        (is (false? (updater/update-advertised? updater))))
      (with-open [_ (start-update-server! "test" "C")]
        (#'updater/check! updater)
        (is (true? (updater/update-advertised? updater)))))))

(deftest skipping-new-update-preserves-downloaded-update
  (let [prefs (make-temp-prefs)
        updater (make-updater "alpha" "A" "B" prefs)]
    (swap! (:state-atom updater) assoc :server-sha1 "C")
    (is (true? (updater/can-install-update? updater)))
    (is (true? (updater/update-advertised? updater)))
    (updater/skip-update! updater "C")
    (is (true? (updater/can-install-update? updater)))
    (is (false? (updater/update-advertised? updater)))
    ;; Skipping only withdraws the offer; the update stays downloadable.
    (is (true? (updater/can-download-update? updater)))))

(deftest version-string-test
  (are [in out] (= out (#'updater/version-string? in))
    "1.13.2" true
    "1.0"    true
    "42"     true
    "1.x"    false
    "abc"    false
    ""       false
    nil      false))

(deftest newer-version-test
  (let [newer? #'updater/newer-version?]
    (is (true? (newer? "1.13.2" "1.13.1")))
    (is (false? (newer? "1.13.1" "1.13.2")))
    (is (false? (newer? "1.13.1" "1.13.1")))
    ;; digit runs compare as numbers, not as characters
    (is (true? (newer? "1.10.0" "1.9.0")))
    ;; a shorter version is not automatically older
    (is (true? (newer? "1.13" "1.2.1")))
    (is (true? (newer? "1.13.2" "1.13")))))

(deftest versions-to-fetch-test
  (let [vtf #'updater/versions-to-fetch]
    ;; versions at or newer than current, newest first; older is dropped
    (is (= ["1.13.2" "1.13.1" "1.13.0"]
           (vtf ["1.13.0" "1.13.1" "1.13.2"] "1.13.0")))
    ;; equal version is included
    (is (= ["1.13.0"] (vtf ["1.13.0"] "1.13.0")))
    ;; nil current -> most recent N, newest first
    (is (= ["1.13.2" "1.13.1" "1.13.0"]
           (vtf ["1.13.0" "1.13.2" "1.13.1"] nil)))
    ;; caps at release-notes-range-limit (5)
    (is (= ["1.0.6" "1.0.5" "1.0.4" "1.0.3" "1.0.2"]
           (vtf ["1.0.0" "1.0.1" "1.0.2" "1.0.3" "1.0.4" "1.0.5" "1.0.6"]
                "1.0.0")))
    ;; unparseable entries are dropped
    (is (= ["1.2.0"]
           (vtf ["1.2.0" "garbage" "1.x"] "1.1.0")))
    ;; semver ordering, not lexicographic
    (is (= ["1.10.0" "1.9.0"]
           (vtf ["1.9.0" "1.10.0"] "1.8.0")))
    ;; segment count doesn't decide the order: 1.13 is newer than 1.2.1
    (is (= ["1.13" "1.2.1"]
           (vtf ["1.2.1" "1.13"] "1.1.0")))))

(deftest fetch-release-notes-walks-manifest
  ;; happy path: manifest order preserved, newest first
  (with-redefs [updater/fetch-manifest! (fn [_ _] ["1.13.2" "1.13.1" "1.13.0"])
                updater/fetch-version-notes! (fn [_ _ v] {:version v :issues []})]
    (is (= ["1.13.2" "1.13.1" "1.13.0"]
           (mapv :version (#'updater/fetch-release-notes! "d" "test"))))))

(deftest fetch-release-notes-keeps-failed-slots
  ;; a version whose file fails stays as a slot with nil notes, in order
  (with-redefs [updater/fetch-manifest! (fn [_ _] ["1.14.0" "1.13.0"])
                updater/fetch-version-notes! (fn [_ _ v]
                                               (when (= v "1.14.0")
                                                 {:version v :issues []}))]
    (let [slots (#'updater/fetch-release-notes! "d" "test")]
      (is (= ["1.14.0" "1.13.0"] (mapv :version slots)))
      (is (some? (:notes (first slots))))
      (is (nil? (:notes (second slots)))))))

(deftest fetch-release-notes-nil-when-manifest-fails
  (with-redefs [updater/fetch-manifest! (fn [_ _] nil)]
    (is (nil? (#'updater/fetch-release-notes! "d" "test")))))

(deftest check-skips-refetch-when-complete
  ;; a complete fetch arms the sha guard, so the next check does not re-download
  (with-open [_ (start-update-server! "test" "2")]
    (let [fetches (atom 0)]
      (with-redefs [updater/fetch-manifest! (fn [_ _] ["1.14.0"])
                    updater/fetch-version-notes! (fn [_ _ v]
                                                   (swap! fetches inc)
                                                   {:version v :issues []})]
        (let [updater (make-updater "test" "1")]
          (#'updater/check! updater)
          (#'updater/check! updater)
          (is (= 1 @fetches)))))))

(deftest check-retries-when-incomplete
  ;; a partial fetch leaves the guard unarmed, so the next check retries
  (with-open [_ (start-update-server! "test" "2")]
    (let [fetches (atom 0)]
      (with-redefs [updater/fetch-manifest! (fn [_ _] ["1.14.0"])
                    updater/fetch-version-notes! (fn [_ _ _]
                                                   (swap! fetches inc)
                                                   nil)]
        (let [updater (make-updater "test" "1")]
          (#'updater/check! updater)
          (#'updater/check! updater)
          (is (= 2 @fetches)))))))

(deftest release-notes-renders-missing-version-in-place
  (let [updater (make-updater "test" "1")]
    (swap! (:state-atom updater) assoc
           :server-sha1 "B"
           :release-notes-sha "B"
           :release-notes [{:version "1.14.0" :notes nil}
                           {:version "1.13.0" :notes {:version "1.13.0"
                                                      :issues []
                                                      :external-link "https://forum"}}])
    (let [{:keys [^String markdown versions]} (updater/release-notes updater)]
      (is (= ["1.14.0" "1.13.0"] versions))
      (is (re-find #"Defold 1\.14\.0" markdown))
      (is (re-find #"(?i)failed to download" markdown))
      (is (re-find #"Defold 1\.13\.0" markdown))
      ;; the missing version renders before the one we have
      (is (< (.indexOf markdown "1.14.0") (.indexOf markdown "1.13.0"))))))

(deftest release-notes-not-shown-for-superseded-update
  ;; notes fetched for one update must not render once :server-sha1 moves on
  ;; (e.g. a newer update appears but its notes fetch failed)
  (let [updater (make-updater "test" "1")]
    (swap! (:state-atom updater) assoc
           :server-sha1 "B"
           :release-notes-sha "A"
           :release-notes [{:version "1.13.0" :notes {:version "1.13.0"
                                                      :issues []
                                                      :external-link "https://forum"}}])
    (is (nil? (updater/release-notes updater)))))

(deftest check-retries-when-selection-empty
  ;; manifest present but nothing newer than the running editor -> empty slots,
  ;; which must NOT count as complete, so the next check retries
  (with-open [_ (start-update-server! "test" "2")]
    (let [fetches (atom 0)]
      (with-redefs [updater/fetch-manifest! (fn [_ _]
                                              (swap! fetches inc)
                                              [])]
        (let [updater (make-updater "test" "1")]
          (#'updater/check! updater)
          (#'updater/check! updater)
          (is (= 2 @fetches)))))))
