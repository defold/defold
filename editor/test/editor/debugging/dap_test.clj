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

(ns editor.debugging.dap-test
  (:require [clojure.data.json :as json]
            [clojure.java.io :as io]
            [clojure.string :as string]
            [clojure.test :refer :all]
            [editor.debugging.dap :as dap]
            [editor.future :as future]
            [editor.ui :as ui]
            [util.coll :as coll])
  (:import [java.io ByteArrayInputStream ByteArrayOutputStream DataInputStream EOFException IOException OutputStream]
           [java.net InetAddress ServerSocket Socket]
           [java.nio.charset StandardCharsets]
           [java.nio.file Files]
           [java.util.concurrent CountDownLatch ExecutionException LinkedBlockingQueue TimeUnit]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(use-fixtures :each
  (fn [f]
    (try
      (f)
      (finally (ui/run-now nil)))))

(defn- await! [value]
  (try
    (let [result (deref value 10000 ::timeout)]
      (when (= ::timeout result)
        (throw (IOException. "Timed out waiting for debugger test")))
      result)
    (catch ExecutionException exception
      (throw (.getCause exception)))))

(defn- take-event! [^LinkedBlockingQueue events]
  (or (.poll events 10 TimeUnit/SECONDS)
      (throw (IOException. "Timed out waiting for debugger event"))))

(defn- wire-bytes [message]
  (let [body (.getBytes (json/write-str message :escape-unicode false) StandardCharsets/UTF_8)
        out (ByteArrayOutputStream.)]
    (.write out (.getBytes (str "Content-Length: " (alength body) "\r\n\r\n") StandardCharsets/US_ASCII))
    (.write out body)
    (.toByteArray out)))

(defn- receive! [^DataInputStream in]
  ;; An independent, intentionally simple adapter-side reader.
  (let [line (.readLine in)]
    (when-not line (throw (EOFException.)))
    (let [length (parse-long (second (string/split line #": ")))
          body (byte-array length)]
      (is (= "" (.readLine in)))
      (.readFully in body)
      (json/read-str (String. body StandardCharsets/UTF_8) :key-fn keyword))))

(defn- send! [^OutputStream out message]
  (.write out ^bytes (wire-bytes message))
  (.flush out))

(defn- respond! [out request body]
  (send! out {:seq 1
              :type "response"
              :request_seq (:seq request)
              :command (:command request)
              :success true
              :body body}))

(defn- event! [out event body]
  (send! out {:seq 1
              :type "event"
              :event event
              :body body}))

(defn- with-adapter [callbacks handler f]
  (with-open [server (ServerSocket. 0 1 (InetAddress/getLoopbackAddress))]
    (let [ready (future/make)
          events (LinkedBlockingQueue.)
          requests (atom [])
          adapter
          (future/io
            (with-open [socket (.accept server)]
              (.setSoTimeout socket 10000)
              (let [in (DataInputStream. (.getInputStream socket))
                    out (.getOutputStream socket)]
                (try
                  (loop [attach nil]
                    (let [request (receive! in)
                          command (:command request)]
                      (swap! requests conj request)
                      (case command
                        "initialize"
                        (respond! out request {:supportsConfigurationDoneRequest true})

                        "attach"
                        (event! out "initialized" {})

                        "configurationDone"
                        (do
                          (respond! out request {})
                          (respond! out attach {}))

                        "setBreakpoints"
                        (respond! out request {:breakpoints []})

                        "disconnect"
                        (do
                          (respond! out request {})
                          (event! out "terminated" {}))

                        (handler request in out socket))
                      (when-not (or (= "disconnect" command) (.isClosed socket))
                        (recur (if (= "attach" command) request attach)))))
                  (catch EOFException _)))))
          session
          (dap/connect! "127.0.0.1" #(.getLocalPort server)
                        (merge {:local-root "/project"
                                :breakpoints {"/main.script" [{:line 5
                                                               :condition "x > 2"}]}
                                :on-connected (fn [session _]
                                                (is (ui/on-ui-thread?))
                                                (future/complete! ready session))
                                :on-suspended (fn [_ snapshot body] (.add events [:stopped (dap/suspension snapshot) body]))
                                :on-resumed (fn [_ _] (.add events [:continued]))
                                :on-output (fn [_ _ body] (.add events [:output body]))
                                :on-closed (fn [_ _] (.add events [:closed]))
                                :on-error (fn [_ _ exception]
                                            (is (ui/on-ui-thread?))
                                            (.add events [:error exception]))}
                               callbacks))]
      (try
        (is (identical? session (await! ready)))
        (f session requests events)
        (finally
          (dap/close! session)
          (await! adapter))))))

;; Verify UTF-8 Content-Length framing handles partial reads and consecutive
;; messages, and rejects incomplete headers or bodies without losing framing.
(deftest framing-test
  (let [message
        {:seq 1
         :type "event"
         :event "output"
         :body {:output "héj 🦊\n"}}
        bytes (wire-bytes message)
        out (ByteArrayOutputStream.)]
    (#'dap/write-message! out message)
    (is (= message (receive! (DataInputStream. (ByteArrayInputStream. (.toByteArray out))))))
    (testing "A UTF-8 message can arrive one byte at a time, followed immediately by another"
      (let [inbox (LinkedBlockingQueue.)
            stopped (future/make)
            state {:session (atom {:status :running}) :stop-requested stopped}
            index (atom -1)
            twice (byte-array (into (vec bytes) bytes))
            next-byte
            (fn []
              (let [i (long (swap! index inc))]
                (if (>= i (alength twice))
                  -1
                  (bit-and 255 (aget twice i)))))
            in
            (proxy [java.io.InputStream] []
              (read
                ([] (next-byte))
                ([buffer offset length]
                 (if (zero? (long length))
                   0
                   (let [value (next-byte)]
                     (if (= -1 value)
                       -1
                       (do
                         (aset-byte buffer offset (unchecked-byte value))
                         1)))))))]
        (with-redefs-fn {#'dap/handle-message! (fn [state message] (assoc state :message message))}
          #(do
             (#'dap/read-messages! inbox in)
             (is (= message (:message ((.take inbox) state))))
             (is (= message (:message ((.take inbox) state))))))
        (is (nil? ((.take inbox) state)))
        (is (future/done? stopped))
        (is (nil? (await! stopped))))))
  (testing "Reject incomplete framing without treating it as a response"
    (doseq [text ["\r\n\r\n"
                  "Content-Length: 2"
                  "Content-Length: 2\r\n"
                  "Content-Length: 4\r\n\r\n{}"
                  "Content-Length: 2\r\ncontent-length: 2\r\n\r\n{}"
                  "Content-Length: invalid\r\nContent-Length: 2\r\n\r\n{}"
                  "Content-Length: 0\r\n\r\n"
                  "Content-Length: -1\r\n\r\n"
                  "Content-Length: \r\n\r\n"
                  "Content-Length: invalid\r\n\r\n"
                  "Content-Length: 9223372036854775808\r\n\r\n"
                  "Content-Length: 1048577\r\n\r\n"
                  (str "X-Header: " (.repeat "x" 4096) "\r\nContent-Length: 2\r\n\r\n{}")]]
      (let [inbox (LinkedBlockingQueue.)]
        (#'dap/read-messages! inbox (ByteArrayInputStream. (.getBytes ^String text StandardCharsets/UTF_8)))
        (is (thrown? IOException ((.take inbox) nil)))))))

;; Verify initialization precedes breakpoint configuration, changed sources replace
;; their full breakpoint sets, and disconnect detaches without terminating Lua.
(deftest configuration-and-breakpoints-test
  (with-adapter
    {}
    (fn [request _ out _]
      (respond! out request {:threads [{:id 7
                                        :name "Lua"}]}))
    (fn [session requests events]
      (is (= ["initialize" "attach" "setBreakpoints" "configurationDone"]
             (mapv :command @requests)))
      (is (= {:localRoot "/project"
              :stopOnEntry false}
             (:arguments (second @requests))))
      (is (= :running (dap/status session)))
      (testing "Replace changed sources and clear the last breakpoint without pausing Lua"
        (dap/set-breakpoints! session
                              {"/other.lua" [{:line 9}]})
        ;; A subsequent request observes the adapter after the submitted edits.
        (dap/evaluate! session nil "nil")
        (let [updates (filterv #(= "setBreakpoints" (:command %)) (subvec @requests 4))]
          (is (= #{["/main.script" []] ["/other.lua" [{:line 9}]]}
                 (into #{} (map #(vector (get-in % [:arguments :source :path])
                                         (get-in % [:arguments :breakpoints]))) updates)))
          (is (coll/every? #(= "setBreakpoints" (:command %)) updates)))
        (dap/set-breakpoints! session
                              {"/other.lua" [{:line 9}]})
        (dap/evaluate! session nil "nil")
        (is (= 3 (count (filterv #(= "setBreakpoints" (:command %)) @requests)))))
      (dap/disconnect! session)
      (is (apply < (mapv :seq @requests)))
      (is (= [:closed] (take-event! events)))
      (is (= false (get-in (peek @requests) [:arguments :terminateDebuggee])))
      (dap/close! session)
      (is (nil? (.poll ^LinkedBlockingQueue events))))))

;; Verify stack/scopes/evaluation and stepping use the correct frame and thread,
;; globals load on demand, and inspection references are discarded after resume.
(deftest inspection-and-control-test
  (with-adapter
    {}
    (fn [{:keys [command arguments] :as request} _ out _]
      (case command
        "threads"
        (respond! out request {:threads [{:id 7
                                          :name "Lua"}]})

        "pause"
        (do
          (respond! out request {})
          (event! out "stopped" {:threadId 7
                                 :reason "pause"}))

        "stackTrace"
        (respond! out request {:stackFrames [{:id 42
                                              :name "update"
                                              :line 5
                                              :source {:path "/project/main.script"}}
                                             {:id 99
                                              :name "native"
                                              :line 0}]})

        "scopes"
        (do
          (is (= 42 (:frameId arguments)))
          (respond! out request {:scopes [{:name "Locals"
                                           :variablesReference 10
                                           :expensive false}
                                          {:name "Upvalues"
                                           :variablesReference 11
                                           :expensive false}
                                          {:name "Globals"
                                           :variablesReference 12
                                           :expensive true}]}))

        "variables"
        (respond! out request {:variables (case (long (:variablesReference arguments))
                                            10 [{:name "self"
                                                 :value "table: 1"
                                                 :variablesReference 20}]
                                            11 [{:name "flag"
                                                 :value "false"
                                                 :variablesReference 0}]
                                            12 [{:name "_VERSION"
                                                 :value "\"Lua 5.1\""
                                                 :variablesReference 0}]
                                            20 [{:name "[\"café\"]"
                                                 :value "42"
                                                 :variablesReference 0}])})

        "evaluate"
        (do
          (is (= 42 (:frameId arguments)))
          (is (= "repl" (:context arguments)))
          (respond! out request {:result "false"
                                 :variablesReference 0}))

        ("next" "stepIn" "stepOut")
        (do
          (is (= 7 (:threadId arguments)))
          (respond! out request {})
          (event! out "continued" {:threadId 7})
          (event! out "stopped" {:threadId 7
                                 :reason "step"}))

        "continue"
        (do
          (respond! out request {})
          (event! out "continued" {:threadId 7}))))
    (fn [session requests events]
      (dap/control! session "pause")
      (let [[event snapshot] (take-event! events)]
        (is (= :stopped event))
        (is (= [{:id 42
                 :function "update"
                 :file "/main.script"
                 :line 5}
                {:id 99
                 :function "native"
                 :file nil
                 :line 0}]
               (dap/stack session snapshot)))
        (let [frame-variables (dap/frame-variables session snapshot 42)
              globals (peek frame-variables)]
          (is (= ["self" "flag" "_G"] (mapv :name frame-variables)))
          (is (= 12 (:variablesReference globals)))
          (testing "Globals are fetched only on expansion"
            (is (= [10 11]
                   (into []
                         (comp (filter #(= "variables" (:command %)))
                               (map #(get-in % [:arguments :variablesReference])))
                         @requests)))
            (is (= [{:name "_VERSION"
                     :value "\"Lua 5.1\""
                     :variablesReference 0}]
                   (dap/variables session snapshot (:variablesReference globals))))))
        (is (= "42" (:value (first (dap/variables session snapshot 20)))))
        (is (= "false" (:result (dap/evaluate! session 42 "flag"))))
        (doseq [command ["next" "stepIn" "stepOut"]]
          (dap/control! session command)
          (is (= [:continued] (take-event! events)))
          (is (= :stopped (first (take-event! events))))
          (is (nil? (dap/variables session snapshot 20))))
        (dap/control! session "continue")
        (is (= [:continued] (take-event! events)))
        (is (nil? (dap/suspension @session)))
        (is (nil? (dap/stack session nil)))))))

;; Console table results retain nested contents, Lua key names, empty tables,
;; and scalar values without repeatedly loading shared or cyclic references.
(deftest evaluation-result-contents-test
  (let [session
        (agent {:status :suspended
                :generation 1
                :threadId 7})

        snapshot (dap/suspension @session)
        requests (atom [])
        children
        {1 [{:name "[1]"
             :value "false"
             :variablesReference 0}
            {:name "nested"
             :value "table: nested"
             :variablesReference 2}
            {:name "alias"
             :value "table: nested"
             :variablesReference 2}
            {:name "self"
             :value "table: root"
             :variablesReference 1}
            {:name "empty"
             :value "table: empty"
             :variablesReference 3}]
         2 [{:name "[\"end\"]"
             :value "42"
             :variablesReference 0}]
         3 []}]
    (with-redefs [dap/request!
                  (fn [_ command {:keys [variablesReference]}]
                    (is (= "variables" command))
                    (swap! requests conj variablesReference)
                    {:variables (get children variablesReference)})]
      (doseq [value ["false" "nil" "42" "\"hello\""]]
        (is (= value
               (dap/evaluation-result->string session snapshot
                                              {:result value
                                               :variablesReference 0}))))
      (is (= [] @requests))
      (is (= (coll/join-to-string
               "\n"
               ["{ -- table: root"
                "  [1] = false,"
                "  nested = { -- table: nested"
                "    [\"end\"] = 42"
                "  },"
                "  alias = table: nested,"
                "  self = table: root,"
                "  empty = { -- table: empty"
                "  }"
                "}"])
             (dap/evaluation-result->string session snapshot
                                            {:result "table: root"
                                             :variablesReference 1})))
      (is (= [1 2 3] @requests)))))

;; Bound recursive console inspection and stop loading references when execution
;; resumes, rather than printing a partially refreshed table from another stop.
(deftest evaluation-result-lifetime-test
  (let [session
        (agent {:status :suspended
                :generation 1
                :threadId 7})

        snapshot (dap/suspension @session)
        result {:result "table: 1"
                :variablesReference 1}
        requests (atom [])]
    (with-redefs [dap/request!
                  (fn [_ _command {:keys [variablesReference]}]
                    (swap! requests conj variablesReference)
                    {:variables [{:name "child"
                                  :value (str "table: " (inc (long variablesReference)))
                                  :variablesReference (inc (long variablesReference))}]})]
      (is (string/includes? (dap/evaluation-result->string session snapshot result) "child = table: 17"))
      (is (= (vec (range 1 17)) @requests)))

    (reset! requests [])
    (with-redefs [dap/request!
                  (fn [_ _command {:keys [variablesReference]}]
                    (swap! requests conj variablesReference)
                    (await (send-via future/io-executor session assoc :status :running :generation 2))
                    {:variables [{:name "stale"
                                  :value "42"
                                  :variablesReference 0}]})]
      (is (= "table: 1" (dap/evaluation-result->string session snapshot result)))
      (is (= [1] @requests))
      (is (= "table: 1" (dap/evaluation-result->string session snapshot result)))
      (is (= [1] @requests)))))

;; A resume during scope or variable retrieval invalidates every returned reference.
(deftest stale-frame-variables-test
  (doseq [resume-command ["scopes" "variables"]]
    (testing (str "Resume during " resume-command)
      (with-adapter
        {}
        (fn [{:keys [command] :as request} _ out _]
          (when (= resume-command command)
            (event! out "continued" {:threadId 7}))
          (case command
            "threads"
            (respond! out request {:threads [{:id 7 :name "Lua"}]})

            "pause"
            (do
              (respond! out request {})
              (event! out "stopped" {:threadId 7 :reason "pause"}))

            "scopes"
            (respond! out request
                      {:scopes [{:name "Locals" :variablesReference 10 :expensive false}
                                {:name "Globals" :variablesReference 20 :expensive true}]})

            "variables"
            (respond! out request
                      {:variables [{:name "old" :value "table" :variablesReference 30}]})))
        (fn [session _ events]
          (dap/control! session "pause")
          (let [[event snapshot] (take-event! events)]
            (is (= :stopped event))
            (is (nil? (dap/frame-variables session snapshot 42)))
            (is (= [:continued] (take-event! events)))
            (is (nil? (dap/suspension @session)))))))))

;; Verify out-of-order responses reach the correct requests, output events remain
;; separate, request errors preserve the connection, and disconnect cancels pending work.
(deftest responses-and-disconnect-test
  (with-adapter
    {}
    (fn [request in out ^Socket socket]
      (case (:command request)
        "first"
        (let [second-request (receive! in)]
          (event! out "output" {:output "héj 🦊"})
          (respond! out second-request {:result "second"})
          (respond! out request {:result "first"}))

        "bad"
        (send! out {:seq 1
                    :type "response"
                    :request_seq (:seq request)
                    :command "bad"
                    :success false
                    :message "Evaluation failed"})

        "close"
        (.close socket)))
    (fn [session _ events]
      (let [first-response (#'dap/send-request! session "first" {})
            second-response (#'dap/send-request! session "second" {})]
        (is (= {:result "first"} (#'dap/await-response! session "first" first-response)))
        (is (= {:result "second"} (#'dap/await-response! session "second" second-response)))
        (is (= [:output {:output "héj 🦊"}] (take-event! events))))
      (is (thrown-with-msg? IOException #"Evaluation failed" (#'dap/request! session "bad" {})))
      (is (= :running (dap/status session)))
      (is (thrown? IOException (#'dap/request! session "close" {})))
      (is (= [:closed] (take-event! events)))
      (is (= :closed (dap/status session))))))

;; The reader preserves responses before EOF, including a successfully completed
;; nil body, rather than cancelling the session before delivering the response.
(deftest response-before-eof-test
  (doseq [body [{:result "last response"} nil]]
    (with-adapter
      {}
      (fn [request _ out ^Socket socket]
        (respond! out request body)
        (.close socket))
      (fn [session _ events]
        (is (= body (#'dap/request! session "last" {})))
        (is (= [:closed] (take-event! events)))
        (dap/close! session)
        (is (= :closed (dap/status session)))))))

;; UI callbacks can schedule requests in the background without blocking later
;; notifications, and receive snapshots without transport queues.
(deftest callbacks-can-schedule-requests-test
  (let [result (future/make)
        second-output (promise)
        order (atom [])]
    (with-adapter
      {:on-output
       (fn [session snapshot {:keys [output]}]
         (is (ui/on-ui-thread?))
         (is (= :running (:status snapshot)))
         (is (not (contains? snapshot :inbox)))
         (swap! order conj output)
         (if (= "first" output)
           (future/io
             (future/complete! result (#'dap/request! session "from-callback" {})))
           (deliver second-output true)))}
      (fn [request _ out _]
        (case (:command request)
          "trigger"
          (do
            (event! out "output" {:output "first"})
            (event! out "output" {:output "second"})
            (respond! out request {}))
          "from-callback"
          (respond! out request {:result "callback response"})))
      (fn [session _ _]
        (is (= {} (#'dap/request! session "trigger" {})))
        (is (= {:result "callback response"} (await! result)))
        (is (true? (await! second-output)))
        (is (= ["first" "second"] @order))))))

;; Event callbacks see their published state and run in wire order, even when
;; suspension, continuation, and termination arrive together.
(deftest callbacks-observe-event-state-test
  (let [observed (atom [])
        closed (promise)]
    (with-adapter
      {:on-suspended (fn [_ snapshot body]
                       (is (ui/on-ui-thread?))
                       (swap! observed conj [:stopped (:status snapshot) body]))
       :on-resumed (fn [_ snapshot]
                     (is (ui/on-ui-thread?))
                     (swap! observed conj [:continued (:status snapshot)]))
       :on-closed (fn [_ snapshot]
                    (is (ui/on-ui-thread?))
                    (swap! observed conj [:closed (:status snapshot)])
                    (deliver closed true))}
      (fn [request _ out _]
        (respond! out request {})
        (event! out "stopped" {:threadId 7})
        (event! out "continued" {})
        (event! out "terminated" {}))
      (fn [session _ _]
        (is (= {} (#'dap/request! session "trigger" {})))
        (is (true? (await! closed)))
        (dap/close! session)
        (is (= [[:stopped :suspended {:threadId 7}]
                [:continued :running]
                [:closed :closed]]
               @observed))
        (is (= :closed (dap/status session)))
        (is (= 2 (:generation @session)))))))

;; Concurrent operations receive their own responses without blocking submission.
(deftest concurrent-requests-test
  (with-adapter
    {}
    (fn [request _ out _]
      (respond! out request (:arguments request)))
    (fn [session requests _]
      (let [responses
            (mapv (fn [index]
                    (future/io (#'dap/request! session "echo" {:index index})))
                  (range 32))]
        (is (= (mapv #(hash-map :index %) (range 32)) (mapv await! responses)))
        (let [sequences (mapv :seq @requests)]
          (is (= (count sequences) (count (set sequences))))
          (is (apply < sequences)))))))

;; An unresponsive adapter closes the session and rejects subsequent work.
(deftest request-timeout-closes-session-test
  (with-adapter
    {}
    (fn [_ _ _ _])
    (fn [session _ events]
      (with-redefs-fn {#'dap/request-timeout-ms 100}
        #(is (thrown-with-msg? IOException #"timed out: wait"
                               (#'dap/request! session "wait" {}))))
      (let [[event exception] (take-event! events)]
        (is (= :error event))
        (is (instance? IOException exception)))
      (is (= [:closed] (take-event! events)))
      (is (= :closed (dap/status session)))
      (is (thrown-with-msg? IOException #"disconnected" (#'dap/request! session "after-close" {}))))))

;; A malformed adapter message closes the session and releases a waiting caller
;; without leaving session tasks running or allowing later edits to reopen it.
(deftest protocol-error-closes-session-test
  (with-adapter
    {}
    (fn [_ _ out _]
      (send! out {:seq 1 :type "unexpected"}))
    (fn [session _ events]
      (is (thrown-with-msg? IOException #"Invalid debugger message"
                            (dap/evaluate! session nil "waiting")))
      (let [[event exception] (take-event! events)]
        (is (= :error event))
        (is (= "Invalid debugger message" (ex-message exception))))
      (is (= [:closed] (take-event! events)))
      (dap/set-breakpoints! session {"/main.script" [{:line 9}]})
      (dap/close! session)
      (is (= :closed (:status @session)))
      (is (not (contains? @(:data session) :transport)))
      (is (nil? (.poll ^LinkedBlockingQueue events))))))

;; Truncated headers and bodies must report protocol errors and release waiting
;; callers, rather than being treated as normal EOF after initialization.
(deftest truncated-message-closes-session-test
  (doseq [fragment ["Content-Length: 4\r\n" "Content-Length: 4\r\n\r\n{}"]]
    (with-adapter
      {}
      (fn [_ _ ^OutputStream out ^Socket socket]
        (.write out (.getBytes ^String fragment StandardCharsets/US_ASCII))
        (.flush out)
        (.close socket))
      (fn [session _ events]
        (is (thrown-with-msg? IOException #"Invalid debugger message" (#'dap/request! session "waiting" {})))
        (let [[event exception] (take-event! events)]
          (is (= :error event))
          (is (= "Invalid debugger message" (ex-message exception))))
        (is (= [:closed] (take-event! events)))
        (dap/close! session)
        (is (= :closed (dap/status session)))))))

;; Verify source mapping handles project boundaries, trailing separators, Windows
;; paths and drive-letter casing, and frames without a source file.
(deftest source-paths-test
  (doseq [[root path expected] [["/project" "/project/a.script" "/a.script"]
                                ["/project/" "/project/a.script" "/a.script"]
                                ["/project" "/project-other/a.lua" "/project-other/a.lua"]
                                ["C:\\project" "c:/project/main.script" "/main.script"]
                                ["/project" nil nil]]]
    (is (= expected (#'dap/source-path->project-path root path)))))

;; Verify disconnect cancels connection retries without reporting an error, and
;; a queued stopped event cannot reopen the cancelled session.
(deftest connection-cancellation-test
  (let [closed (future/make)
        errors (atom [])
        session
        (dap/connect! "127.0.0.1" (constantly nil)
                      {:local-root "/project"
                       :breakpoints {}
                       :on-closed (fn [session _] (future/complete! closed session))
                       :on-error (fn [_ _ error] (swap! errors conj error))})
        inbox (get-in @(:data session) [:transport :inbox])]
    (dap/disconnect! session)
    (is (identical? session (await! closed)))
    (is (= :closed (dap/status session)))
    (is (= [] @errors))
    (testing "A queued stopped event cannot reopen a cancelled session"
      (.add ^LinkedBlockingQueue inbox
            (fn [state]
              (#'dap/handle-message! state {:type "event" :event "stopped" :body {:threadId 7}})))
      (dap/close! session)
      (is (= :closed (dap/status session))))))

;; Verify a server closing during initialization reports connection failure and
;; closes the session instead of leaving attachment pending.
(deftest handshake-disconnect-test
  (with-open [server (ServerSocket. 0 1 (InetAddress/getLoopbackAddress))]
    (let [adapter
          (future/io
            (with-open [socket (.accept server)]
              (receive! (DataInputStream. (.getInputStream socket)))))
          closed (future/make)
          error (future/make)
          session
          (dap/connect! "127.0.0.1" #(.getLocalPort server)
                        {:local-root "/project"
                         :breakpoints {}
                         :on-closed (fn [session _] (future/complete! closed session))
                         :on-error (fn [_ _ exception] (future/complete! error exception))})]
      (try
        (is (instance? IOException (await! error)))
        (is (identical? session (await! closed)))
        (is (= :closed (dap/status session)))
        (finally
          (dap/close! session)
          (await! adapter))))))

;; Closing during initialization releases requests waiting behind the handshake
;; without sending them or waiting for the initialization timeout.
(deftest close-during-initialization-test
  (with-open [server (ServerSocket. 0 1 (InetAddress/getLoopbackAddress))]
    (let [initializing (promise)
          adapter (future/io
                    (with-open [socket (.accept server)]
                      (.setSoTimeout socket 10000)
                      (let [in (DataInputStream. (.getInputStream socket))]
                        (is (= "initialize" (:command (receive! in))))
                        (deliver initializing true)
                        (is (thrown? EOFException (receive! in))))))
          session (dap/connect! "127.0.0.1" #(.getLocalPort server)
                                {:local-root "/project"})]
      (try
        (is (true? (await! initializing)))
        (let [response (#'dap/send-request! session "evaluate" {:expression "queued"})
              closing (future/io (dap/close! session))]
          (is (thrown? IOException (#'dap/await-response! session "evaluate" response)))
          (is (= :closed (dap/status session)))
          (is (nil? (await! closing))))
        (await! adapter)
        (finally
          (dap/close! session))))))

;; Edits made during initialization and while an earlier edit awaits a response
;; reach the adapter in order. A stop during configuration stays suspended.
(deftest breakpoint-edits-during-connection-test
  (with-open [server (ServerSocket. 0 1 (InetAddress/getLoopbackAddress))]
    (let [initializing (future/make)
          initialize (future/make)
          first-edit (future/make)
          second-edit (future/make)
          acknowledge (future/make)
          ready (future/make)
          adapter
          (future/io
            (with-open [socket (.accept server)]
              (.setSoTimeout socket 10000)
              (let [in (DataInputStream. (.getInputStream socket))
                    out (.getOutputStream socket)
                    request (receive! in)]
                (is (= "initialize" (:command request)))
                (future/complete! initializing true)
                (await! initialize)
                (respond! out request {:supportsConfigurationDoneRequest true})
                (let [attach (receive! in)]
                  (is (= "attach" (:command attach)))
                  (event! out "initialized" {})
                  (let [first-request (receive! in)]
                    (is (= "setBreakpoints" (:command first-request)))
                    (is (= [{:line 9}] (get-in first-request [:arguments :breakpoints])))
                    (future/complete! first-edit true)
                    (await! acknowledge)
                    (respond! out first-request {})
                    (let [second-request (receive! in)]
                      (is (= "setBreakpoints" (:command second-request)))
                      (is (= [{:line 11}] (get-in second-request [:arguments :breakpoints])))
                      (respond! out second-request {})
                      (future/complete! second-edit true)))
                  (let [configuration (receive! in)]
                    (is (= "configurationDone" (:command configuration)))
                    (event! out "stopped" {:threadId 7 :reason "entry"})
                    (respond! out configuration {})
                    (respond! out attach {}))
                  (let [disconnect (receive! in)]
                    (is (= "disconnect" (:command disconnect)))
                    (is (false? (get-in disconnect [:arguments :terminateDebuggee])))
                    (respond! out disconnect {}))))))
          session (dap/connect! "127.0.0.1" #(.getLocalPort server)
                                {:local-root "/project"
                                 :breakpoints {"/main.script" [{:line 5}]}
                                 :on-connected (fn [session _] (future/complete! ready session))
                                 :on-error (fn [_ _ error] (future/fail! ready error))})]
      (try
        (await! initializing)
        (dap/set-breakpoints! session {"/main.script" [{:line 9}]})
        (future/complete! initialize true)
        (await! first-edit)
        (let [updated (dap/set-breakpoints! session {"/main.script" [{:line 11}]})]
          (is (identical? session updated))
          (future/complete! acknowledge true)
          (await! second-edit))
        (is (identical? session (await! ready)))
        (is (= :suspended (dap/status session)))
        (dap/disconnect! session)
        (await! adapter)
        (finally
          (future/complete! initialize true)
          (future/complete! acknowledge true)
          (dap/close! session))))))

;; Closing releases outstanding and newly submitted requests, closes the socket,
;; and notifies the UI once even when close is called repeatedly.
(deftest close-settles-work-test
  (let [received (future/make)]
    (with-adapter
      {}
      (fn [_ _ _ _] (future/complete! received true))
      (fn [session _ events]
        (let [evaluation (future/io (dap/evaluate! session nil "waiting"))]
          (await! received)
          (let [closed (future/io (dap/close! session))
                late (mapv #(future/io (dap/evaluate! session nil (str %))) (range 20))]
            (await! closed)
            (is (= :closed (dap/status session)))
            (is (= [:closed] (take-event! events)))
            (doseq [response (conj late evaluation)]
              (is (thrown? IOException (await! response))))
            (dap/close! session)
            (is (nil? (.poll ^LinkedBlockingQueue events)))))))))

;; Closing rejects new work before joining interrupted connection setup, so a
;; blocked resolver cannot retain an already closed session or accept late edits.
(deftest close-joins-connection-task-test
  (let [started (promise)
        interrupted (promise)
        release (CountDownLatch. 1)
        finished (promise)
        closed (promise)
        errors (atom [])
        session
        (dap/connect! "127.0.0.1"
                      (fn []
                        (deliver started (Thread/currentThread))
                        (try
                          (.await (CountDownLatch. 1))
                          (catch InterruptedException exception
                            (deliver interrupted true)
                            (.await release)
                            (throw exception))
                          (finally
                            (deliver finished true))))
                      {:local-root "/project"
                       :on-closed (fn [_ _] (deliver closed true))
                       :on-error (fn [_ _ exception] (swap! errors conj exception))})]
    (try
      (let [^Thread connection-task (await! started)
            closing (future/io (dap/close! session))]
        (await! interrupted)
        (is (= :connecting (dap/status session)))
        (is (not (.isDone ^java.util.concurrent.CompletableFuture (:ended session))))
        (let [state @session]
          (is (not (contains? @(:data session) :transport)))
          (dap/set-breakpoints! session {"/late.script" [{:line 9}]})
          (is (identical? state @session)))
        (is (thrown-with-msg? IOException #"disconnected"
                              (dap/evaluate! session nil "late")))
        (.countDown release)
        (await! closing)
        (is (realized? finished))
        (is (not (.isAlive connection-task)))
        (is (not (contains? @(:data session) :transport)))
        (is (= :closed (dap/status session)))
        (is (true? (await! closed)))
        (is (= [] @errors))
        (is (.isDone ^java.util.concurrent.CompletableFuture (:ended session))))
      (finally
        (.countDown release)
        (dap/close! session)))))

;; Closing joins breakpoint synchronization as well as initialization, so a
;; cancelled workflow cannot keep running after another session is started.
(deftest close-joins-breakpoint-task-test
  (let [started (promise)
        finished (promise)
        protocol-request! @#'dap/protocol-request!]
    (with-redefs-fn
      {#'dap/protocol-request!
       (fn [state command arguments]
         (if (and (= "setBreakpoints" command) (= 9 (get-in arguments [:breakpoints 0 :line])))
           (try
             (deliver started (Thread/currentThread))
             (.await (CountDownLatch. 1))
             (finally
               (deliver finished true)))
           (protocol-request! state command arguments)))}
      #(with-adapter
         {}
         (fn [_ _ _ _])
         (fn [session _ events]
           (dap/set-breakpoints! session {"/main.script" [{:line 9}]})
           (let [^Thread breakpoint-task (await! started)]
             (dap/close! session)
             (is (realized? finished))
             (is (not (.isAlive breakpoint-task)))
             (is (= [:closed] (take-event! events)))
             (is (nil? (.poll ^LinkedBlockingQueue events)))))))))

;; Cancellation must wake a handler blocked in a write, settle its request, and
;; join the reader even though the handler cannot consume another inbox message.
(deftest close-during-write-test
  (let [writing (promise)
        finished (promise)
        reader-started (promise)
        reader-finished (promise)
        write-message! @#'dap/write-message!
        read-messages! @#'dap/read-messages!]
    (with-redefs-fn
      {#'dap/read-messages!
       (fn [inbox in]
         (deliver reader-started (Thread/currentThread))
         (try
           (read-messages! inbox in)
           (finally
             (deliver reader-finished true))))

       #'dap/write-message!
       (fn [out message]
         (if (= "blocked-write" (:command message))
           (try
             (deliver writing true)
             (.await (CountDownLatch. 1))
             (finally
               (deliver finished true)))
           (write-message! out message)))}
      #(with-adapter
         {}
         (fn [_ _ _ _])
         (fn [session _ events]
           (let [^Thread reader (await! reader-started)
                 response (#'dap/send-request! session "blocked-write" {})]
             (await! writing)
             (dap/close! session)
             (is (realized? finished))
             (is (realized? reader-finished))
             (is (not (.isAlive reader)))
             (is (thrown-with-msg? IOException #"disconnected"
                                   (#'dap/await-response! session "blocked-write" response)))
             (is (= [:closed] (take-event! events)))
             (is (nil? (.poll ^LinkedBlockingQueue events)))))))))

;; A burst of edits queued behind a blocked synchronization coalesces to the
;; latest value, and a later request waits for that synchronization to finish.
(deftest pending-breakpoint-edits-coalesce-test
  (let [started (promise)
        proceed (CountDownLatch. 1)
        protocol-request! @#'dap/protocol-request!]
    (with-redefs-fn
      {#'dap/protocol-request!
       (fn [state command arguments]
         (when (and (= "setBreakpoints" command) (= 9 (get-in arguments [:breakpoints 0 :line])))
           (deliver started true)
           (.await proceed))
         (protocol-request! state command arguments))}
      #(with-adapter
         {}
         (fn [request _ out _] (respond! out request {}))
         (fn [session requests _]
           (try
             (dap/set-breakpoints! session {"/main.script" [{:line 9}]})
             (await! started)
             (dap/set-breakpoints! session {"/main.script" [{:line 11}]})
             (let [response (#'dap/send-request! session "after-edit" {})
                   desired {"/main.script" [{:line 13}]}]
               (doseq [line (range 12 1012)]
                 (dap/set-breakpoints! session {"/main.script" [{:line line}]}))
               (dap/set-breakpoints! session desired)
               (is (= desired (:desired-breakpoints @session)))
               (.countDown proceed)
               (is (= {} (#'dap/await-response! session "after-edit" response))))
             (is (= [5 9 13]
                    (into []
                          (comp (filter (fn [request] (= "setBreakpoints" (:command request))))
                                (map (fn [request] (get-in request [:arguments :breakpoints 0 :line]))))
                          @requests)))
             (is (= "after-edit" (:command (peek @requests))))
             (finally
               (.countDown proceed))))))))

;; Interrupting a real socket reader must not enqueue normal EOF, which could
;; otherwise win the session's stop race and mask the failure of a sibling task.
(deftest cancelled-reader-does-not-report-eof-test
  (with-open [server (ServerSocket. 0 1 (InetAddress/getLoopbackAddress))]
    (let [inbox (LinkedBlockingQueue.)
          started (promise)
          reader
          (future/io
            (with-open [socket (Socket. "127.0.0.1" (.getLocalPort server))]
              (deliver started (Thread/currentThread))
              (#'dap/read-messages! inbox (.getInputStream socket))))]
      (with-open [_socket (.accept server)]
        (.interrupt ^Thread (await! started))
        (await! reader)
        (is (nil? (.poll inbox)))))))

;; Reader failures, including Errors, must follow previously read responses
;; through the inbox rather than cancelling their delivery from the reader task.
(deftest reader-failure-preserves-message-order-test
  (let [message {:type "response" :request_seq 1 :success true :body nil}
        ^bytes bytes (wire-bytes message)
        index (volatile! -1)
        failure (AssertionError. "Reader failed")
        next-byte
        (fn []
          (let [i (long (vswap! index #(inc (long %))))]
            (if (>= i (alength bytes))
              (throw failure)
              (bit-and 255 (aget bytes i)))))
        in (proxy [java.io.InputStream] []
             (read
               ([] (next-byte))
               ([buffer offset length]
                (if (zero? (long length))
                  0
                  (do
                    (aset-byte buffer (int offset) (unchecked-byte (next-byte)))
                    1)))))
        inbox (LinkedBlockingQueue.)
        response (future/make)
        state {:pending {1 response}}]
    (#'dap/read-messages! inbox in)
    (is (= {} (:pending ((take-event! inbox) state))))
    (is (future/done? response))
    (is (nil? (await! response)))
    (try
      ((take-event! inbox) nil)
      (is false "The reader failure must reach the inbox worker")
      (catch AssertionError exception
        (is (identical? failure exception))))))

;; A failed workflow cancels sibling tasks and releases requests still queued
;; behind it, without sending them or waiting for their request timeout.
(deftest workflow-failure-settles-queued-work-test
  (let [started (promise)
        fail (CountDownLatch. 1)
        received (atom [])
        protocol-request! @#'dap/protocol-request!]
    (with-redefs-fn
      {#'dap/protocol-request!
       (fn [state command arguments]
         (if (and (= "setBreakpoints" command) (= 9 (get-in arguments [:breakpoints 0 :line])))
           (do
             (deliver started true)
             (.await fail)
             (throw (IOException. "Breakpoint workflow failed")))
           (protocol-request! state command arguments)))}
      #(with-adapter
         {}
         (fn [request _ _ _] (swap! received conj request))
         (fn [session _ events]
           (try
             (dap/set-breakpoints! session {"/main.script" [{:line 9}]})
             (await! started)
             (let [response (#'dap/send-request! session "queued" {})]
               (.countDown fail)
               (is (thrown-with-msg? IOException #"Breakpoint workflow failed"
                                     (#'dap/await-response! session "queued" response))))
             (dap/close! session)
             (let [[event exception] (take-event! events)]
               (is (= :error event))
               (is (= "Breakpoint workflow failed" (ex-message exception))))
             (is (= [:closed] (take-event! events)))
             (is (= [] @received))
             (is (= :closed (dap/status session)))
             (is (not (contains? @(:data session) :transport)))
             (is (.isDone ^java.util.concurrent.CompletableFuture (:ended session)))
             (finally
               (.countDown fail))))))))

;; A UI callback can schedule closure in the background, joining transport
;; workers without blocking the UI thread or waiting for its notifications.
(deftest close-scheduled-from-callback-test
  (let [closed (promise)]
    (with-adapter
      {:on-output (fn [session _ _]
                    (is (ui/on-ui-thread?))
                    (future/io
                      (dap/close! session)
                      (deliver closed true)))}
      (fn [request _ out _]
        (respond! out request {})
        (event! out "output" {:output "close"}))
      (fn [session _ events]
        (is (= {} (#'dap/request! session "trigger" {})))
        (is (true? (await! closed)))
        (is (= :closed (dap/status session)))
        (is (= [:closed] (take-event! events)))))))

;; Closing joins transport workers without waiting for queued UI notifications,
;; guarding against deadlock when the UI is busy during cancellation.
(deftest close-does-not-join-notifications-test
  (let [started (promise)
        notified (promise)
        proceed (CountDownLatch. 1)]
    (try
      (with-adapter
        {:on-output (fn [_ _ _] (deliver notified true))}
        (fn [request _ out _]
          ;; Queue the notification before the response releases the caller to close.
          (event! out "output" {:output "queued notification"})
          (respond! out request {}))
        (fn [session _ _]
          (ui/run-later
            (deliver started true)
            (.await proceed))
          (await! started)
          (is (= {} (#'dap/request! session "trigger" {})))
          (is (nil? (dap/close! session)))
          (is (= :closed (dap/status session)))
          (is (not (realized? notified)))))
      (finally (.countDown proceed)))
    (is (true? (await! notified)))))

;; A caller that obtained the work queue immediately before close may enqueue into
;; the detached queue; session completion still releases it without queue cleanup.
(deftest submission-racing-close-test
  (let [captured (promise)
        proceed (CountDownLatch. 1)]
    (with-adapter
      {}
      (fn [_ _ _ _] (is false "A late request must not reach the adapter"))
      (fn [session _ _]
        (let [request-operation @#'dap/request-operation]
          (with-redefs-fn
            {#'dap/request-operation
             (fn [command arguments response]
               (if-not (= "late" command)
                 (request-operation command arguments response)
                 (do
                   (deliver captured true)
                   (.await proceed)
                   (request-operation command arguments response))))}
            #(let [request (future/io (#'dap/request! session "late" {}))]
               (try
                 (await! captured)
                 (dap/close! session)
                 (.countDown proceed)
                 (is (thrown-with-msg? IOException #"disconnected" (await! request)))
                 (finally (.countDown proceed))))))))))

;; Set -Ddefold.dap.debuggee to a built dap_debuggee or dap_debuggee_engine
;; executable to exercise this editor client against the real native server.
;; Verify real breakpoints, nested/global inspection, evaluation, fresh frame IDs
;; after stepping, and detachment followed by normal Lua exit.
(deftest native-debugger-test
  (when-let [binary (System/getProperty "defold.dap.debuggee")]
    (let [directory (.toFile (Files/createTempDirectory "editor-dap-" (make-array java.nio.file.attribute.FileAttribute 0)))
          source (io/file directory "main.lua")
          ready (future/make)
          stopped (LinkedBlockingQueue.)]
      (spit source (str "local data = {nested = {value = 42}}\n"
                        "local function work(value)\n"
                        "  local result = value + 1\n"
                        "  result = result + 1\n"
                        "  result = result + 1\n"
                        "  return result\n"
                        "end\n"
                        "local result = work(data.nested.value)\n"
                        "assert(result == 45)\n"))
      (try
        (let [^"[Ljava.lang.String;" command (into-array String [binary (.getAbsolutePath source)])
              process (.start (doto (ProcessBuilder. command)
                                (.redirectErrorStream true)))]
          (try
            (with-open [^java.io.BufferedReader reader (io/reader (.getInputStream process))]
              (let [port
                    (loop []
                      (let [line (.readLine reader)]
                        (when-not line (throw (IOException. "Native debugger exited before listening")))
                        (if-let [[_ port] (re-find #"(?:PORT |listening on [^:]+:)(\d+)" line)]
                          (parse-long port)
                          (recur))))
                    session
                    (dap/connect! "127.0.0.1" (constantly port)
                                  {:local-root (.getAbsolutePath directory)
                                   :breakpoints {(.getAbsolutePath source) [{:line 8}]}
                                   :on-connected (fn [session _] (future/complete! ready session))
                                   :on-suspended (fn [_ snapshot _] (.add stopped (dap/suspension snapshot)))
                                   :on-error (fn [_ _ exception] (future/fail! ready exception))})]
                (try
                  (is (identical? session (await! ready)))
                  (let [snapshot (take-event! stopped)
                        frame (first (dap/stack session snapshot))
                        locals (dap/frame-variables session snapshot (:id frame))
                        globals (coll/first-where #(= "_G" (:name %)) locals)
                        global-variables (dap/variables session snapshot (:variablesReference globals))
                        data (coll/first-where #(= "data" (:name %)) locals)
                        nested (first (dap/variables session snapshot (:variablesReference data)))]
                    (is (= "/main.lua" (:file frame)))
                    (is (= 8 (:line frame)))
                    (is (= "\"Lua 5.1\"" (:value (coll/first-where #(= "_VERSION" (:name %)) global-variables))))
                    (is (pos? (:variablesReference (coll/first-where #(= "math" (:name %)) global-variables))))
                    (is (= "42" (:result (dap/evaluate! session (:id frame) "data.nested.value"))))
                    (is (= "42" (:value (first (dap/variables session snapshot (:variablesReference nested)))))))
                  (dap/control! session "stepIn")
                  (is (= "work" (:function (first (dap/stack session (take-event! stopped))))))
                  (doseq [[line expected] [[4 "43"] [5 "44"] [6 "45"]]]
                    (dap/control! session "next")
                    (let [snapshot (take-event! stopped)
                          frame (first (dap/stack session snapshot))
                          locals (dap/frame-variables session snapshot (:id frame))]
                      (is (= ["work" line] [(:function frame) (:line frame)]))
                      (is (= expected (:value (coll/first-where #(= "result" (:name %)) locals))))
                      (is (= expected (:result (dap/evaluate! session (:id frame) "result"))))))
                  (dap/disconnect! session)
                  (is (.waitFor process 10 TimeUnit/SECONDS))
                  (is (zero? (.exitValue process)))
                  (finally
                    (dap/close! session)))))
            (finally
              (.destroyForcibly process))))
        (finally
          (.delete source)
          (.delete directory))))))
