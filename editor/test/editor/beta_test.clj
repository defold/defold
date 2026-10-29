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
  (:require [clojure.string :as string]
            [clojure.test :refer :all]
            [dynamo.graph :as g]
            [editor.beta :as beta]
            [editor.fs :as fs]
            [editor.localization :as localization]
            [editor.notifications :as notifications]
            [editor.prefs :as prefs]
            [editor.progress :as progress]
            [editor.ui :as ui]
            [support.test-support :refer [with-clean-system]]
            [util.http-server :as http-server])
  (:import [com.dynamo.bob Platform]
           [javafx.scene Group Scene]
           [javafx.stage Stage]
           [util.http_server ServerWithHandler]))

(set! *warn-on-reflection* true)

(def ^:private timeout-ms 10000)

(def ^:private info-path "/beta/info.json")

(defn- notes-path [version]
  (format "/editor2/channels/beta/release-notes/%s.json" version))

(def ^:private release-notes
  {:external-link "https://forum.defold.com/release"
   :issues [{:title "Beta feature"
             :type "NEW"
             :body "Feature details"
             :url "https://github.com/defold/defold/pull/1"
             :pr_number 1
             :author "author"
             :repository "defold"}
            {:title "Duplicate"
             :type "NEW"
             :repository "defold"
             :duplicate true}]})

(defn- beta-routes [version]
  {info-path (http-server/json-response {:version version
                                         :sha1 "B2"})
   (notes-path version) (http-server/json-response release-notes)})

(defn- start-server!
  "Serves the responses in the routes atom, keyed by path. A route may be a
  0-arg fn producing the response. Requested paths are recorded in order."
  ^ServerWithHandler [routes requests]
  (http-server/start!
    (fn [{:keys [path]}]
      (swap! requests conj path)
      (let [route (get @routes path http-server/not-found)]
        (if (fn? route) (route) route)))))

(defn- make-source [server & {:as overrides}]
  (merge {:base-url (http-server/local-url server)
          :channel "stable"
          :version "1.13.2"
          :platform Platform/X86_64Linux}
         overrides))

(defn- make-temp-prefs []
  (prefs/make :scopes {:global (fs/create-temp-file! "beta-test" ".editor_settings")}
              :schemas [:default]))

(defn- make-hidden-stage
  ^Stage []
  (ui/run-now (ui/make-stage)))

(defn- make-showing-stage
  ^Stage []
  (ui/run-now (doto (ui/make-stage)
                (.setScene (Scene. (Group.)))
                (.show))))

(defn- ui-flushed?
  "Returns true once the UI thread has run everything queued before the call."
  []
  (let [flushed (promise)]
    (ui/run-later (deliver flushed true))
    (deref flushed timeout-ms false)))

(defn- notification [notifications-node id]
  (get-in (g/node-value notifications-node :notifications) [:id->notification id]))

(defn- suggest! [session source stage notifications-node prefs]
  (beta/suggest! session source stage notifications-node nil :git prefs nil progress/null-render-progress!))

(defn- try! [session source stage render-progress!]
  (beta/try! session source stage nil nil nil render-progress!))

;; Verifies download links point to the beta archive, with a .dmg on macOS so
;; the app signature stays intact and a .zip elsewhere.
(deftest download-url-per-platform
  (let [source {:base-url "https://d.defold.com"}]
    (is (= "https://d.defold.com/archive/B2/beta/editor2/Defold-x86_64-linux.zip"
           (beta/download-url (assoc source :platform Platform/X86_64Linux) "B2")))
    (is (= "https://d.defold.com/archive/B2/beta/editor2/Defold-x86_64-win32.zip"
           (beta/download-url (assoc source :platform Platform/X86_64Win32) "B2")))
    (is (= "https://d.defold.com/archive/B2/beta/editor2/Defold-arm64-macos.dmg"
           (beta/download-url (assoc source :platform Platform/Arm64MacOS) "B2")))
    (is (= "https://d.defold.com/archive/B2/beta/editor2/Defold-x86_64-macos.dmg"
           (beta/download-url (assoc source :platform Platform/X86_64MacOS) "B2")))))

