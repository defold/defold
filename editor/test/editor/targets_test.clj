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

(ns editor.targets-test
  (:require [clojure.java.io :as io]
            [clojure.test :refer :all]
            [editor.console :as console]
            [editor.fs :as fs]
            [editor.prefs :as prefs]
            [editor.process :as process]
            [editor.targets :as targets]
            [editor.ui :as ui]
            [support.process-util :as process-util]
            [util.fn :as fn]
            [util.http-server :as http-server]
            [util.path :as path])
  (:import [com.dynamo.system.proto System$Exit]
           [java.io BufferedReader ByteArrayInputStream InputStream]
           [java.net InetAddress ServerSocket]
           [java.util.concurrent TimeUnit TimeoutException]
           [javafx.scene Group Scene]
           [javafx.stage Stage]))

(set! *warn-on-reflection* true)

(def ^:private local-target
  {:name "Local"
   :url "http://127.0.0.1:8001"
   :address "127.0.0.1"
   :local-address "127.0.0.1"})

;; Loopback targets are local even when manual discovery stores a LAN adapter
;; address, guarding the locality check used for automatic trace opening.
(deftest loopback-target-locality-test
  (doseq [address ["127.0.0.1" "127.0.0.2" "localhost"]]
    (let [target {:name "Engine"
                  :url (str "http://" address ":8001")
                  :address address
                  :local-address "192.0.2.1"}]
      (is (= (targets/target-message (assoc target :local-address address))
             (targets/target-message target)))))

  (let [target {:name "Remote"
                :url "http://192.0.2.2:8001"
                :address "192.0.2.2"
                :local-address "192.0.2.1"}]
    (is (not= (targets/target-message (assoc target :local-address (:address target)))
              (targets/target-message target)))))

