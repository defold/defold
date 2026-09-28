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

(ns editor.debugging.dap
  (:require [clojure.core.async :as a]
            [clojure.data.json :as json]
            [clojure.set :as set]
            [clojure.string :as string]
            [editor.error-reporting :as error-reporting]
            [editor.future :as future]
            [util.coll :as coll])
  (:import [java.io BufferedInputStream EOFException IOException InputStream OutputStream]
           [java.net InetSocketAddress Socket SocketException]
           [java.nio.charset StandardCharsets]
           [java.util.concurrent ExecutionException]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(def ^:private request-timeout-ms 10000)
(def ^:private ^:const max-message-size (* 1024 1024))

(defn- read-message! [^InputStream in]
  (let [header (StringBuilder.)
        content-length
        (loop []
          (let [ch (.read in)]
            (when (= -1 ch)
              (throw (EOFException. "Debugger disconnected")))
            (.append header (char ch))
            (when (> (.length header) 4096)
              (throw (IOException. "Debugger message header is too large")))
            (if-not (and (>= (.length header) 4)
                         (= "\r\n\r\n" (.substring header (- (.length header) 4))))
              (recur)
              (let [lengths (into []
                                  (keep #(second (re-matches #"(?i)Content-Length:\s*(\d+)" %)))
                                  (string/split (.toString header) #"\r\n"))]
                (when-not (= 1 (count lengths))
                  (throw (IOException. "Expected one Content-Length header from debugger")))
                (let [length (Long/parseLong (first lengths))]
                  (when-not (<= 1 length max-message-size)
                    (throw (IOException. "Invalid debugger message size")))
                  length)))))
        bytes (.readNBytes in (int content-length))]
    (when-not (= content-length (alength bytes))
      (throw (EOFException. "Incomplete debugger message")))
    (let [message (json/read-str (String. bytes StandardCharsets/UTF_8) :key-fn keyword)]
      (when-not (and (map? message) (string? (:type message)))
        (throw (IOException. "Invalid debugger message")))
      message)))

(defn- write-message! [^OutputStream out message]
  (let [bytes (.getBytes (json/write-str message) StandardCharsets/UTF_8)
        header (.getBytes (str "Content-Length: " (alength bytes) "\r\n\r\n") StandardCharsets/US_ASCII)]
    (when (> (alength bytes) max-message-size)
      (throw (IOException. "Debugger request is too large")))
    (doto out
      (.write header)
      (.write bytes)
      (.flush))))

(defn status [session]
  (:status @(:state session)))

(defn suspension [session]
  (let [snapshot @(:state session)]
    (when (= :suspended (:status snapshot))
      (select-keys snapshot [:generation :thread-id]))))

(defn- notify! [session callback & args]
  (when-let [f (get (:callbacks session) callback)]
    (a/put! (:notifications session) #(apply f session args))))

(defn- close-session! [session exception]
  ;; Only the protocol loop closes the session. Clear related fields together
  ;; before completing requests, since their callers can immediately run again.
  (let [[old] (swap-vals! (:state session) assoc :status :closed :socket nil :pending {})]
    (when-not (= :closed (:status old))
      (when-let [^Socket socket (:socket old)]
        (try
          (.close socket)
          (catch IOException _)))
      (let [error (or exception (IOException. "Debugger disconnected"))]
        (future/fail! (:initialized session) error)
        (doseq [[_ response] (:pending old)]
          (future/fail! response error)))
      (when exception
        (notify! session :on-error exception))
      (notify! session :on-closed)
      (a/close! (:outgoing session))
      (a/close! (:notifications session))
      (a/close! (:protocol session)))))

(defn- fail! [session exception]
  (a/>!! (:protocol session) [:fail exception]))

(defn- await-response! [session command response]
  (let [result (try
                 (deref response request-timeout-ms ::timeout)
                 (catch ExecutionException exception
                   (throw (.getCause exception))))]
    (if-not (= ::timeout result)
      result
      (let [exception (IOException. (str "Debugger request timed out: " command))]
        (fail! session exception)
        (throw exception)))))

(defn close!
  "Close once, cancel pending requests, and release any suspended Lua execution.
  Blocks until the protocol loop has closed the session. Call off the UI thread."
  [session]
  (let [closed (future/make)]
    (when (a/>!! (:protocol session) [:close closed])
      (await-response! session "close" closed))))

(defn- send-request! [session command arguments]
  (let [response (future/make)]
    (when-not (a/>!! (:protocol session) [:request command arguments response])
      (future/fail! response (IOException. "Debugger disconnected")))
    response))

(defn request!
  "Make a blocking DAP request. Call off the UI and protocol threads."
  [session command arguments]
  (await-response! session command (send-request! session command arguments)))

(defn- handle-event! [session {:keys [event body]}]
  (case event
    "initialized"
    (future/complete! (:initialized session) true)

    "stopped"
    (do
      (swap! (:state session)
             #(-> %
                  (assoc :status :suspended :thread-id (:threadId body))
                  (update :generation inc)))
      (notify! session :on-suspended body))

    "continued"
    (do
      (swap! (:state session)
             #(-> %
                  (assoc :status :running)
                  (update :generation inc)))
      (notify! session :on-resumed))

    "output"
    (notify! session :on-output body)

    "invalidated"
    (notify! session :on-invalidated body)

    ("terminated" "exited")
    (close-session! session nil)

    nil))

(defn- read-messages! [^InputStream in protocol]
  (future/io
    (try
      (loop []
        (when (a/>!! protocol [:message (read-message! in)])
          (recur)))
      (catch EOFException exception
        (a/>!! protocol [:eof exception]))
      (catch SocketException exception
        (a/>!! protocol [:eof exception]))
      (catch Exception exception
        (a/>!! protocol [:fail exception])))))

(defn- write-messages! [^OutputStream out outgoing protocol]
  (future/io
    (try
      (loop []
        (when-let [message (a/<!! outgoing)]
          (write-message! out message)
          (recur)))
      (catch Exception exception
        (a/>!! protocol [:fail exception])))))

(defn- handle-message! [session {:keys [type request_seq command success body message] :as response}]
  (when-not (= :closed (status session))
    (case type
      "response"
      (let [[old] (swap-vals! (:state session) update :pending dissoc request_seq)]
        (when-let [pending (get-in old [:pending request_seq])]
          (if success
            (future/complete! pending body)
            (future/fail! pending (IOException. (str (or message (str "Debugger request failed: " command))))))))

      "event"
      (handle-event! session response)

      "request"
      (let [state (swap! (:state session) update :next-seq inc)]
        (a/put! (:outgoing session)
                {:seq (:next-seq state)
                 :type "response"
                 :request_seq (:seq response)
                 :command command
                 :success false
                 :message "Client request is not supported"}))

      (throw (IOException. "Unknown debugger message type")))))

(defn- run-protocol! [session]
  (future/io
    (loop []
      (when-let [[operation x y response] (a/<!! (:protocol session))]
        (try
          (case operation
            :close
            (do
              (close-session! session nil)
              (future/complete! x nil))

            :socket
            (if (= :closed (status session))
              (do
                (.close ^Socket x)
                (future/fail! y (IOException. "Debugger connection cancelled")))
              (do
                (swap! (:state session) assoc :socket x)
                (future/complete! y nil)))

            :request
            (if (= :closed (status session))
              (future/fail! response (IOException. "Debugger disconnected"))
              (let [state
                    (swap! (:state session)
                           (fn [state]
                             (let [sequence-number (inc (long (:next-seq state)))]
                               (-> state
                                   (assoc :next-seq sequence-number)
                                   (assoc-in [:pending sequence-number] response)))))]
                (a/put! (:outgoing session)
                        {:seq (:next-seq state)
                         :type "request"
                         :command x
                         :arguments y})))

            :connected
            (do
              (when-not (= :closed (status session))
                ;; configurationDone may already have produced a stopped event.
                (swap! (:state session)
                       #(cond-> %
                          (= :connecting (:status %))
                          (assoc :status :running)))
                (notify! session :on-connected))
              (future/complete! x nil))

            :fail
            (close-session! session x)

            :eof
            (close-session! session (when (= :connecting (status session)) x))

            :message
            (handle-message! session x))
          (catch Exception exception
            (close-session! session exception)))
        (recur)))))

(defn- sync-breakpoints! [{:keys [breakpoint-lock] :as session}]
  ;; setBreakpoints replaces a source's complete set, including an empty set
  ;; when its last breakpoint is removed. Read the latest desired set in the lock.
  (locking breakpoint-lock
    (let [{old :breakpoints new :desired-breakpoints} @(:state session)]
      (doseq [path (set/union (set (coll/keys old)) (set (coll/keys new)))
              :let [breakpoints (get new path [])]
              :when (not= (get old path []) breakpoints)]
        (request! session "setBreakpoints"
                  {:source {:path path}
                   :breakpoints breakpoints}))
      (swap! (:state session) assoc :breakpoints new))))

(defn set-breakpoints!
  "Replace breakpoints, grouped by project path. Blocks until synchronized when connected."
  [session breakpoints]
  (swap! (:state session) assoc :desired-breakpoints breakpoints)
  (when (#{:running :suspended} (status session))
    (try
      (sync-breakpoints! session)
      (catch Exception exception
        (fail! session exception)
        (throw exception)))))

(defn- connect-socket!
  ^Socket [session address resolve-port]
  (let [deadline (+ (System/nanoTime) (* 1000000 (long request-timeout-ms)))]
    (loop []
      (when (= :closed (status session))
        (throw (IOException. "Debugger connection cancelled")))
      (let [port (resolve-port)
            socket (Socket.)
            registered (future/make)]
        (when-not (a/>!! (:protocol session) [:socket socket registered])
          (.close socket)
          (throw (IOException. "Debugger connection cancelled")))
        (await-response! session "connect" registered)
        (let [error (try
                      (when-not port
                        (throw (IOException. "Waiting for the engine's DAP listener")))
                      (.connect socket (InetSocketAddress. ^String address (int port)) 1000)
                      (.setTcpNoDelay socket true)
                      nil
                      (catch IOException exception
                        (.close socket)
                        exception))]
          (if-not error
            socket
            (if (>= (System/nanoTime) deadline)
              (throw (IOException. (str "Failed to connect to DAP debugger on " address
                                        (when port (str ":" port))) error))
              (do
                (Thread/sleep 100)
                (recur)))))))))

(defn connect!
  "Connect and configure asynchronously, returning a session immediately.
  Callbacks run in order on a separate thread and may make blocking DAP requests."
  [address resolve-port {:keys [local-root breakpoints stop-on-entry target]} callbacks]
  (let [session
        {:state (atom {:status :connecting
                       :generation 0
                       :socket nil
                       :next-seq 0
                       :pending {}
                       :breakpoints {}
                       :desired-breakpoints breakpoints})
         :initialized (future/make)
         :protocol (a/chan 128)
         :outgoing (a/chan 128)
         :notifications (a/chan 128)
         :breakpoint-lock (Object.)
         :callbacks callbacks
         :local-root local-root
         :target target}]
    (run-protocol! session)
    (future/io
      (loop []
        (when-let [callback (a/<!! (:notifications session))]
          (error-reporting/catch-all! (callback))
          (recur))))
    (future/io
      (try
        (let [socket (connect-socket! session address resolve-port)]
          (read-messages! (BufferedInputStream. (.getInputStream socket)) (:protocol session))
          (write-messages! (.getOutputStream socket) (:outgoing session) (:protocol session)))
        (let [capabilities
              (request! session "initialize"
                        {:clientID "defold"
                         :clientName "Defold Editor"
                         :adapterID "defold"
                         :pathFormat "path"
                         :linesStartAt1 true
                         :columnsStartAt1 true
                         :supportsVariableType true
                         :supportsInvalidatedEvent true})

              attach-response
              (send-request! session "attach"
                             {:localRoot local-root
                              :stopOnEntry (boolean stop-on-entry)})]
          (await-response! session "initialized" (:initialized session))
          (sync-breakpoints! session)
          (when (:supportsConfigurationDoneRequest capabilities)
            (request! session "configurationDone" {}))
          (await-response! session "attach" attach-response)
          (let [connected (future/make)]
            (when (a/>!! (:protocol session) [:connected connected])
              (await-response! session "connected" connected)
              (sync-breakpoints! session))))
        (catch Exception exception
          (fail! session exception))))
    session))

(defn disconnect! [session]
  (try
    (when (#{:running :suspended} (status session))
      (request! session "disconnect" {:terminateDebuggee false}))
    (finally
      (close! session))))

(defn control! [session command]
  (let [thread-id (or (when-not (= "pause" command) (:thread-id @(:state session)))
                      (:id (first (:threads (request! session "threads" {})))))]
    (request! session command {:threadId thread-id})))

(defn- source-path->project-path [local-root path]
  (when path
    (let [path (string/replace path "\\" "/")
          root (string/replace local-root "\\" "/")
          root (string/replace root #"/+$" "")
          ;; Native DAP normalizes Windows drive letters to lower case.
          normalized-path (string/replace path #"^[A-Z]:" string/lower-case)
          normalized-root (string/replace root #"^[A-Z]:" string/lower-case)]
      (if-not (string/starts-with? normalized-path (str normalized-root "/"))
        path
        (subs path (count root))))))

(defn stack [session snapshot]
  (when (and snapshot (= snapshot (suspension session)))
    (let [thread-id (or (:thread-id snapshot)
                        (:id (first (:threads (request! session "threads" {})))))
          frames (:stackFrames (request! session "stackTrace" {:threadId thread-id}))]
      (when (= snapshot (suspension session))
        (mapv (fn [{:keys [id name source line]}]
                {:id id
                 :function name
                 :file (source-path->project-path (:local-root session) (:path source))
                 :line line})
              frames)))))

(defn variables [session snapshot reference]
  (when (and snapshot (= snapshot (suspension session)))
    (let [result (:variables (request! session "variables" {:variablesReference reference}))]
      (when (= snapshot (suspension session))
        result))))

(defn frame-variables [session snapshot frame-id]
  (when (and snapshot (= snapshot (suspension session)))
    (let [scopes (:scopes (request! session "scopes" {:frameId frame-id}))
          ;; Keep locals/upvalues flat and fetch globals only when _G is expanded.
          result
          (into []
                (mapcat (fn [{:keys [name variablesReference expensive]}]
                          (cond
                            (= "Globals" name)
                            [{:name "_G"
                              :value "table"
                              :type "table"
                              :variablesReference variablesReference}]

                            expensive
                            []

                            :else
                            (variables session snapshot variablesReference))))
                scopes)]
      (when (= snapshot (suspension session))
        result))))

(defn evaluate! [session frame-id expression]
  (request! session "evaluate"
            (cond-> {:expression expression
                     :context "repl"}
              frame-id
              (assoc :frameId frame-id))))