;; Verifies malformed or unavailable remote metadata cannot become an offer.
(deftest fetch-offer-validates-metadata
  (let [routes (atom {})
        requests (atom [])]
    (with-open [server (start-server! routes requests)]
      (let [source (make-source server)]
        (doseq [response [http-server/not-found
                          http-server/internal-server-error
                          (http-server/response 200 "not json")]]
          (reset! routes {info-path response})
          (is (= {:info nil
                  :newer false
                  :markdown nil}
                 (beta/fetch-offer! source))))
        (doseq [info [{} [] "bad" {:version "1.14.0"}
                      {:version 114
                       :sha1 "B2"}
                      {:version "bad"
                       :sha1 "B2"}
                      {:version "1.14.0"
                       :sha1 ""}]]
          (reset! routes {info-path (http-server/json-response info)})
          (is (nil? (:info (beta/fetch-offer! source)))))
        (reset! routes (beta-routes "1.14.0"))
        (is (= {:version "1.14.0"
                :sha1 "B2"}
               (:info (beta/fetch-offer! source))))))))

;; Verifies equal and older betas are not offered and their notes are not
;; fetched, using natural version order and treating dev builds without a
;; version as older than any beta.
(deftest fetch-offer-compares-with-running-version
  (let [routes (atom {})
        requests (atom [])]
    (with-open [server (start-server! routes requests)]
      (doseq [[running beta newer] [["1.13.2" "1.13.2" false]
                                    ["1.13.2" "1.12.0" false]
                                    ["1.9.0" "1.10.0" true]
                                    [nil "1.14.0" true]]]
        (testing (str running " vs " beta)
          (reset! routes (beta-routes beta))
          (reset! requests [])
          (let [offer (beta/fetch-offer! (make-source server :version running))]
            (is (= newer (:newer offer)))
            (is (= (cond-> [info-path] newer (conj (notes-path beta)))
                   @requests))))))))

;; Verifies beta release notes are fetched from the beta channel and rendered
;; with the updater's formatting, and missing or malformed notes still produce
;; an offer, just without notes.
(deftest fetch-offer-release-notes
  (let [routes (atom (beta-routes "1.14.0"))
        requests (atom [])]
    (with-open [server (start-server! routes requests)]
      (let [source (make-source server)
            {:keys [markdown]} (beta/fetch-offer! source)]
        (is (= [info-path (notes-path "1.14.0")] @requests))
        (is (string/includes? (first (string/split-lines markdown)) "Defold 1.14.0 beta"))
        (is (string/includes? markdown "Beta feature"))
        (is (string/includes? markdown "Feature details"))
        (is (string/includes? markdown "https://forum.defold.com/release"))
        (is (not (string/includes? markdown "Duplicate"))))
      (doseq [notes [[] {} {:issues "bad"}
                     {:issues [{:repository "defold"
                                :labels 42}]}]]
        (swap! routes assoc (notes-path "1.14.0") (http-server/json-response notes))
        (let [offer (beta/fetch-offer! (make-source server))]
          (is (:newer offer))
          (is (nil? (:markdown offer)))))
      (swap! routes dissoc (notes-path "1.14.0"))
      (let [offer (beta/fetch-offer! (make-source server))]
        (is (:newer offer))
        (is (nil? (:markdown offer)))))))

;; Verifies running a beta is remembered so the user is never suggested one
;; again, and that projects without git make no requests.
(deftest suggest-records-ran-beta
  (let [routes (atom (beta-routes "1.14.0"))
        requests (atom [])]
    (with-open [server (start-server! routes requests)]
      (doseq [[channel ran-beta] [["beta" true]
                                  ["stable" false]]]
        (testing channel
          (let [prefs (make-temp-prefs)]
            (is (nil? (beta/suggest! (beta/make-session) (make-source server :channel channel) (make-hidden-stage) nil nil nil prefs nil progress/null-render-progress!)))
            (is (= ran-beta (prefs/get prefs [:versioning :ran-beta]))))))
      (is (= [] @requests)))))

