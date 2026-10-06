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

(ns support.dap-util
  (:require [clojure.data.json :as json]
            [clojure.string :as string]
            [clojure.test :refer [is]]
            [editor.debugging.dap :as dap]
            [editor.future :as future]
            [editor.ui :as ui])
  (:import [java.io ByteArrayOutputStream DataInputStream EOFException OutputStream]
           [java.net InetAddress ServerSocket Socket SocketException]
           [java.nio.charset StandardCharsets]
           [java.util.concurrent ExecutionException LinkedBlockingQueue]
           [javafx.scene.control TreeItem TreeView]))

(set! *warn-on-reflection* true)

(defn await! [value]
  (try
    @value
    (catch ExecutionException exception
      (throw (.getCause exception)))))

(defn await-ui! [^TreeView view f]
  (let [finished (future/make)
        check! (fn []
                 (when-not (future/done? finished)
                   (try
                     (when-let [result (f)]
                       (future/complete! finished result))
                     (catch Throwable exception
                       (future/fail! finished exception)))))
        ;; Tree events fire inside cljfx advancement. The first queued callback
        ;; runs after advancement has queued viewport restoration; the second
        ;; checks the tree after that restoration.
        schedule-check! (fn [] (ui/run-later (ui/run-later (check!))))
        tree-changed (ui/event-handler _ (schedule-check!))
        root-changed (ui/change-listener _ old-root new-root
                                         (when old-root
                                           (.removeEventHandler ^TreeItem old-root (TreeItem/treeNotificationEvent) tree-changed))
                                         (when new-root
                                           (.addEventHandler ^TreeItem new-root (TreeItem/treeNotificationEvent) tree-changed))
                                         (schedule-check!))]
    (ui/run-now
      (.addListener (.rootProperty view) root-changed)
      (when-let [root (.getRoot view)]
        (.addEventHandler root (TreeItem/treeNotificationEvent) tree-changed))
      (schedule-check!))
    (try
      @finished
      (catch ExecutionException exception
        (throw (.getCause exception)))
      (finally
        (ui/run-now
          (.removeListener (.rootProperty view) root-changed)
          (when-let [root (.getRoot view)]
            (.removeEventHandler root (TreeItem/treeNotificationEvent) tree-changed)))))))

(defn take-event! [^LinkedBlockingQueue events]
  (.take events))

(defn wire-bytes [message]
  (let [body (.getBytes (json/write-str message :escape-unicode false) StandardCharsets/UTF_8)
        out (ByteArrayOutputStream.)]
    (.write out (.getBytes (str "Content-Length: " (alength body) "\r\n\r\n") StandardCharsets/US_ASCII))
    (.write out body)
    (.toByteArray out)))

(defn receive! [^DataInputStream in]
  (let [line (.readLine in)]
    (when-not line (throw (EOFException.)))
    (let [length (parse-long (second (string/split line #": ")))
          body (byte-array length)]
      (is (= "" (.readLine in)))
      (.readFully in body)
      (json/read-str (String. body StandardCharsets/UTF_8) :key-fn keyword))))

(defn send! [^OutputStream out message]
  (.write out ^bytes (wire-bytes message))
  (.flush out))

(defn respond! [out request body]
  (send! out {:seq 1
              :type "response"
              :request_seq (:seq request)
              :command (:command request)
              :success true
              :body body}))

(defn reject! [out request message]
  (send! out {:seq 1
              :type "response"
              :request_seq (:seq request)
              :command (:command request)
              :success false
              :message message}))

(defn event! [out event body]
  (send! out {:seq 1 :type "event" :event event :body body}))

(defmacro with-server
  "Runs the body with a loopback DAP server, binding port and requests.

  The handler receives requests after initialization. The server, its connection,
  and the adapter task are closed or joined when the body exits."
  [handler & body]
  `(with-open [server# (ServerSocket. 0 1 (InetAddress/getLoopbackAddress))]
     (let [handler# ~handler
           requests# (atom [])
           connection# (atom nil)
           adapter#
           (future/io
             (try
               (with-open [socket# (.accept server#)]
                 (reset! connection# socket#)
                 (let [in# (DataInputStream. (.getInputStream socket#))
                       out# (.getOutputStream socket#)]
                   (try
                     (loop [attach# nil configured# false]
                       (let [request# (receive! in#)
                             command# (:command request#)]
                         (swap! requests# conj request#)
                         (case command#
                           "initialize" (respond! out# request# {:supportsConfigurationDoneRequest true})
                           "attach" (event! out# "initialized" {})
                           "configurationDone" (do (respond! out# request# {}) (respond! out# attach# {}))
                           "setBreakpoints" (if configured#
                                              (handler# request# in# out# socket#)
                                              (respond! out# request# {:breakpoints []}))
                           "disconnect" (do (respond! out# request# {}) (event! out# "terminated" {}))
                           (handler# request# in# out# socket#))
                         (when-not (or (= "disconnect" command#) (.isClosed socket#))
                           (recur (if (= "attach" command#) request# attach#)
                                  (or configured# (= "configurationDone" command#))))))
                     (catch EOFException _#)
                     (catch SocketException exception#
                       (when-not (.isClosed socket#) (throw exception#))))))
               (catch SocketException exception#
                 (when-not (.isClosed server#) (throw exception#)))))]
       (try
         (let [~'port (.getLocalPort server#)
               ~'requests requests#]
           ~@body)
         (finally
           (when-let [^Socket socket# @connection#] (.close socket#))
           (.close server#)
           (await! adapter#))))))

(defmacro with-adapter
  "Runs the body with a connected DAP session, binding session and events.

  Also binds port and requests from with-server. Options are passed to
  connect!; callbacks override the default event collectors. The session
  is closed when the body exits."
  [local-root options handler & body]
  `(with-server
     ~handler
     (let [ready# (future/make)
           ~'events (LinkedBlockingQueue.)
           ~'session
           (dap/connect! "127.0.0.1" (constantly ~'port) ~local-root
                         :on-connected (fn [session# _#]
                                         (future/complete! ready# session#))
                         :on-suspended (fn [_# snapshot# body#] (.add ~'events [:stopped (dap/suspension snapshot#) body#]))
                         :on-resumed (fn [_# _snapshot#] (.add ~'events [:continued]))
                         :on-output (fn [_# _snapshot# body#] (.add ~'events [:output body#]))
                         :on-closed (fn [_# _snapshot#] (.add ~'events [:closed]))
                         :on-error (fn [_# _snapshot# exception#]
                                     (future/fail! ready# exception#)
                                     (.add ~'events [:error exception#]))
                         ~options)]
       (try
         (is (identical? ~'session (await! ready#)))
         ~@body
         (finally
           (await! (future/io (dap/close! ~'session))))))))
