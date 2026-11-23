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

(ns editor.profiler-xctrace-test
  (:require [cljfx.plorer :as p]
            [clojure.java.io :as io]
            [clojure.test :refer [deftest is testing]]
            [dynamo.graph :as g]
            [editor.defold-project :as project]
            [editor.editor-extensions :as extensions]
            [editor.editor-extensions.runtime :as rt]
            [editor.fs :as fs]
            [editor.future :as future]
            [editor.localization :as localization]
            [editor.os :as os]
            [editor.prefs :as prefs]
            [editor.ui :as ui]
            [editor.web-server :as web-server]
            [editor.workspace :as workspace]
            [support.test-support :as test-support]
            [util.coll :as coll]
            [util.http-server :as http-server]
            [util.http-server.types :as http-server.types]
            [util.path :as path])
  (:import [java.io OutputStream]
           [javafx.beans.value ChangeListener]
           [javafx.scene.control Button Label TextField]
           [javafx.stage Stage]))

(set! *warn-on-reflection* true)

(defn- click! [window text]
  (let [button (p/one window {:fx.plorer/class Button :text text})]
    (p/mouse-click! window (p/point button 0.5 0.5) :primary)))

(defn- await-status! [^Label label text action]
  (let [changed (promise)

        listener
        (reify ChangeListener
          (changed [_ _ _ value]
            (when (= text value)
              (deliver changed value))))]
    (ui/run-now (.addListener (.textProperty label) listener))
    (try
      (action)
      (is (= text (deref changed 10000 ::timeout)))
      (finally
        (ui/run-now (.removeListener (.textProperty label) listener))))))

;; Non-macOS editors must not register commands/preferences or require a UI API.
(deftest unavailable-on-other-platforms
  (test-support/with-clean-system
    (doseq [platform ["x86_64-linux" "arm64-linux" "x86_64-win32"]]
      (let [runtime (rt/make :env {"editor" {"platform" platform}})

            module
            (rt/invoke-immediate-1 runtime
                                   (rt/bind runtime (rt/read (io/resource "profiler_xctrace.editor_script"))))]
        (is (= {} (rt/->clj runtime module)))))))

