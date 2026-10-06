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
  "Cancel the debugger connection and wait for its tasks to finish.

  Closes the transport and cancels ongoing connection work. Safe to call
  repeatedly. Connection failures are reported through session callbacks.

  Returns nil. Blocks until cleanup completes; call off the UI thread.
  Queued callbacks may still run after this function returns."
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

(defn status
  "Return the session's current status.

  Returns :connecting, :running, :suspended or :closed."
  [session]
  (:status @session))

(defn suspension
  "Return a snapshot identifying a suspension in the supplied debugger state.

  Accepts state obtained by dereferencing a session or received by a callback.
  Returns a map with :generation and any reported :threadId, or nil unless
  the state is suspended.

  Pass this snapshot to inspection functions to discard results from an
  earlier suspension."
  [state]
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

(defn- enqueue-request! [{:keys [^LinkedBlockingQueue protocol-queue]} command arguments]
  (let [response (future/make)]
    (.add protocol-queue (request-operation command arguments response))
    response))

(defn- send-request! [session command arguments]
  (if-let [^LinkedBlockingQueue coordinator-queue (get-in @(:data session) [:transport :coordinator-queue])]
    (let [response (future/make)
          operation (request-operation command arguments response)]
      (.add coordinator-queue
            (fn [{:keys [^LinkedBlockingQueue protocol-queue] :as state}]
              (.add protocol-queue operation)
              state))
      response)
    (future/failed (IOException. "Debugger disconnected"))))

;; External callers enqueue through the coordinator and wait on their own thread.
(defn- request! [session command arguments]
  (await-response! session command (send-request! session command arguments)))

;; The coordinator sends directly to the protocol task: queuing behind its own
;; current operation and waiting for the response would deadlock.
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

(defn- read-messages! [^LinkedBlockingQueue protocol-queue ^InputStream in]
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
                      (.add protocol-queue #(handle-message! % message)))))
                (recur))))
          (catch SocketException _ nil)
          (catch Throwable exception exception))]
    (when-not (.isInterrupted (Thread/currentThread))
      (.add protocol-queue
            (fn [{:keys [session stop-requested]}]
              (when failure (throw failure))
              (if (= :connecting (status session))
                (throw (IOException. "Debugger disconnected"))
                (future/complete! stop-requested nil))
              nil)))))

;; Each task owns its loop state; queued operations return the next state.
;; Shared debugger state lives in the session atom.
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
  "Replace the session's desired breakpoints and queue their synchronization.

  Args:
    session        debugger session
    breakpoints    map from source paths to vectors of DAP breakpoint maps;
                   :line is one-based, and :condition is an optional expression

  An empty vector clears a source's breakpoints. Omitting a previously
  configured source also clears its breakpoints.

  Returns session without waiting for the adapter. Changes made during
  connection setup are included in initialization."
  [session breakpoints]
  (let [data (swap! (:data session)
                    #(if-not (:transport %) % (assoc-in % [:debugger :desired-breakpoints] breakpoints)))]
    (when-let [^LinkedBlockingQueue coordinator-queue (get-in data [:transport :coordinator-queue])]
      (.add coordinator-queue sync-breakpoints!)))
  session)

(defn- initialize! [{:keys [session ^LinkedBlockingQueue protocol-queue initialized local-root stop-on-entry] :as state}]
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
        ;; Attach may need configurationDone before replying. Keep its response
        ;; future while configuring breakpoints, then await it below.
        attach-response (enqueue-request! state "attach"
                                          {:localRoot local-root :stopOnEntry (boolean stop-on-entry)})]
    (await-response! session "initialized" initialized)
    (let [state
          (loop [state (assoc state :breakpoints {})]
            (if (= (:breakpoints state) (:desired-breakpoints @session))
              state
              (recur (sync-breakpoints! state))))]
      (when (:supportsConfigurationDoneRequest capabilities)
        (protocol-request! state "configurationDone" {}))
      (await-response! session "attach" attach-response)
      (.add protocol-queue
            (fn [{:keys [session] :as state}]
              (swap! (:data session) update :debugger
                     #(cond-> % (= :connecting (:status %)) (assoc :status :running)))
              (notify! state :on-connected)
              state))
      state)))

