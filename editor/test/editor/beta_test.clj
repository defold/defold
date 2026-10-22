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

(ns editor.beta-test
  (:require [clojure.java.io :as io]
            [clojure.string :as string]
            [clojure.test :refer :all]
            [editor.beta :as beta]
            [editor.fs :as fs]
            [editor.prefs :as prefs]
            [editor.progress :as progress]
            [editor.system :as system]
            [editor.updater :as updater]
            [util.http-server :as http-server]
            [util.net :as net])
  (:import [com.dynamo.bob Platform]
           [java.io File IOException]))

(set! *warn-on-reflection* true)

(use-fixtures :each
  (fn isolated-session-fixture [f]
    (with-redefs [beta/automatic-check-started (atom false)
                  system/defold-channel (constantly "stable")
                  system/defold-version (constantly "1.13.2")]
      (f))))

;; Verifies startup discovery runs once even when another project opens during
;; an outstanding check, without depending on a timer or updater instance.
(deftest automatic-check-runs-once
  (let [started (promise)
        finish (promise)
        info {:version "1.14.0" :sha1 "B2"}]
    (with-redefs [prefs/get (constantly nil)
                  beta/fetch-info! (fn fetch-info! []
                                     (deliver started true)
                                     @finish
                                     info)]
      (let [check (future (beta/check-once! nil Platform/X86_64Linux))]
        (try
          (is (= true (deref started 10000 :timed-out)))
          (is (nil? (beta/check-once! nil Platform/X86_64Linux)))
          (finally
            (deliver finish true)))
        (is (= info @check))
        (is (nil? (beta/check-once! nil Platform/X86_64Linux)))))))

;; Verifies opted-out users and ineligible installations never request beta
;; metadata, including on platforms without editor downloads.
(deftest automatic-check-respects-eligibility
  (with-redefs [beta/fetch-info! (fn unexpected-request! []
                                   (throw (AssertionError. "Unexpected beta request")))]
    (testing "Opt-out"
      (with-redefs [prefs/get (constantly true)]
        (is (nil? (beta/check-once! nil Platform/X86_64Linux)))))
    (with-redefs [prefs/get (constantly nil)]
      (testing "Other channels"
        (doseq [channel ["beta" "alpha" nil]]
          (with-redefs [system/defold-channel (constantly channel)]
            (is (nil? (beta/check-once! nil Platform/X86_64Linux))))))
      (testing "Unsupported platform"
        (is (nil? (beta/check-once! nil Platform/WasmWeb)))))))

;; Verifies a failed startup request is not retried during the same session.
(deftest automatic-check-failure-does-not-retry
  (let [requests (atom 0)]
    (with-redefs [prefs/get (constantly nil)
                  beta/fetch-info! (fn failed-request! []
                                     (swap! requests inc)
                                     nil)]
      (is (nil? (beta/check-once! nil Platform/X86_64Linux)))
      (is (nil? (beta/check-once! nil Platform/X86_64Linux)))
      (is (= 1 @requests)))))

;; Verifies suggestions exclude equal, older and already downloaded betas.
(deftest automatic-check-filters-offers
  (doseq [[version downloaded] [["1.13.2" nil] ["1.12.0" nil] ["1.14.0" "B2"]]]
    (with-redefs [beta/automatic-check-started (atom false)
                  prefs/get (fn get-pref [_ path]
                              (when (= [:versioning :downloaded-beta-sha1] path)
                                downloaded))
                  beta/fetch-info! (constantly {:version version :sha1 "B2"})]
      (is (nil? (beta/check-once! nil Platform/X86_64Linux))))))

;; Verifies malformed or unavailable remote metadata cannot become an offer.
(deftest fetch-info-validates-metadata
  (doseq [info [nil {} [] "bad" {:version 114 :sha1 "B2"}
                {:version "bad" :sha1 "B2"} {:version "1.14.0"}
                {:version "1.14.0" :sha1 ""}]]
    (with-redefs [updater/fetch-json! (constantly info)]
      (is (nil? (beta/fetch-info!)))))
  (let [info {:version "1.14.0" :sha1 "B2"}]
    (with-redefs [updater/fetch-json! (constantly info)]
      (is (= info (beta/fetch-info!))))))

