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
            [clojure.string :as string]
            [editor.error-reporting :as error-reporting]
            [editor.future :as future]
            [editor.ui :as ui]
            [util.coll :as coll]
            [util.defonce :as defonce]
            [util.task :as task])
  (:import [clojure.lang IDeref]
           [java.io BufferedInputStream IOException InputStream OutputStream]
           [java.net InetSocketAddress Socket SocketException]
           [java.nio.charset StandardCharsets]
           [java.util.concurrent CompletableFuture ExecutionException LinkedBlockingQueue TimeUnit TimeoutException]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(def ^:private request-timeout-ms 10000)
(def ^:private ^:const max-message-size (* 1024 1024))

(defn- write-message! [^OutputStream out message]
  (let [bytes (.getBytes (json/write-str message) StandardCharsets/UTF_8)
        header (.getBytes (str "Content-Length: " (alength bytes) "\r\n\r\n") StandardCharsets/US_ASCII)]
    (when (> (alength bytes) max-message-size)
      (throw (IOException. "Debugger request is too large")))
    (doto out
      (.write header)
      (.write bytes)
      (.flush))))

(defn close!
  [session]
  (assert (not (ui/on-ui-thread?)))
  (when-let [transport (:transport (first (swap-vals! (:data session) dissoc :transport)))]
    (future/complete! (:stop-requested transport) nil))
  (try
    (.get ^CompletableFuture (:ended session))
    (catch ExecutionException _))
  nil)

(defonce/record Session [data ended]
  IDeref
  (deref [_] (:debugger @data)))

(defn status [session]
  (:status @session))

(defn suspension [state]
  (when (= :suspended (:status state))
    (select-keys state [:generation :threadId])))

(defn- notify! [{:keys [session] :as state} callback & args]
  (when-let [f (get state callback)]
    (let [snapshot @session]
      (ui/run-later (error-reporting/catch-all! (apply f session snapshot args))))))

(defn- await-response! [session command ^CompletableFuture response]
  (try
    (.get (CompletableFuture/anyOf (into-array CompletableFuture [response (:ended session)]))
          (long request-timeout-ms) 
          TimeUnit/MILLISECONDS)
    (catch ExecutionException exception (throw (.getCause exception)))
    (catch TimeoutException _
      (let [exception (IOException. (str "Debugger request timed out: " command))]
        (when-let [transport (:transport @(:data session))]
          (future/complete! (:stop-requested transport) exception))
        (throw exception)))))

(defn- request-operation [command arguments response]
  (fn [{:keys [out next-seq] :as state}]
    (let [id (inc (long next-seq))]
      (write-message! out {:seq id :type "request" :command command :arguments arguments})
      (-> state (assoc :next-seq id) (assoc-in [:pending id] response)))))

(defn- enqueue-request! [{:keys [inbox]} command arguments]
  (let [response (future/make)]
    (.add ^LinkedBlockingQueue inbox (request-operation command arguments response))
    response))

(defn- send-request! [session command arguments]
  (if-let [^LinkedBlockingQueue work (get-in @(:data session) [:transport :work])]
    (let [response (future/make)
          operation (request-operation command arguments response)]
      (.add work
            (fn [{:keys [inbox] :as state}]
              (.add ^LinkedBlockingQueue inbox operation)
              state))
      response)
    (future/failed (IOException. "Debugger disconnected"))))

(defn- request! [session command arguments]
  (await-response! session command (send-request! session command arguments)))

(defn- protocol-request! [{:keys [session] :as state} command arguments]
  (await-response! session command (enqueue-request! state command arguments)))

(defn- handle-event! [{:keys [session initialized stop-requested] :as state} {:keys [event body]}]
  (case event
    "initialized" (future/complete! initialized nil)
    "stopped" (do
                (swap! (:data session) update :debugger
                       #(-> %
                            (assoc :status :suspended :threadId (:threadId body))
                            (update :generation inc)))
                (notify! state :on-suspended body))
    "continued" (do
                  (swap! (:data session) update :debugger
                         #(-> % (assoc :status :running) (update :generation inc)))
                  (notify! state :on-resumed))
    "output" (notify! state :on-output body)
    "invalidated" (notify! state :on-invalidated body)
    ("terminated" "exited") (future/complete! stop-requested nil)
    nil)
  state)

(defn- handle-message! [{:keys [out] :as state} {:keys [type request_seq command success body message] :as response}]
  (case type
    "response"
    (do
      (when-let [pending (get-in state [:pending request_seq])]
        (if success
          (future/complete! pending body)
          (future/fail! pending (IOException. (str (or message (str "Debugger request failed: " command)))))))
      (update state :pending dissoc request_seq))

    "event"
    (handle-event! state response)

    "request"
    (let [state (update state :next-seq inc)]
      (write-message! out {:seq (:next-seq state)
                           :type "response"
                           :request_seq (:seq response)
                           :command command
                           :success false
                           :message "Client request is not supported"})
      state)

    (throw (IOException. "Invalid debugger message"))))

(defn- read-messages! [^LinkedBlockingQueue inbox ^InputStream in]
  (let [failure
        (try
          (loop []
            (let [header (StringBuilder.)
                  complete
                  (loop []
                    (let [ch (.read in)]
                      (if (= -1 ch)
                        (when (pos? (.length header))
                          (throw (IOException. "Invalid debugger message")))
                        (do
                          (.append header (char ch))
                          (when (> (.length header) 4096)
                            (throw (IOException. "Invalid debugger message")))
                          (if (and (>= (.length header) 4)
                                   (= "\r\n\r\n" (.substring header (- (.length header) 4))))
                            true
                            (recur))))))]
              (when complete
                (let [lengths (into []
                                    (keep #(second (re-matches #"(?i)Content-Length:\s*(.*?)\s*" %)))
                                    (string/split (.toString header) #"\r\n"))
                      length (when (= 1 (count lengths)) (parse-long (first lengths)))]
                  (when-not (and length (<= 1 (long length) max-message-size))
                    (throw (IOException. "Invalid debugger message")))
                  (let [bytes (.readNBytes in (int length))]
                    (when-not (= (long length) (alength bytes))
                      (throw (IOException. "Invalid debugger message")))
                    (let [message (json/read-str (String. bytes StandardCharsets/UTF_8) :key-fn keyword)]
                      (when-not (and (map? message) (string? (:type message)))
                        (throw (IOException. "Invalid debugger message")))
                      (.add inbox #(handle-message! % message)))))
                (recur))))
          (catch SocketException _ nil)
          (catch Throwable exception exception))]
    (when-not (.isInterrupted (Thread/currentThread))
      (.add inbox
            (fn [{:keys [session stop-requested]}]
              (when failure (throw failure))
              (if (= :connecting (status session))
                (throw (IOException. "Debugger disconnected"))
                (future/complete! stop-requested nil))
              nil)))))

(defn- run-operations! [^LinkedBlockingQueue queue state]
  (loop [state state]
    (when-let [state ((.take queue) state)]
      (recur state))))

(defn- sync-breakpoints! [{:keys [session breakpoints] :as state}]
  (let [desired (:desired-breakpoints @session)]
    (doseq [path (into (set (coll/keys breakpoints)) (coll/keys desired))
            :let [new (get desired path [])]
            :when (not= (get breakpoints path []) new)]
      (protocol-request! state "setBreakpoints" {:source {:path path} :breakpoints new}))
    (assoc state :breakpoints desired)))

(defn set-breakpoints!
  [session breakpoints]
  (let [data (swap! (:data session)
                    #(if-not (:transport %) % (assoc-in % [:debugger :desired-breakpoints] breakpoints)))]
    (when-let [^LinkedBlockingQueue work (get-in data [:transport :work])]
      (.add work sync-breakpoints!)))
  session)

(defn- initialize! [{:keys [session inbox initialized local-root stop-on-entry] :as state}]
  (let [capabilities
        (protocol-request! state "initialize"
                           {:clientID "defold"
                            :clientName "Defold Editor"
                            :adapterID "defold"
                            :pathFormat "path"
                            :linesStartAt1 true
                            :columnsStartAt1 true
                            :supportsVariableType true
                            :supportsInvalidatedEvent true})
        attach (enqueue-request! state "attach"
                                 {:localRoot local-root :stopOnEntry (boolean stop-on-entry)})]
    (await-response! session "initialized" initialized)
    (let [state
          (loop [state (assoc state :breakpoints {})]
            (if (= (:breakpoints state) (:desired-breakpoints @session))
              state
              (recur (sync-breakpoints! state))))]
      (when (:supportsConfigurationDoneRequest capabilities)
        (protocol-request! state "configurationDone" {}))
      (await-response! session "attach" attach)
      (.add ^LinkedBlockingQueue inbox
            (fn [{:keys [session] :as state}]
              (swap! (:data session) update :debugger
                     #(cond-> % (= :connecting (:status %)) (assoc :status :running)))
              (notify! state :on-connected)
              state))
      state)))

(defn- connect-socket!
  ^Socket [address resolve-port]
  (let [deadline (+ (System/nanoTime) (* 1000000 (long request-timeout-ms)))]
    (loop []
      (let [port (resolve-port)
            socket (Socket.)
            error (try
                    (when port
                      (.connect socket (InetSocketAddress. ^String address (int port)) 1000)
                      (.setTcpNoDelay socket true))
                    nil
                    (catch IOException exception exception)
                    (catch Throwable exception (.close socket) (throw exception)))]
        (if (and port (not error))
          socket
          (do
            (.close socket)
            (when (>= (System/nanoTime) deadline)
              (throw (IOException. (str "Failed to connect to DAP debugger on " address
                                        (when port (str ":" port))) error)))
            (Thread/sleep 100)
            (recur)))))))

(defn- run-connection! [{:keys [inbox work] :as state} address resolve-port]
  (task/with-open [socket (connect-socket! address resolve-port)]
    (let [in (BufferedInputStream. (.getInputStream socket))
          out (.getOutputStream socket)]
      (task/scope :all-successful
        (task/fork (read-messages! inbox in))
        (task/fork (run-operations! inbox (assoc state :out out :next-seq 0 :pending {})))
        (task/fork (run-operations! work (initialize! state))))))
  nil)

(defn- run-session! [{:keys [session stop-requested] :as state} address resolve-port]
  (let [exception
        (try
          (task/scope :first-completed
            (task/fork (.get ^CompletableFuture stop-requested))
            (task/fork (run-connection! state address resolve-port)))
          (catch Throwable exception exception))]
    (swap! (:data session) #(-> %
                                (dissoc :transport)
                                (assoc-in [:debugger :status] :closed)
                                (update :debugger dissoc :threadId)))
    (try
      (when exception (notify! state :on-error exception))
      (notify! state :on-closed)
      (finally
        (future/fail! (:ended session) (or exception (IOException. "Debugger disconnected")))))))

(defn connect!
  [address resolve-port {:keys [local-root breakpoints target] :as options}]
  (let [transport {:inbox (LinkedBlockingQueue.)
                   :work (LinkedBlockingQueue.)
                   :stop-requested (future/make)
                   :initialized (future/make)}
        session (->Session (atom {:debugger {:status :connecting
                                             :generation 0
                                             :local-root local-root
                                             :target target
                                             :desired-breakpoints (or breakpoints {})}
                                  :transport transport})
                           (future/make))]
    (future/io
      (.setName (Thread/currentThread) "dap-session")
      (run-session! (assoc (merge options transport) :session session) address resolve-port))
    session))

(defn disconnect!
  [session]
  (assert (not (ui/on-ui-thread?)))
  (try
    (when (#{:running :suspended} (status session))
      (request! session "disconnect" {:terminateDebuggee false}))
    (finally
      (close! session))))

(defn control! [session command]
  (let [thread-id (or (when-not (= "pause" command) (:threadId @session))
                      (:id (first (:threads (request! session "threads" {})))))]
    (request! session command {:threadId thread-id})))

(defn- source-path->project-path [local-root path]
  (when path
    (let [path (string/replace path "\\" "/")
          root (string/replace local-root "\\" "/")
          root (string/replace root #"/+$" "")
          normalized-path (string/replace path #"^[A-Z]:" string/lower-case)
          normalized-root (string/replace root #"^[A-Z]:" string/lower-case)]
      (if-not (string/starts-with? normalized-path (str normalized-root "/"))
        path
        (subs path (count root))))))

(defn- inspect [session snapshot f]
  (when (and snapshot (= snapshot (suspension @session)))
    (try
      (let [result (f)]
        (when (= snapshot (suspension @session))
          result))
      (catch Exception exception
        (when (= snapshot (suspension @session))
          (throw exception))))))

(defn stack [session snapshot]
  (inspect session snapshot
           (fn []
             (let [thread-id (or (:threadId snapshot)
                                 (:id (first (:threads (request! session "threads" {})))))
                   frames (:stackFrames (request! session "stackTrace" {:threadId thread-id}))]
               (mapv (fn [{:keys [id name source line]}]
                       {:id id
                        :function name
                        :file (source-path->project-path (:local-root @session) (:path source))
                        :line line})
                     frames)))))

(defn variables [session snapshot reference]
  (inspect session snapshot
           #(:variables (request! session "variables" {:variablesReference reference}))))

(defn frame-variables [session snapshot frame-id]
  (inspect session snapshot
           (fn []
             (let [scopes (:scopes (request! session "scopes" {:frameId frame-id}))]
               (into []
                     (mapcat (fn [{:keys [name variablesReference expensive]}]
                               (cond
                                 (= "Globals" name)
                                 [{:name "_G" :value "table" :type "table"
                                   :variablesReference variablesReference}]
                                 expensive []
                                 :else (variables session snapshot variablesReference))))
                     scopes)))))

(defn evaluate! [session frame-id expression]
  (request! session "evaluate"
            (cond-> {:expression expression :context "repl"}
              frame-id (assoc :frameId frame-id))))

(defn- evaluation-value->string-impl
  [session snapshot seen depth {:keys [value variablesReference]}]
  (if (or (zero? (long variablesReference))
          (>= (long depth) 16)
          (contains? @seen variablesReference))
    value
    (do
      (vswap! seen conj variablesReference)
      (let [children (variables session snapshot variablesReference)]
        (if-not children
          value
          (let [indent (.repeat "  " (int depth))
                entries
                (coll/join-to-string
                  ",\n"
                  (eduction
                    (map (fn [{:keys [name] :as child}]
                           (str indent "  " name " = "
                                (evaluation-value->string-impl session snapshot seen (inc (long depth)) child))))
                    children))]
            (str "{ -- " value "\n" entries
                 (when-not (coll/empty? children) "\n")
                 indent "}")))))))

(defn evaluation-result->string
  [session snapshot result]
  (let [output
        (evaluation-value->string-impl session snapshot (volatile! #{}) 0
                                       (assoc result :value (:result result)))]
    (if (= snapshot (suspension @session))
      output
      (:result result))))