;; Exercise the built-in script, UI, preferences and HTTP transport, including
;; message/path encoding, busy handling and cancellation when its dialog closes.
(deftest recording-dialog
  (when (os/is-mac-os?)
    (test-support/with-clean-system
      (let [directory (fs/create-temp-directory! "xctrace-editor-test")

            test-prefs
            (prefs/project directory (prefs/global (io/file directory "global.editor_settings")))

            localizer
            (localization/make test-prefs ::test
                               {"en.editor_localization" #(io/reader (io/resource "localization/en.editor_localization"))}
                               (fn report-error [^Throwable error] (.printStackTrace error)))

            workspace (workspace/make-workspace directory {} {} localizer)
            project (project/make-project workspace (extensions/make))
            mode (atom :recording)
            state (atom "idle")
            bodies (atom [])
            deletes (atom 0)
            finished (atom (promise))
            trace-path "/tmp/å & traces/game.trace"]
        (prefs/set! test-prefs [:code :enable-lua-language-server] false)
        (with-open [web-server (http-server/start! (web-server/make-dynamic-handler []) :host "127.0.0.1")

                    recorder
                    (http-server/start!
                      (fn handle-request [{:keys [method body]}]
                        (case method
                          "POST" (do
                                   (swap! bodies conj (slurp body))
                                   (reset! state "recording"))
                          "DELETE" (do
                                     (swap! deletes inc)
                                     (reset! state "cancelled"))
                          "GET" (when (= :complete @mode) (reset! state "complete")))
                        (let [response
                              (http-server/json-response
                                {:state @state :path trace-path :message ""}
                                (if (= :busy @mode) 409 200))]
                          (if-not (and (= "GET" method) (= "cancelled" @state))
                            response
                            (assoc response :body
                                   (reify http-server.types/ConnectionWrite
                                     (connection-write! [_ output]
                                       (with-open [^OutputStream output output]
                                         (.write output ^bytes (:body response)))
                                       (deliver @finished true)))))))
                      :host "127.0.0.1")]
          (try
            (extensions/reload! project :all
                                :prefs test-prefs
                                :localization localizer
                                :web-server web-server
                                :reload-resources! (fn reload-resources []
                                                     (workspace/resource-sync! workspace)
                                                     (future/completed nil))
                                :display-output! (fn display-output [type text] (println type text))
                                :save! (fn save [] (future/completed nil))
                                :open-resource! (fn open-resource [_ _] (future/completed nil))
                                :fetch-libraries! (fn fetch-libraries [] (future/completed [[] true]))
                                :invoke-bob! (fn invoke-bob [_ _] (throw (Exception. "Unexpected build invocation"))))
            (let [extension-state
                  (g/with-auto-evaluation-context ec (extensions/ext-state project ec))

                  runtime (:rt extension-state)

                  [_ get-commands]
                  (coll/first-where #(= "profiler_xctrace.editor_script" (first %))
                                    (get-in extension-state [:all :get_commands]))

                  [command]
                  (rt/->clj runtime extensions/commands-coercer
                            (rt/invoke-immediate-1 runtime get-commands))

                  config
                  {:port (http-server/port recorder)
                   :output "/tmp/å & traces"
                   :delay 250
                   :duration 5000
                   :receiver "main:/controller#script"
                   :messages "reset, start_test"
                   :energy false
                   :open false}]
              (is (= :defold.xctrace (:id command)))
              (is (= ["Project"] (:locations command)))
              (prefs/set! test-prefs [:profiler :xctrace] config)
              (let [dialog (rt/invoke-suspending runtime (:run command))
                    window (p/one {:fx.plorer/class Stage :title "XcTrace"})
                    status (p/one window {:fx.plorer/class Label :text "Ready"})]
                (try
                  (testing "settings are committed and sent, including Unicode and message URLs"
                    (let [duration (nth (p/all window TextField) 3)]
                      (p/mouse-click! window (p/point duration 0.5 0.5) :primary)
                      (p/key-chord! window [:meta :a])
                      (p/key-tap! window :digit0)
                      ;; Enter commits in background windows. Focused/headless
                      ;; windows exercise clicking Start without pressing Enter.
                      (when-not (:focused (p/props window :only [:focused]))
                        (p/key-tap! window :enter)))
                    (await-status! status "Recording…" #(click! window "Start Recording"))
                    (is (= [(str "delay_ms=250&duration_ms=0&energy=0&path=%2Ftmp%2F%C3%A5%20%26%20traces%2F"
                                 "&receiver=main%3A%2Fcontroller%23script&messages=reset%2C%20start_test")]
                           @bodies))
                    (is (= (assoc config :duration 0) (prefs/get test-prefs [:profiler :xctrace])))
                    (await-status! status "Recording cancelled" #(click! window "Stop Recording"))
                    (is (= 1 @deletes)))
                  (testing "busy requests do not stop another recording"
                    (reset! mode :busy)
                    (await-status! status "Recording failed" #(click! window "Start Recording"))
                    (is (= 1 @deletes)))
                  (testing "completion exposes the saved path"
                    (reset! mode :complete)
                    (await-status! status "Recording saved" #(click! window "Start Recording"))
                    (is (p/one window {:fx.plorer/class TextField :text trace-path})))
                  (testing "closing stops an unlimited recording owned by the dialog"
                    (reset! mode :recording)
                    (reset! finished (promise))
                    (await-status! status "Recording…" #(click! window "Start Recording"))
                    (click! window "Close")
                    (is (= true (deref @finished 10000 ::timeout)))
                    (is (= 2 @deletes)))
                  (finally
                    (when-not (future/done? dialog)
                      (click! window "Close"))))))
            (finally
              (prefs/unregister-schema! (path/real directory))
              (prefs/sync!)
              (fs/delete-directory! directory))))))))