(defn- make-context
  []
  {:targets-atom (atom #{local-target})
   :log-fn (fn/make-call-logger)
   :on-targets-changed-fn (fn/make-call-logger)})

(defn- make-device-info
  [id name host port local-address]
  {:id id
   :name name
   :instance-name name
   :service-name (str name "._defold._tcp.local")
   :host host
   :address host
   :local-address local-address
   :port port
   :log-port "12345"
   :txt {"id" id
         "name" name
         "log_port" "12345"}})

(def ^:private make-local-device-info (partial make-device-info "local-id" "Local" "127.0.0.1" 8001 "127.0.0.1"))
(def ^:private make-iphone-device-info (partial make-device-info "iphone-id" "iPhone" "192.168.0.10" 8001 "127.0.0.1"))
(def ^:private make-tablet-device-info (partial make-device-info "tablet-id" "Tablet" "192.168.0.11" 8001 "127.0.0.1"))

(defn- targets-hostnames
  [targets]
  (->> targets (map :address) sort vec))

(def ^:private call-count (comp count fn/call-logger-calls))

(deftest update-targets-with-new-devices
  (let [{:keys [targets-atom
                on-targets-changed-fn] :as context} (make-context)]
    (testing "iPhone joins"
      (targets/update-targets! context [(make-local-device-info) (make-iphone-device-info)])
      (is (= ["127.0.0.1" "192.168.0.10"] (targets-hostnames @targets-atom)))
      (is (= 1 (call-count on-targets-changed-fn))))
    (testing "iPhone remains, tablet joins"
      (targets/update-targets! context [(make-local-device-info) (make-iphone-device-info) (make-tablet-device-info)])
      (is (= ["127.0.0.1" "192.168.0.10" "192.168.0.11"] (targets-hostnames @targets-atom)))
      (is (= 2 (call-count on-targets-changed-fn))))))

(deftest update-targets-with-known-devices
  (let [{:keys [targets-atom
                on-targets-changed-fn] :as context} (make-context)]
    (testing "iPhone joins, then remains"
      (targets/update-targets! context [(make-local-device-info) (make-iphone-device-info)])
      (targets/update-targets! context [(make-local-device-info) (make-iphone-device-info)])
      (is (= ["127.0.0.1" "192.168.0.10"] (targets-hostnames @targets-atom)))
      (is (= 1 (call-count on-targets-changed-fn))))))

(deftest update-targets-rejects-devices-with-missing-required-fields
  (let [{:keys [targets-atom
                on-targets-changed-fn] :as context} (make-context)
        malformed-device {:id "broken" :name "Broken" :address "192.168.0.99"}]
    (targets/update-targets! context [(make-local-device-info) malformed-device])
    (is (= ["127.0.0.1"] (targets-hostnames @targets-atom)))
    (is (= 1 (call-count on-targets-changed-fn)))))

(deftest update-targets-deduplicates-manual-and-discovered-targets
  (let [{:keys [targets-atom] :as context} (make-context)
        manual-device-atom (deref #'editor.targets/manual-device)
        manual-device (#'editor.targets/manual-target-device "192.168.0.10" "8001" "127.0.0.1" {:log_port "12345"})]
    (try
      (reset! manual-device-atom manual-device)
      (targets/update-targets! context [(make-local-device-info) (make-iphone-device-info)])
      (is (= ["127.0.0.1" "192.168.0.10"] (targets-hostnames @targets-atom)))
      (let [iphone-target (some #(when (= "192.168.0.10" (:address %)) %) @targets-atom)]
        (is (= "manual-192.168.0.10:8001" (:id iphone-target)))
        (is (= "iPhone" (:name iphone-target)))
        (is (= 8001 (:port iphone-target))))
      (finally
        (reset! manual-device-atom nil)))))

(deftest selected-target-restores-manual-target-from-discovered-entry
  (let [mdns-targets-atom (deref #'editor.targets/mdns-targets)
        discovered-target (#'editor.targets/device->target (make-iphone-device-info))]
    (try
      (#'editor.targets/clear-selected-target-hint!)
      (reset! mdns-targets-atom [discovered-target])
      (with-redefs [prefs/get (fn [_ path]
                                (when (= [:run :selected-target-id] path)
                                  "manual-192.168.0.10:8001"))]
        (let [selected-target (targets/selected-target nil)]
          (is (= "192.168.0.10" (:address selected-target)))
          (is (= 8001 (:port selected-target)))
          (is (= "iphone-id" (:id selected-target)))))
      (finally
        (reset! mdns-targets-atom [])
        (#'editor.targets/clear-selected-target-hint!)))))

;; Selecting a target without a log service clears the previous console stream,
;; guarding against keeping stale logs after adding completion callbacks.
(deftest select-target-clears-log-service-stream-when-unreachable
  (let [directory (fs/create-temp-directory! "target-log-service")
        prefs (prefs/make :scopes {:global (path/of directory "preferences")} :schemas [:default])
        previous-target (targets/selected-target prefs)]
    (try
      (with-open [stream (ByteArrayInputStream. (byte-array 0))]
        (console/reset-console-stream! stream)
        (targets/select-target! prefs (dissoc (make-iphone-device-info) :log-port))
        (is (console/current-stream? nil)))
      (finally
        (targets/select-target! prefs previous-target)
        (fs/delete-directory! directory)))))

;; Registered listeners receive only log lines and can be added while a
;; local log-service connection is active, without target-selection plumbing.
(deftest select-target-log-callback-test
  (let [directory (fs/create-temp-directory! "target-log-callback")
        prefs (prefs/make :scopes {:global (path/of directory "preferences")} :schemas [:default])
        previous-target (targets/selected-target prefs)
        connected (promise)
        received (promise)
        next-received (promise)
        line "INFO:ENGINE: target log"]
    (try
      (console/register-local-log-listener!
        (fn receive-log-line! [line]
          (deliver received line)))
      (with-open [server (doto (ServerSocket. 0 1 (InetAddress/getByName "127.0.0.1"))
                           (.setSoTimeout 10000))]
        (let [sent (future
                     (with-open [socket (.accept server)
                                 writer (io/writer (.getOutputStream socket) :encoding "UTF-8")]
                       (.write writer "0 OK\n")
                       (.flush writer)
                       (when (= true (deref connected 10000 ::not-connected))
                         (.write writer (str line "\n"))
                         (.flush writer))))]
          (try
            (targets/select-target!
              prefs
              {:id "log-callback-test"
               :name "Engine"
               :url "http://127.0.0.1:8001"
               :address "127.0.0.1"
               :local-address "192.0.2.1"
               :log-port (str (.getLocalPort server))})
            (deliver connected true)
            (is (= line (deref received 10000 ::not-received)))
            (is (nil? (deref sent 10000 ::not-sent)))
            (console/register-local-log-listener!
              (fn receive-next-log-line! [line]
                (deliver next-received line)))
            (console/notify-local-log-listeners! "next line")
            (is (= "next line" (deref next-received 10000 ::not-received)))
            (finally
              (deliver connected false)
              (future-cancel sent)))))
      (finally
        (targets/select-target! prefs previous-target)
        (fs/delete-directory! directory)))))

(defmacro ^:private with-launched-target
  "Bind process and target with an HTTP exit service handled by handler."
  [handler & body]
  `(process-util/with-child-process {:err :inherit}
     ;; Process.destroy may close stdin before delivering its signal.
     ;; Only an exit-service byte should let the child exit successfully.
     '(do
        (println "ready")
        (flush)
        (when (= -1 (.read System/in))
          @(promise))) []
     (with-open [reader# (io/reader (process/out ~'process))]
       (is (= "ready" (.readLine ^BufferedReader reader#))))
     (with-open [server# (http-server/start!
                           (http-server/router-handler
                             {"/post/@system/exit" {"POST" ~handler}})
                           :host "127.0.0.1")]
       (let [prefs# (prefs/global (fs/create-temp-file! "target-test" ".editor_settings"))
             ^Stage stage# (ui/run-now (doto (Stage.) (.setScene (Scene. (Group.)))))]
         (try
           (binding [ui/*main-stage* (atom stage#)]
             (let [~'target (targets/add-launched-target! prefs# 0
                                                          {:process ~'process :url (http-server/local-url server#)})]
               (try
                 ~@body
                 (finally
                   (targets/kill-launched-target! ~'target)))))
           (finally
             (ui/run-now (.close stage#))))))))

;; An unregistered target still acknowledges input after Stop, guarding against
;; stale stop requests without relying on the scheduling of process destruction.
(deftest kill-unregistered-target-test
  (process-util/with-child-process {:err :inherit}
    '(do (println "ready") (flush) (println (str "ack:" (read-line)))) []
    (with-open [reader (io/reader (process/out process))
                writer (io/writer (process/in process))]
      (is (= "ready" (.readLine ^BufferedReader reader)))
      (targets/kill-launched-target! {:process process})
      (.write writer "after-stop\n")
      (.flush writer)
      (is (= "ack:after-stop" (.readLine ^BufferedReader reader))))))

;; Stop must use the target's exit service and preserve its exit code,
;; guarding against replacing graceful shutdown with process destruction.
(deftest kill-launched-target-prefers-clean-exit-when-service-url-is-known
  (let [exit-code (atom nil)]
    (with-launched-target
      (fn exit-target [{:keys [body]}]
        (reset! exit-code (.getCode (System$Exit/parseFrom ^InputStream body)))
        (.write (process/in process) (int 1))
        (.flush (process/in process))
        ;; Complete the response after the child exits so the production grace
        ;; period does not depend on how quickly CI schedules the child.
        (when-not (.waitFor process 10 TimeUnit/SECONDS)
          (throw (TimeoutException. "Child did not respond to the exit request within 10 seconds.")))
        http-server/ok)
      (targets/kill-launched-target! target)
      (is (= 0 @exit-code))
      (is (.waitFor process 10 TimeUnit/SECONDS))
      (is (zero? (.exitValue process))))))

;; A failed exit-service request must fall back to terminating the real process,
;; guarding against leaving registered targets running after Stop.
(deftest kill-launched-target-falls-back-to-process-destroy-when-clean-exit-fails
  (let [exit-code (atom nil)]
    (with-launched-target
      (fn reject-exit [{:keys [body]}]
        (reset! exit-code (.getCode (System$Exit/parseFrom ^InputStream body)))
        http-server/internal-server-error)
      (targets/kill-launched-target! target)
      (is (= 0 @exit-code))
      (is (.waitFor process 10 TimeUnit/SECONDS))
      (is (not (zero? (.exitValue process)))))))
