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
            [service.log :as log]
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
             :repository "defold"}]})

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

(defn- ui-flushed? []
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

;; Verifies unavailable or malformed metadata gives no offer, a beta that isn't
;; newer is not offered and its notes are not fetched, and missing notes still
;; give an offer, just without notes.
(deftest fetch-offer
  (let [routes (atom {})
        requests (atom [])]
    (with-open [server (start-server! routes requests)]
      (let [source (make-source server)]
        (testing "Unavailable or malformed metadata"
          (doseq [response [http-server/not-found
                            (http-server/json-response {:version "1.14.0"})]]
            (reset! routes {info-path response})
            (is (nil? (:info (log/without-logging (beta/fetch-offer! source)))))))
        (testing "Newer beta"
          (reset! routes (beta-routes "1.14.0"))
          (reset! requests [])
          (let [{:keys [info newer markdown]} (beta/fetch-offer! source)]
            (is (= {:version "1.14.0"
                    :sha1 "B2"}
                   info))
            (is newer)
            (is (string/includes? markdown "Defold 1.14.0 beta"))
            (is (= [info-path (notes-path "1.14.0")] @requests))))
        (testing "Not newer"
          (reset! routes (beta-routes "1.13.2"))
          (reset! requests [])
          (is (not (:newer (beta/fetch-offer! source))))
          (is (= [info-path] @requests)))
        (testing "Missing notes"
          (reset! routes (dissoc (beta-routes "1.14.0") (notes-path "1.14.0")))
          (let [offer (log/without-logging (beta/fetch-offer! source))]
            (is (:newer offer))
            (is (nil? (:markdown offer)))))))))

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
;; installations never request beta metadata, the menu command does nothing on
;; other channels, and startup discovery runs once per session.
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
            (is (nil? (check! (make-source server :channel channel) (make-temp-prefs))))
            (is (nil? (try! (beta/make-session) (make-source server :channel channel) stage progress/null-render-progress!)))))
        (testing "Unsupported platform"
          (is (nil? (check! (make-source server :platform Platform/WasmWeb) (make-temp-prefs)))))
        (is (= [] @requests))
        (testing "Once per session"
          (let [session (beta/make-session)
                source (make-source server)
                prefs (make-temp-prefs)]
            (is (nil? (deref (suggest! session source stage nil prefs) timeout-ms :timed-out)))
            (is (nil? (deref (suggest! session source stage nil prefs) timeout-ms :timed-out)))
            (is (= [info-path] @requests))))))))

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
              (is (not (beta/busy? session))))
            (finally
              (ui/run-now (.hide stage)))))))))

;; Verifies the command is disabled while a check is in progress and becomes
;; available again after a failed check, with progress shown and cleared.
(deftest try-releases-busy-state
  (let [started (promise)
        finish (promise)
        routes (atom {info-path (fn blocked-info []
                                  (deliver started true)
                                  (deref finish timeout-ms nil)
                                  http-server/not-found)})
        requests (atom [])
        progress (atom [])]
    (with-open [server (start-server! routes requests)]
      (let [session (beta/make-session)
            source (make-source server)
            stage (make-hidden-stage)
            check (log/without-logging (try! session source stage #(swap! progress conj %)))]
        (try
          (is (true? (deref started timeout-ms :timed-out)))
          (is (beta/busy? session))
          (is (nil? (try! session source stage progress/null-render-progress!)))
          (finally
            (deliver finish true)))
        (is (not= :timed-out (deref check timeout-ms :timed-out)))
        (is (ui-flushed?))
        (is (not (beta/busy? session)))
        (is (= 2 (count @progress)))
        (is (nil? (progress/fraction (first @progress))))
        (is (progress/done? (last @progress)))))))
