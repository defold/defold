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
  (:require [clojure.data.json :as json]
            [clojure.set :as set]
            [clojure.string :as string]
            [editor.future :as future]
            [util.coll :as coll])
  (:import [java.io BufferedInputStream EOFException IOException InputStream OutputStream]
           [java.net InetSocketAddress Socket SocketException]
           [java.nio.charset StandardCharsets]))

(set! *warn-on-reflection* true)

(def ^:private request-timeout-ms 10000)
(def ^:private max-message-size (* 1024 1024))
(def ^:private max-header-size 4096)

(defn- read-message! [^InputStream in]
  (let [header (StringBuilder.)
        content-length
        (loop []
          (let [ch (.read in)]
            (when (= -1 ch)
              (throw (EOFException. "Debugger disconnected")))
            (.append header (char ch))
            (when (> (.length header) max-header-size)
              (throw (IOException. "Debugger message header is too large")))
            (if (and (>= (.length header) 4)
                     (= "\r\n\r\n" (.substring header (- (.length header) 4))))
              (let [lengths (into []
                                  (keep #(second (re-matches #"(?i)Content-Length:\s*(\d+)" %)))
                                  (string/split (.toString header) #"\r\n"))]
                (when-not (= 1 (count lengths))
                  (throw (IOException. "Expected one Content-Length header from debugger")))
                (let [length (Long/parseLong (first lengths))]
                  (when-not (<= 1 length max-message-size)
                    (throw (IOException. "Invalid debugger message size")))
                  length))
              (recur))))
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

(defn state [session]
  (:status @(:state session)))

(defn suspension [session]
  (let [snapshot @(:state session)]
    (when (= :suspended (:status snapshot))
      (select-keys snapshot [:generation :thread-id]))))

(defn- notify! [session callback & args]
  (when-let [f (get (:callbacks session) callback)]
    (apply f session args)))

(defn close!
  "Close once, cancel pending requests, and release any suspended Lua execution."
  [{:keys [write-lock] :as session}]
  (locking write-lock
    (let [[old] (swap-vals! (:state session) assoc :status :closed)]
      (when-not (= :closed (:status old))
        (when-let [^Socket socket @(:socket session)]
          (try (.close socket) (catch IOException _)))
        (let [error (IOException. "Debugger disconnected")
              [pending] (reset-vals! (:pending session) {})]
          (deliver (:initialized session) error)
          (doseq [[_ response] pending]
            (deliver response error)))
        (notify! session :on-closed)))))

(defn- fail! [{:keys [write-lock] :as session} exception]
  (locking write-lock
    (when-not (= :closed (state session))
      (notify! session :on-error exception)
      (close! session))))

(defn- send-request! [{:keys [write-lock] :as session} command arguments]
  (locking write-lock
    (when (= :closed (state session))
      (throw (IOException. "Debugger disconnected")))
    (let [sequence-number (swap! (:next-seq session) inc)
          response (promise)]
      (swap! (:pending session) assoc sequence-number response)
      (try
        (write-message! (.getOutputStream ^Socket @(:socket session))
                        {:seq sequence-number
                         :type "request"
                         :command command
                         :arguments arguments})
        [sequence-number response]
        (catch Exception exception
          (fail! session exception)
          (throw exception))))))

(defn- await-response! [session command [sequence-number response]]
  (let [result (deref response request-timeout-ms ::timeout)]
    (swap! (:pending session) dissoc sequence-number)
    (cond
      (= ::timeout result)
      (let [exception (IOException. (str "Debugger request timed out: " command))]
        (fail! session exception)
        (throw exception))

      (instance? Throwable result)
      (throw result)

      (:success result)
      (:body result)

      :else
      (throw (ex-info (or (:message result) (str "Debugger request failed: " command))
                      {:command command :response result})))))

(defn request!
  "Make a blocking DAP request. Call off the UI and reader threads."
  [session command arguments]
  (await-response! session command (send-request! session command arguments)))

(defn- handle-event! [{:keys [write-lock] :as session} {:keys [event body]}]
  (locking write-lock
    (when-not (= :closed (state session))
      (case event
        "initialized" (deliver (:initialized session) true)
        "stopped" (do
                    (swap! (:state session)
                           #(-> %
                                (assoc :status :suspended :thread-id (:threadId body))
                                (update :generation inc)))
                    (notify! session :on-suspended body))
        "continued" (do
                      (swap! (:state session)
                             #(-> % (assoc :status :running) (update :generation inc)))
                      (notify! session :on-resumed))
        "output" (notify! session :on-output body)
        "invalidated" (notify! session :on-invalidated body)
        ("terminated" "exited") (close! session)
        nil))))

(defn- read-messages! [{:keys [write-lock] :as session} ^Socket socket]
  (future/io
    (try
      (let [in (BufferedInputStream. (.getInputStream socket))]
        (loop []
          (when-not (= :closed (state session))
            (let [message (read-message! in)]
              (case (:type message)
                "response" (when-let [response (get @(:pending session) (:request_seq message))]
                             (deliver response message))
                "event" (handle-event! session message)
                "request" (locking write-lock
                            (write-message! (.getOutputStream socket)
                                            {:seq (swap! (:next-seq session) inc)
                                             :type "response"
                                             :request_seq (:seq message)
                                             :command (:command message)
                                             :success false
                                             :message "Client request is not supported"}))
                (throw (IOException. "Unknown debugger message type"))))
            (recur))))
      (catch EOFException exception
        (if (= :connecting (state session))
          (fail! session exception)
          (close! session)))
      (catch SocketException exception
        (if (= :connecting (state session))
          (fail! session exception)
          (close! session)))
      (catch Exception exception (fail! session exception)))))

(defn- sync-breakpoints! [{:keys [breakpoint-lock] :as session}]
  ;; setBreakpoints replaces a source's complete set, including an empty set
  ;; when its last breakpoint is removed. Always read the latest desired set
  ;; inside the lock so queued UI updates cannot restore an older snapshot.
  (locking breakpoint-lock
    (let [old @(:breakpoints session)
          new @(:desired-breakpoints session)]
      (doseq [path (set/union (set (coll/keys old)) (set (coll/keys new)))
              :let [breakpoints (get new path [])]
              :when (not= (get old path []) breakpoints)]
        (request! session "setBreakpoints" {:source {:path path} :breakpoints breakpoints}))
      (reset! (:breakpoints session) new))))

(defn set-breakpoints!
  "Asynchronously replace breakpoints, grouped by project path. Works while running."
  [session breakpoints]
  (reset! (:desired-breakpoints session) breakpoints)
  (future/io
    (when (#{:running :suspended} (state session))
      (try
        (sync-breakpoints! session)
        (catch Exception exception (fail! session exception))))))

(defn- connect-socket! [session address resolve-port]
  (let [deadline (+ (System/nanoTime) (* 1000000 request-timeout-ms))]
    (loop []
      (when (= :closed (state session))
        (throw (IOException. "Debugger connection cancelled")))
      (let [port (resolve-port)
            socket (Socket.)
            _ (reset! (:socket session) socket)
            error (try
                    (when-not port
                      (throw (IOException. "Waiting for the engine's DAP listener")))
                    (.connect socket (InetSocketAddress. ^String address (int port)) 1000)
                    (.setTcpNoDelay socket true)
                    nil
                    (catch IOException exception
                      (.close socket)
                      exception))]
        (if-not error
          (if (= :closed (state session))
            (do (.close socket) (throw (IOException. "Debugger connection cancelled")))
            socket)
          (if (>= (System/nanoTime) deadline)
            (throw (IOException. (str "Failed to connect to DAP debugger on " address
                                     (when port (str ":" port))) error))
            (do (Thread/sleep 100) (recur))))))))

(defn connect!
  "Connect and configure asynchronously. Callbacks must not block the reader.
  Returns a session immediately, including during connection retries."
  [address resolve-port {:keys [local-root breakpoints stop-on-entry target]} callbacks]
  (let [session {:socket (atom nil)
                 :state (atom {:status :connecting :generation 0})
                 :next-seq (atom 0)
                 :pending (atom {})
                 :initialized (promise)
                 :write-lock (Object.)
                 :breakpoint-lock (Object.)
                 :breakpoints (atom {})
                 :desired-breakpoints (atom breakpoints)
                 :callbacks callbacks
                 :local-root local-root
                 :target target}]
    (future/io
      (try
        (read-messages! session (connect-socket! session address resolve-port))
        (let [capabilities (request! session "initialize"
                                     {:clientID "defold"
                                      :clientName "Defold Editor"
                                      :adapterID "defold"
                                      :pathFormat "path"
                                      :linesStartAt1 true
                                      :columnsStartAt1 true
                                      :supportsVariableType true
                                      :supportsInvalidatedEvent true})
              attach-response (send-request! session "attach" {:localRoot local-root
                                                                :stopOnEntry (boolean stop-on-entry)})
              initialized (deref (:initialized session) request-timeout-ms ::timeout)]
          (when-not (true? initialized)
            (throw (if (instance? Throwable initialized)
                     initialized
                     (IOException. "Timed out waiting for DAP initialization"))))
          (sync-breakpoints! session)
          (when (:supportsConfigurationDoneRequest capabilities)
            (request! session "configurationDone" {}))
          (await-response! session "attach" attach-response)
          ;; A stopped event may already have arrived after configurationDone.
          (swap! (:state session) #(if (= :connecting (:status %)) (assoc % :status :running) %))
          (when-not (= :closed (state session))
            (sync-breakpoints! session)
            (notify! session :on-connected)))
        (catch Exception exception (fail! session exception))))
    session))

(defn disconnect! [session]
  (future/io
    (try
      (when (#{:running :suspended} (state session))
        (request! session "disconnect" {:terminateDebuggee false}))
      (finally (close! session)))))

(defn control! [session command]
  (future/io
    (try
      (let [thread-id (or (when-not (= "pause" command) (:thread-id @(:state session)))
                          (:id (first (:threads (request! session "threads" {})))))]
        (request! session command {:threadId thread-id}))
      (catch Exception exception
        (when-not (= :closed (state session))
          (notify! session :on-error exception))))))

(defn- source-path->project-path [local-root path]
  (when path
    (let [path (string/replace path "\\" "/")
          root (string/replace local-root "\\" "/")
          root (string/replace root #"/+$" "")
          ;; Native DAP normalizes Windows drive letters to lower case.
          normalized-path (string/replace path #"^[A-Z]:" string/lower-case)
          normalized-root (string/replace root #"^[A-Z]:" string/lower-case)]
      (if (string/starts-with? normalized-path (str normalized-root "/"))
        (subs path (count root))
        path))))

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
    (let [scopes (:scopes (request! session "scopes" {:frameId frame-id}))]
      ;; Preserve the flat locals/upvalues view. Large global scopes remain
      ;; available through console evaluation without eagerly downloading them.
      (into []
            (comp (remove :expensive)
                  (mapcat #(variables session snapshot (:variablesReference %))))
            scopes))))

(defn evaluate! [session frame-id expression]
  (request! session "evaluate" (cond-> {:expression expression :context "repl"}
                                 frame-id (assoc :frameId frame-id))))
