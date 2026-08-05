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
            [editor.system :as system]
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
                (fn [_archive-domain channel]
                  (format "http://localhost:%s/editor2/channels/%s/release-notes/manifest.json"
                          test-port channel))

                updater/release-notes-version-url
                (fn [_archive-domain channel version]
                  (format "http://localhost:%s/editor2/channels/%s/release-notes/%s.json"
                          test-port channel version))]
    (f)))

(defn- test-support-dir-fixture [f]
  (let [^File temp-updates-dir (fs/create-temp-directory! "updater-test-updates")]
    (with-redefs [updater/updates-dir temp-updates-dir]
      (f))
    (fs/delete-directory! temp-updates-dir)))

(use-fixtures :once error-log-level-fixture test-urls-fixture test-support-dir-fixture)

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

(defn- make-multi-channel-resource-handler [channel->sha1]
  (let [resources (apply merge (map (fn [[channel sha1]] (make-handler-resources channel sha1))
                                     channel->sha1))]
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

(defn- start-multi-channel-update-server!
  ^ServerWithHandler [channel->sha1]
  (http-server/start! (make-multi-channel-resource-handler channel->sha1) :port test-port))

(defn make-updater
  ([channel editor-sha1]
   (make-updater channel editor-sha1 editor-sha1))
  ([channel editor-sha1 downloaded-sha1]
   (#'updater/make-updater
     channel
     editor-sha1
     downloaded-sha1
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
          ^File update-sha1-file (:update-sha1-file updater)
          ^File update-dir (:update-dir updater)]
      (fs/delete-directory! update-dir)
      (fs/delete! update-sha1-file)
      (#'updater/check! updater)
      @(updater/download-and-extract! updater)
      (is (.exists update-dir))
      (is (.exists update-sha1-file))
      (is (= #{"extracted-file.txt"} (list-files update-dir)))
      (fs/delete-directory! update-dir)
      (fs/delete! update-sha1-file))))

(deftest channel-downloads-do-not-clobber-each-other
  (with-open [_ (start-multi-channel-update-server! {"alpha" "A2" "beta" "B2"})]
    (let [alpha-install-dir (fs/create-temp-directory! "updater-test-install")
          alpha-updater (assoc (make-updater "alpha" "A1" nil)
                          :install-dir alpha-install-dir)
          beta-updater (make-updater "beta" "B1" nil)
          ^File alpha-dir (:update-dir alpha-updater)
          ^File alpha-sha1-file (:update-sha1-file alpha-updater)
          ^File beta-dir (:update-dir beta-updater)
          ^File beta-sha1-file (:update-sha1-file beta-updater)]
      (fs/delete-directory! alpha-dir)
      (fs/delete! alpha-sha1-file)
      (fs/delete-directory! beta-dir)
      (fs/delete! beta-sha1-file)
      (try
        (#'updater/check! alpha-updater)
        (#'updater/check! beta-updater)
        (is (true? (updater/can-download-update? alpha-updater)))
        (is (true? (updater/can-download-update? beta-updater)))

        @(updater/download-and-extract! alpha-updater)
        (is (true? (updater/can-install-update? alpha-updater)))
        (is (false? (updater/can-install-update? beta-updater)))
        (is (.exists alpha-dir))
        (is (.exists alpha-sha1-file))
        (is (= "A2" (slurp alpha-sha1-file)))
        (is (not (.exists beta-dir)))
        (is (not (.exists beta-sha1-file)))

        @(updater/download-and-extract! beta-updater)
        (is (true? (updater/can-install-update? beta-updater)))
        (is (.exists beta-dir))
        (is (.exists beta-sha1-file))
        (is (= "B2" (slurp beta-sha1-file)))

        ;; downloading beta didn't touch alpha's already-extracted bundle
        (is (.exists alpha-dir))
        (is (= "A2" (slurp alpha-sha1-file)))
        (is (= #{"extracted-file.txt"} (list-files alpha-dir)))
        (is (= #{"extracted-file.txt"} (list-files beta-dir)))

        (updater/install! alpha-updater)
        (is (= #{"extracted-file.txt"} (list-files alpha-install-dir)))
        (is (false? (updater/can-install-update? alpha-updater)))
        (is (not (.exists alpha-dir)))
        (is (not (.exists alpha-sha1-file)))

        ;; installing alpha didn't touch beta's still-pending download
        (is (true? (updater/can-install-update? beta-updater)))
        (is (.exists beta-dir))
        (is (.exists beta-sha1-file))
        (is (= "B2" (slurp beta-sha1-file)))
        (finally
          (fs/delete-directory! alpha-dir)
          (fs/delete! alpha-sha1-file)
          (fs/delete-directory! beta-dir)
          (fs/delete! beta-sha1-file)
          (fs/delete-directory! alpha-install-dir))))))

(deftest throws-if-zip-is-missing-on-server
  (with-open [_ (start-update-server! "test" "2")]
    (let [updater (#'updater/make-updater
                    "test"
                    "1"
                    "1"
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

(deftest fetch-release-notes-keeps-failed-entries
  ;; a version whose file fails stays as an entry with nil notes, in order
  (with-redefs [updater/fetch-manifest! (fn [_ _] ["1.14.0" "1.13.0"])
                updater/fetch-version-notes! (fn [_ _ v]
                                               (when (= v "1.14.0")
                                                 {:version v :issues []}))]
    (let [entries (#'updater/fetch-release-notes! "d" "test")]
      (is (= ["1.14.0" "1.13.0"] (mapv :version entries)))
      (is (some? (:notes (first entries))))
      (is (nil? (:notes (second entries)))))))

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
  ;; manifest present but nothing newer than the running editor -> empty entries,
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

(defn- issue [pr-number type duplicate]
  {:author "tester"
   :pr_number pr-number
   :title (str "Issue for PR " pr-number)
   :type type
   :url (str "https://github.com/defold/defold/pull/" pr-number)
   :closed_issues []
   :repository "defold"
   :duplicate duplicate
   :labels []
   :body ""})

(defn- notes-map [version issues]
  {:version version :issues issues :external-link "https://forum"})

(deftest release-notes-drops-issues-already-in-bundled
  (let [updater (make-updater "test" "1")]
    (swap! (:state-atom updater) assoc
           :server-sha1 "B" :release-notes-sha "B"
           :release-notes [{:version "9.9.9"
                            :notes (notes-map "9.9.9" [(issue 1 "FIX" false) (issue 2 "FIX" false)])}])
    (with-redefs [system/defold-version (constantly "9.9.9")
                  updater/bundled-release-notes (constantly (notes-map "9.9.9" [(issue 1 "FIX" false)]))]
      (let [md (:markdown (updater/release-notes updater))]
        (is (re-find #"Issue for PR 2" md))
        (is (not (re-find #"Issue for PR 1" md)))))))

(deftest release-notes-shows-no-new-message-when-fully-seen
  (let [updater (make-updater "test" "1")]
    (swap! (:state-atom updater) assoc
           :server-sha1 "B" :release-notes-sha "B"
           :release-notes [{:version "9.9.9"
                            :notes (notes-map "9.9.9" [(issue 1 "FIX" false)])}])
    (with-redefs [system/defold-version (constantly "9.9.9")
                  updater/bundled-release-notes (constantly (notes-map "9.9.9" [(issue 1 "FIX" false)]))]
      (let [md (:markdown (updater/release-notes updater))]
        (is (re-find #"(?i)no new release notes" md))
        (is (not (re-find #"Issue for PR 1" md)))))))

(deftest release-notes-does-not-diff-newer-versions
  (let [updater (make-updater "test" "1")]
    (swap! (:state-atom updater) assoc
           :server-sha1 "B" :release-notes-sha "B"
           :release-notes [{:version "9.9.10"
                            :notes (notes-map "9.9.10" [(issue 1 "FIX" false)])}])
    (with-redefs [system/defold-version (constantly "9.9.9")
                  updater/bundled-release-notes (constantly (notes-map "9.9.9" [(issue 1 "FIX" false)]))]
      (let [md (:markdown (updater/release-notes updater))]
        (is (re-find #"Issue for PR 1" md))))))

(deftest release-notes-shows-full-notes-when-bundled-unavailable
  (let [updater (make-updater "test" "1")]
    (swap! (:state-atom updater) assoc
           :server-sha1 "B" :release-notes-sha "B"
           :release-notes [{:version "9.9.9"
                            :notes (notes-map "9.9.9" [(issue 1 "FIX" false) (issue 2 "FIX" false)])}])
    (with-redefs [system/defold-version (constantly "9.9.9")
                  updater/bundled-release-notes (constantly nil)]
      (let [md (:markdown (updater/release-notes updater))]
        (is (re-find #"Issue for PR 1" md))
        (is (re-find #"Issue for PR 2" md))))))