(defn- connect-socket!
  ^Socket [^String address resolve-port]
  (let [deadline (+ (System/nanoTime) (* 1000000 (long request-timeout-ms)))]
    (loop []
      (let [port (resolve-port)
            socket (Socket.)
            error (try
                    (when port
                      (.connect socket (InetSocketAddress. address (int port)) 1000)
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

(defn- run-connection! [{:keys [protocol-queue coordinator-queue] :as state} address resolve-port]
  (task/with-open [socket (connect-socket! address resolve-port)]
    (let [in (BufferedInputStream. (.getInputStream socket))
          out (.getOutputStream socket)]
      (task/scope :all-successful
        ;; Reader: read and parse socket messages, then queue protocol operations.
        (task/fork (read-messages! protocol-queue in))

        ;; Protocol: own socket writes, sequence numbers and pending responses.
        ;; Handle messages without waiting for responses.
        (task/fork (run-operations! protocol-queue (assoc state :out out :next-seq 0 :pending {})))

        ;; Coordinator: initialize, then process caller operations in order.
        ;; Own synchronized breakpoints; may wait for protocol responses.
        (task/fork (run-operations! coordinator-queue (initialize! state))))))
  nil)

(defn- run-session! [{:keys [session ^CompletableFuture stop-requested] :as state} address resolve-port]
  (let [exception
        (try
          (task/scope :first-completed
            (task/fork (.get stop-requested))
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
  "Start an asynchronous debugger connection and return its session.

  Args:
    address         debugger hostname or IP address
    resolve-port    function of no args that returns the debugger port, or nil
                    while the port is unavailable
    local-root      absolute project directory used to convert source paths
                    to project paths

  Options may be omitted, passed as keyword/value pairs, as a map, or as
  keyword/value pairs followed by a map. A trailing map overrides preceding
  pairs.

  Options:
    :breakpoints     initial breakpoints by source path; defaults to {}
                     (see set-breakpoints!)
    :stop-on-entry   whether attachment should stop execution; defaults to false
    :target          target metadata retained in the session state
    :on-connected    fn of session and state; called after initialization
    :on-suspended    fn of session, state and stopped-event body
    :on-resumed      fn of session and state
    :on-output       fn of session, state and output-event body
    :on-invalidated  fn of session, state and invalidated-event body
    :on-error        fn of session, state and exception
    :on-closed       fn of session and state

  Callbacks run on the UI thread in notification order. Their state argument
  is captured when the notification is queued, so it may differ from the
  session's state when the callback runs. Callbacks may be omitted.

  Dereferencing the session returns its debugger state. Its initial status
  is :connecting. Port discovery and connection attempts are retried until
  successful, cancelled or timed out."
  [address resolve-port local-root & {:keys [breakpoints target] :as options}]
  (let [transport {:protocol-queue (LinkedBlockingQueue.)
                   :coordinator-queue (LinkedBlockingQueue.)
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
      (run-session! (assoc (merge options transport) :session session :local-root local-root) address resolve-port))
    session))

(defn disconnect!
  "Detach from the debuggee and close the debugger connection.

  Requests detachment without terminating the debuggee when the session is
  running or suspended. Closes the connection even if the request fails.

  Returns the response body, or nil when no request is sent. Request failures
  are thrown after cleanup. Blocks; call off the UI thread."
  [session]
  (assert (not (ui/on-ui-thread?)))
  (try
    (when (#{:running :suspended} (status session))
      (request! session "disconnect" {:terminateDebuggee false}))
    (finally
      (close! session))))

(defn control!
  "Send an execution-control command to the debugger.

  Args:
    session    debugger session
    command    DAP command string: pause, continue, next, stepIn or stepOut

  Uses the suspended thread for commands other than pause. Otherwise,
  requests the adapter's threads and uses the first one.

  Returns the response body. Execution-state changes arrive separately
  through callbacks. Blocks and throws request failures;
  call off the UI thread."
  [session command]
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

(defn stack
  "Fetch the call stack for the suspension identified by snapshot.

  Returns a vector of frames with :id, :function, :file and one-based :line.
  Source paths within the project directory become project paths.

  Returns nil for a nil or stale snapshot. Results and request failures are
  discarded if the debugger resumes, stops again or closes during retrieval.
  Request failures are thrown if the suspension remains unchanged.

  Blocks; call off the UI thread."
  [session snapshot]
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

(defn variables
  "Fetch the children of a variable reference from a debugger suspension.

  Args:
    session      debugger session
    snapshot     snapshot returned by suspension
    reference    positive :variablesReference from an adapter result

  Returns the adapter's variable maps, including :name, :value and
  :variablesReference. A zero :variablesReference denotes a leaf value.

  Returns nil for a nil or stale snapshot. Results and request failures are
  discarded if the debugger resumes, stops again or closes during retrieval.
  Request failures are thrown if the suspension remains unchanged.

  Blocks; call off the UI thread."
  [session snapshot reference]
  (inspect session snapshot
           #(:variables (request! session "variables" {:variablesReference reference}))))

(defn frame-variables
  "Fetch the variables visible in a suspended stack frame.

  Args:
    session     debugger session
    snapshot    snapshot returned by suspension
    frame-id    :id of a frame returned by stack for that suspension

  Combines variables from non-expensive scopes in adapter order. Exposes
  the Globals scope as an expandable _G variable.

  Returns nil for a nil or stale snapshot. Results and request failures are
  discarded if the debugger resumes, stops again or closes during retrieval.
  Request failures are thrown if the suspension remains unchanged.

  Blocks; call off the UI thread."
  [session snapshot frame-id]
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

(defn evaluate!
  "Evaluate Lua code in the debuggee using the debugger REPL context.

  Args:
    session       debugger session
    frame-id      suspended frame id, or nil for the global context
    expression    Lua expression or statements to evaluate

  Returns the adapter's evaluation response body. Use
  evaluation-result->string to format it for display.

  Evaluation may modify the debuggee and invalidate previously fetched
  variables. Blocks and throws request failures; call off the UI thread."
  [session frame-id expression]
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
  "Format an evaluation response for display in the debugger console.

  Args:
    session     debugger session
    snapshot    suspension snapshot captured for the evaluation
    result      response body returned by evaluate!

  Fetches and recursively formats inspectable values, limiting expansion
  depth and avoiding repeated expansion of shared or cyclic references.
  Multiple return values are formatted independently and joined with
  newlines, preserving their order and nil values.

  Returns the adapter's original :result text if the suspension is stale
  or changes during formatting.

  May perform blocking requests and throw request failures;
  call off the UI thread."
  [session snapshot result]
  (let [output
        (if (:defoldResultCount result)
          (if-let [children (variables session snapshot (:variablesReference result))]
            (coll/join-to-string
              "\n"
              (eduction
                (map (fn [child]
                       (evaluation-value->string-impl session snapshot (volatile! #{}) 0 child)))
                children))
            (:result result))
          (evaluation-value->string-impl session snapshot (volatile! #{}) 0
                                         (assoc result :value (:result result))))]
    (if (= snapshot (suspension @session))
      output
      (:result result))))