;; Verifies opted-out users, users who have run a beta and ineligible
;; installations never request beta metadata.
(deftest suggest-respects-eligibility
  (let [routes (atom (beta-routes "1.14.0"))
        requests (atom [])]
    (with-open [server (start-server! routes requests)]
      (let [stage (make-hidden-stage)
            check! (fn check! [source prefs]
                     (deref (suggest! (beta/make-session) source stage nil prefs) timeout-ms :timed-out))]
        (doseq [[label path] [["Opt-out" [:versioning :beta-suggestions-disabled]]
                              ["Ran a beta" [:versioning :ran-beta]]]]
          (testing label
            (let [prefs (make-temp-prefs)]
              (prefs/set! prefs path true)
              (is (nil? (check! (make-source server) prefs))))))
        (testing "Other channels"
          (doseq [channel ["beta" "alpha" nil]]
            (is (nil? (check! (make-source server :channel channel) (make-temp-prefs))))))
        (testing "Unsupported platform"
          (is (nil? (check! (make-source server :platform Platform/WasmWeb) (make-temp-prefs)))))
        (is (= [] @requests))))))

;; Verifies startup discovery runs once per session even when another project
;; opens during an outstanding check.
(deftest suggest-checks-once-per-session
  (let [started (promise)
        finish (promise)
        routes (atom (assoc (beta-routes "1.14.0")
                       info-path (fn blocked-info []
                                   (deliver started true)
                                   (deref finish timeout-ms nil)
                                   (http-server/json-response {:version "1.14.0"
                                                               :sha1 "B2"}))))
        requests (atom [])]
    (with-open [server (start-server! routes requests)]
      (let [session (beta/make-session)
            source (make-source server)
            stage (make-hidden-stage)
            prefs (make-temp-prefs)
            first-check (suggest! session source stage nil prefs)]
        (try
          (is (true? (deref started timeout-ms :timed-out)))
          (is (nil? (deref (suggest! session source stage nil prefs) timeout-ms :timed-out)))
          (is (= [info-path] @requests))
          (finally
            (deliver finish true)))
        (is (not= :timed-out (deref first-check timeout-ms :timed-out)))
        (is (nil? (deref (suggest! session source stage nil prefs) timeout-ms :timed-out)))
        (is (= [info-path] @requests))))))

;; Verifies a failed startup request is not retried during the same session.
(deftest suggest-failure-does-not-retry
  (let [routes (atom {})
        requests (atom [])]
    (with-open [server (start-server! routes requests)]
      (let [session (beta/make-session)
            source (make-source server)
            stage (make-hidden-stage)
            prefs (make-temp-prefs)]
        (is (nil? (deref (suggest! session source stage nil prefs) timeout-ms :timed-out)))
        (is (nil? (deref (suggest! session source stage nil prefs) timeout-ms :timed-out)))
        (is (= [info-path] @requests))))))

;; Verifies the suggestion appears in the notifications view with the beta
;; version, and its actions opt out or open the offer.
(deftest suggest-shows-notification
  (with-clean-system
    (let [routes (atom (beta-routes "1.14.0"))
          requests (atom [])]
      (with-open [server (start-server! routes requests)]
        (let [session (beta/make-session)
              source (make-source server)
              prefs (make-temp-prefs)
              notifications-node (g/make-node! notifications/NotificationsNode)
              stage (make-showing-stage)]
          (try
            (deref (suggest! session source stage notifications-node prefs) timeout-ms :timed-out)
            (is (ui-flushed?))
            (let [{:keys [message actions]} (notification notifications-node ::beta/suggestion)
                  [try-it do-not-suggest] actions]
              (is (= (localization/message "beta.notification.available" {"version" "1.14.0"}) message))
              (is (= [(localization/message "beta.notification.try-it")
                      (localization/message "beta.button.do-not-suggest")]
                     (mapv :message actions)))
              ((:on-action do-not-suggest))
              (is (true? (prefs/get prefs [:versioning :beta-suggestions-disabled])))
              ;; The offer dialog only opens on a showing stage, so hide it first.
              (ui/run-now (.hide stage))
              (reset! requests [])
              (let [offer (:on-action try-it)]
                (is (not= :timed-out (deref (offer) timeout-ms :timed-out))))
              (is (ui-flushed?))
              (is (= [info-path (notes-path "1.14.0")] @requests))
              (is (beta/try-enabled? session source)))
            (finally
              (ui/run-now (.hide stage)))))))))