;; Verifies natural version comparison handles double-digit components and dev
;; builds without a version, independently of the updater.
(deftest compares-beta-with-running-version
  (with-redefs [system/defold-version (constantly "1.9.0")]
    (is (beta/newer-than-running? "1.10.0"))
    (is (not (beta/newer-than-running? "1.9.0"))))
  (with-redefs [system/defold-version (constantly nil)]
    (is (beta/newer-than-running? "1.14.0"))))

;; Verifies beta release notes are fetched from the beta channel and rendered
;; with the updater's formatting, and missing or malformed notes allow a plain
;; download prompt.
(deftest beta-release-notes
  (let [notes {:external-link "https://forum.defold.com/release"
               :issues [{:title "Beta feature" :type "NEW" :body "Feature details"
                         :url "https://github.com/defold/defold/pull/1"
                         :pr_number 1 :author "author" :repository "defold"}
                        {:title "Duplicate" :type "NEW" :repository "defold" :duplicate true}]}
        requests (atom [])]
    (with-redefs [updater/fetch-version-notes! (fn fetch-notes! [_archive-domain channel version]
                                                 (swap! requests conj [channel version])
                                                 notes)]
      (let [markdown (beta/release-notes "1.14.0")]
        (is (= [["beta" "1.14.0"]] @requests))
        (is (string/includes? (first (string/split-lines markdown)) "Defold 1.14.0 beta"))
        (is (string/includes? markdown "Beta feature"))
        (is (string/includes? markdown "Feature details"))
        (is (string/includes? markdown "https://forum.defold.com/release"))
        (is (not (string/includes? markdown "Duplicate"))))))
  (doseq [notes [nil [] {} {:issues "bad"}]]
    (with-redefs [updater/fetch-version-notes! (constantly notes)]
      (is (nil? (beta/release-notes "1.14.0"))))))

;; Verifies real HTTP downloads select the platform's installer format, preserve
;; sibling files and clear progress after replacing the chosen destination.
(deftest downloads-beta-archive-to-chosen-file
  (with-open [server (http-server/start!
                       (fn serve-archive [_]
                         (http-server/response 200 "beta archive")))]
    (let [dir (fs/create-temp-directory! "beta-download-test")
          target (io/file dir "chosen.bin")
          sibling (io/file dir "chosen.bin.part")
          download! net/download!]
      (try
        (spit sibling "existing sibling")
        (doseq [platform [Platform/X86_64Linux Platform/X86_64Win32
                          Platform/X86_64MacOS Platform/Arm64MacOS]]
          (let [events (atom [])
                urls (atom [])
                extension (if (= "macos" (.getOs ^Platform platform)) "dmg" "zip")]
            (with-redefs [net/download! (fn download-local! [url out & args]
                                          (swap! urls conj url)
                                          (apply download!
                                                 (str "http://localhost:" (http-server/port server) "/archive")
                                                 out args))]
              (is (= target (beta/download! {:sha1 "B2"} platform target
                                            (fn render-progress! [value] (swap! events conj value)))))
              (is (string/ends-with? (first @urls) (str "/Defold-" (.getPair ^Platform platform) "." extension)))
              (is (string/ends-with? (beta/download-file-name platform "1.14.0") (str "." extension)))
              (is (= "beta archive" (slurp target)))
              (is (= "existing sibling" (slurp sibling)))
              (is (= #{"chosen.bin" "chosen.bin.part"} (set (.list ^File dir))))
              (is (progress/done? (last @events))))))
        (finally
          (fs/delete-directory! dir))))))

;; Verifies a transfer that fails after writing some bytes cleans its temporary
;; file, preserves the existing destination and sibling, and completes progress.
(deftest failed-download-preserves-existing-files
  (let [dir (fs/create-temp-directory! "beta-download-failure-test")
        target (io/file dir "chosen.bin")
        sibling (io/file dir "chosen.bin.part")
        events (atom [])]
    (try
      (spit target "existing download")
      (spit sibling "existing sibling")
      (with-redefs [net/download! (fn fail-mid-download! [_ out & _args]
                                    (spit out "partial download")
                                    (throw (IOException. "Transfer interrupted")))]
        (is (nil? (beta/download! {:sha1 "B2"} Platform/X86_64Linux target
                                  (fn render-progress! [value] (swap! events conj value))))))
      (is (= "existing download" (slurp target)))
      (is (= "existing sibling" (slurp sibling)))
      (is (= #{"chosen.bin" "chosen.bin.part"} (set (.list ^File dir))))
      (is (progress/done? (last @events)))
      (finally
        (fs/delete-directory! dir)))))