;; Verifies a suggestion is not shown to users who opted out while the check
;; was in flight.
(deftest suggest-skips-notification-after-opt-out
  (with-clean-system
    (let [prefs (make-temp-prefs)
          routes (atom (assoc (beta-routes "1.14.0")
                         info-path (fn opt-out-during-check []
                                     (prefs/set! prefs [:versioning :beta-suggestions-disabled] true)
                                     (http-server/json-response {:version "1.14.0"
                                                                 :sha1 "B2"}))))
          requests (atom [])]
      (with-open [server (start-server! routes requests)]
        (let [notifications-node (g/make-node! notifications/NotificationsNode)
              stage (make-showing-stage)]
          (try
            (deref (suggest! (beta/make-session) (make-source server) stage notifications-node prefs) timeout-ms :timed-out)
            (is (ui-flushed?))
            (is (nil? (notification notifications-node ::beta/suggestion)))
            (finally
              (ui/run-now (.hide stage)))))))))

;; Verifies the menu command is only available on eligible installations.
(deftest try-availability
  (let [source {:base-url "https://d.defold.com"
                :channel "stable"
                :version "1.13.2"
                :platform Platform/X86_64Linux}]
    (is (beta/try-active? source))
    (is (beta/try-enabled? (beta/make-session) source))
    (is (not (beta/try-active? (assoc source :channel "beta"))))
    (is (not (beta/try-active? (assoc source :platform Platform/WasmWeb))))
    (is (not (beta/try-enabled? (beta/make-session) (assoc source :channel "alpha"))))
    (is (nil? (try! (beta/make-session) (assoc source :channel "beta") (make-hidden-stage) progress/null-render-progress!)))))

;; Verifies the command is disabled while a check is in progress and becomes
;; available again afterwards, with progress shown and cleared, whether the
;; check succeeds or fails.
(deftest try-releases-busy-state
  (doseq [[label routes] [["Offer" (beta-routes "1.14.0")]
                          ["No notes" (dissoc (beta-routes "1.14.0") (notes-path "1.14.0"))]
                          ["Not newer" (beta-routes "1.13.2")]
                          ["Malformed" {info-path (http-server/json-response {:version "bad"})}]
                          ["Unavailable" {}]
                          ["Server error" {info-path http-server/internal-server-error}]]]
    (testing label
      (let [started (promise)
            finish (promise)
            routes (atom (update routes info-path
                                 (fn [response]
                                   (fn blocked-info []
                                     (deliver started true)
                                     (deref finish timeout-ms nil)
                                     (or response http-server/not-found)))))
            requests (atom [])
            progress (atom [])]
        (with-open [server (start-server! routes requests)]
          (let [session (beta/make-session)
                source (make-source server)
                stage (make-hidden-stage)
                check (try! session source stage #(swap! progress conj %))]
            (try
              (is (true? (deref started timeout-ms :timed-out)))
              (is (not (beta/try-enabled? session source)))
              (is (nil? (try! session source stage progress/null-render-progress!)))
              (finally
                (deliver finish true)))
            (is (not= :timed-out (deref check timeout-ms :timed-out)))
            (is (ui-flushed?))
            (is (beta/try-enabled? session source))
            (is (= 2 (count @progress)))
            (is (nil? (progress/fraction (first @progress))))
            (is (progress/done? (last @progress)))))))))
