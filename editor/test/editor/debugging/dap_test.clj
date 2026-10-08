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
  (:require [clojure.string :as string]
            [clojure.test :refer :all]
            [editor.debugging.dap :as dap]
            [editor.future :as future]
            [editor.ui :as ui]
            [support.dap-util :as dap-util]
            [util.coll :as coll])
  (:import [java.io DataInputStream EOFException IOException OutputStream]
           [java.net InetAddress ServerSocket Socket]
           [java.nio.charset StandardCharsets]
           [java.util.concurrent CountDownLatch LinkedBlockingQueue]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

;; Watches expose real debugger state changes, ignore transport-only traffic, and stop notifying after removal.
(deftest session-watches-test
  (dap-util/with-adapter
    "/project"
    {}
    (fn [request _ out _]
      (case (:command request)
        "evaluate" (do
                     (if (= "stop" (get-in request [:arguments :expression]))
                       (dap-util/event! out "stopped" {:threadId 7})
                       (dap-util/event! out "output" {:output "transport-only"}))
                     (dap-util/respond! out request {}))
        "continue" (do
                     (dap-util/event! out "continued" {:threadId 7})
                     (dap-util/respond! out request {}))))
    (let [initial @session
          changes (atom [])
          callback (fn [key reference old-state new-state]
                     (swap! changes conj [key reference old-state new-state]))]
      (is (identical? session (add-watch session ::watch callback)))
      (dap/evaluate! session nil "output")
      (is (= [:output {:output "transport-only"}] (dap-util/take-event! events)))
      (is (= [] @changes))
      (dap/evaluate! session nil "stop")
      (is (= :stopped (first (dap-util/take-event! events))))
      (is (= [[::watch session initial @session]] @changes))
      (is (= :suspended (dap/status session)))
      (is (identical? session (remove-watch session ::watch)))
      (dap/control! session "continue")
      (is (= [:continued] (dap-util/take-event! events)))
      (is (= :running (dap/status session)))
      (is (= 1 (count @changes))))))

;; UTF-8 requests and consecutive fragmented events retain their byte framing.
(deftest framing-test
  (dap-util/with-adapter
    "/project"
    {}
    (fn [request _ ^OutputStream out _]
      (is (= "héj 🦊" (get-in request [:arguments :expression])))
      (doseq [text ["héj 🦊" "next"]
              byte (dap-util/wire-bytes {:seq 1 :type "event" :event "output" :body {:output text}})]
        (.write out (bit-and 255 (long byte))))
      (.flush out)
      (dap-util/respond! out request {:result "héj 🦊"}))
    (is (= "héj 🦊" (:result (dap/evaluate! session nil "héj 🦊"))))
    (is (= [:output {:output "héj 🦊"}] (dap-util/take-event! events)))
    (is (= [:output {:output "next"}] (dap-util/take-event! events)))))

;; Initialization configures breakpoints; edits replace sources and detach preserves the debuggee.
(deftest configuration-and-breakpoints-test
  (dap-util/with-adapter
    "/project"
    {:breakpoints {"/main.script" [{:line 5 :condition "x > 2"}]}}
    (fn [request _ out _]
      (dap-util/respond! out request {:threads [{:id 7
                                                 :name "Lua"}]}))
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
                                       (get-in % [:arguments :breakpoints]))) updates))))
      (dap/set-breakpoints! session
                            {"/other.lua" [{:line 9}]})
      (dap/evaluate! session nil "nil")
      (is (= 3 (count (filterv #(= "setBreakpoints" (:command %)) @requests)))))
    (dap/disconnect! session)
    (is (apply < (mapv :seq @requests)))
    (is (= [:closed] (dap-util/take-event! events)))
    (is (= false (get-in (peek @requests) [:arguments :terminateDebuggee])))
    (dap/close! session)
    (is (nil? (.poll ^LinkedBlockingQueue events)))))

;; Inspection and control use the current frame/thread and invalidate references on resume.
(deftest inspection-and-control-test
  (dap-util/with-adapter
    "/project"
    {}
    (fn [{:keys [command arguments] :as request} _ out _]
      (case command
        "threads"
        (dap-util/respond! out request {:threads [{:id 7
                                                   :name "Lua"}]})

        "pause"
        (do
          (dap-util/respond! out request {})
          (dap-util/event! out "stopped" {:threadId 7
                                          :reason "pause"}))

        "stackTrace"
        (dap-util/respond! out request {:stackFrames [{:id 42
                                                       :name "update"
                                                       :line 5
                                                       :source {:path "/project/main.script"}}
                                                      {:id 99
                                                       :name "native"
                                                       :line 0}]})

        "scopes"
        (do
          (is (= 42 (:frameId arguments)))
          (dap-util/respond! out request {:scopes [{:name "Locals"
                                                    :variablesReference 10
                                                    :expensive false}
                                                   {:name "Upvalues"
                                                    :variablesReference 11
                                                    :expensive false}
                                                   {:name "Globals"
                                                    :variablesReference 12
                                                    :expensive true}]}))

        "variables"
        (dap-util/respond! out request {:variables (case (long (:variablesReference arguments))
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
          (dap-util/respond! out request {:result "false"
                                          :variablesReference 0}))

        ("next" "stepIn" "stepOut")
        (do
          (is (= 7 (:threadId arguments)))
          (dap-util/respond! out request {})
          (dap-util/event! out "continued" {:threadId 7})
          (dap-util/event! out "stopped" {:threadId 7
                                          :reason "step"}))

        "continue"
        (do
          (dap-util/respond! out request {})
          (dap-util/event! out "continued" {:threadId 7}))))
    (dap/control! session "pause")
    (let [[event snapshot] (dap-util/take-event! events)]
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
        (is (= [:continued] (dap-util/take-event! events)))
        (is (= :stopped (first (dap-util/take-event! events))))
        (is (nil? (dap/variables session snapshot 20))))
      (dap/control! session "continue")
      (is (= [:continued] (dap-util/take-event! events)))
      (is (nil? (dap/suspension @session)))
      (is (nil? (dap/stack session nil))))))

;; Table evaluation preserves nested contents and bounds shared/cyclic references.
(deftest evaluation-result-contents-test
  (let [children {1 [{:name "[1]" :value "false" :variablesReference 0}
                     {:name "nested" :value "table: nested" :variablesReference 2}
                     {:name "alias" :value "table: nested" :variablesReference 2}
                     {:name "self" :value "table: root" :variablesReference 1}
                     {:name "empty" :value "table: empty" :variablesReference 3}]
                  2 [{:name "[\"end\"]" :value "42" :variablesReference 0}]
                  3 []}]
    (dap-util/with-adapter
      "/project"
      {}
      (fn [{:keys [command arguments] :as request} _ out _]
        (case command
          "evaluate" (do (dap-util/event! out "stopped" {:threadId 7})
                         (dap-util/respond! out request {:result "table: root" :variablesReference 1}))
          "variables" (dap-util/respond! out request {:variables (get children (:variablesReference arguments))})))
      (let [result (dap/evaluate! session 42 "table")
            [_ snapshot] (dap-util/take-event! events)]
        (doseq [value ["false" "nil" "42" "\"hello\""]]
          (is (= value (dap/evaluation-result->string session snapshot {:result value :variablesReference 0}))))
        (is (= (coll/join-to-string "\n" ["{ -- table: root"
                                          "  [1] = false,"
                                          "  nested = { -- table: nested"
                                          "    [\"end\"] = 42"
                                          "  },"
                                          "  alias = table: nested,"
                                          "  self = table: root,"
                                          "  empty = { -- table: empty"
                                          "  }"
                                          "}"])
               (dap/evaluation-result->string session snapshot result)))
        (is (= [1 2 3] (into [] (comp (filter #(= "variables" (:command %)))
                                      (map #(get-in % [:arguments :variablesReference]))) @requests)))))))

;; Recursive result printing is bounded, and resume discards a partially loaded result.
(deftest evaluation-result-lifetime-test
  (doseq [resume [false true]]
    (dap-util/with-adapter
      "/project"
      {}
      (fn [{:keys [command arguments] :as request} _ out _]
        (case command
          "evaluate" (do (dap-util/event! out "stopped" {:threadId 7})
                         (dap-util/respond! out request {:result "table: 1" :variablesReference 1}))
          "variables" (let [reference (long (:variablesReference arguments))]
                        (when resume (dap-util/event! out "continued" {:threadId 7}))
                        (dap-util/respond! out request {:variables [{:name "child"
                                                                     :value (str "table: " (inc reference))
                                                                     :variablesReference (inc reference)}]}))))
      (let [result (dap/evaluate! session 42 "table")
            [_ snapshot] (dap-util/take-event! events)
            output (dap/evaluation-result->string session snapshot result)
            references (into [] (comp (filter #(= "variables" (:command %)))
                                      (map #(get-in % [:arguments :variablesReference]))) @requests)]
        (if resume
          (do (is (= "table: 1" output))
              (is (= [1] references))
              (is (= [:continued] (dap-util/take-event! events)))
              (is (= "table: 1" (dap/evaluation-result->string session snapshot result))))
          (do (is (string/includes? output "child = table: 17"))
              (is (= (vec (range 1 17)) references))))))))

;; Resume during stack, scopes or variables retrieval discards stale frame contents.
(deftest stale-frame-variables-test
  (doseq [resume-command ["stackTrace" "scopes" "variables"]]
    (testing (str "Resume during " resume-command)
      (dap-util/with-adapter
        "/project"
        {}
        (fn [{:keys [command] :as request} _ out _]
          (when (= resume-command command)
            (dap-util/event! out "continued" {:threadId 7}))
          (case command
            "stackTrace"
            (dap-util/respond! out request {:stackFrames [{:id 42 :name "old" :line 5}]})

            "threads"
            (dap-util/respond! out request {:threads [{:id 7 :name "Lua"}]})

            "pause"
            (do
              (dap-util/respond! out request {})
              (dap-util/event! out "stopped" {:threadId 7 :reason "pause"}))

            "scopes"
            (dap-util/respond! out request
                               {:scopes [{:name "Locals" :variablesReference 10 :expensive false}
                                         {:name "Globals" :variablesReference 20 :expensive true}]})

            "variables"
            (dap-util/respond! out request
                               {:variables [{:name "old" :value "table" :variablesReference 30}]})))
        (dap/control! session "pause")
        (let [[event snapshot] (dap-util/take-event! events)]
          (is (= :stopped event))
          (is (nil? (if (= "stackTrace" resume-command)
                      (dap/stack session snapshot)
                      (dap/frame-variables session snapshot 42))))
          (is (= [:continued] (dap-util/take-event! events)))
          (is (nil? (dap/suspension @session))))))))

;; Concurrent evaluations correlate out-of-order replies; request failure leaves the session usable.
(deftest responses-and-disconnect-test
  (let [first-received (promise)]
    (dap-util/with-adapter
      "/project"
      {}
      (fn [request in out ^Socket socket]
        (case (get-in request [:arguments :expression])
          "first" (do (deliver first-received true)
                      (let [second-request (dap-util/receive! in)]
                        (dap-util/event! out "output" {:output "héj 🦊"})
                        (dap-util/respond! out second-request {:result "second"})
                        (dap-util/respond! out request {:result "first"})))
          "bad" (dap-util/reject! out request "Evaluation failed")
          "close" (.close socket)))
      (let [first-response (future/io (dap/evaluate! session nil "first"))]
        (dap-util/await! first-received)
        (let [second-response (future/io (dap/evaluate! session nil "second"))]
          (is (= {:result "first"} (dap-util/await! first-response)))
          (is (= {:result "second"} (dap-util/await! second-response)))))
      (is (= [:output {:output "héj 🦊"}] (dap-util/take-event! events)))
      (is (thrown-with-msg? IOException #"Evaluation failed" (dap/evaluate! session nil "bad")))
      (is (= :running (dap/status session)))
      (is (thrown? IOException (dap/evaluate! session nil "close")))
      (is (= [:closed] (dap-util/take-event! events)))
      (is (= :closed (dap/status session))))))

;; A successful response immediately before EOF is delivered, including a nil body.
(deftest response-before-eof-test
  (doseq [body [{:result "last response"} nil]]
    (dap-util/with-adapter
      "/project"
      {}
      (fn [request _ out ^Socket socket]
        (dap-util/respond! out request body)
        (.close socket))
      (is (= body (dap/evaluate! session nil "last")))
      (is (= [:closed] (dap-util/take-event! events)))
      (dap/close! session)
      (is (= :closed (dap/status session))))))

;; UI callbacks can schedule public operations without blocking later notifications.
(deftest callbacks-can-schedule-requests-test
  (let [result (future/make)
        second-output (promise)
        order (atom [])]
    (dap-util/with-adapter
      "/project"
      {:on-output (fn [session snapshot {:keys [output]}]
                    (is (ui/on-ui-thread?))
                    (is (= :running (:status snapshot)))
                    (swap! order conj output)
                    (if (= "first" output)
                      (future/io (future/complete! result (dap/evaluate! session nil "from-callback")))
                      (deliver second-output true)))}
      (fn [request _ out _]
        (case (get-in request [:arguments :expression])
          "trigger" (do (dap-util/event! out "output" {:output "first"})
                        (dap-util/event! out "output" {:output "second"})
                        (dap-util/respond! out request {}))
          "from-callback" (dap-util/respond! out request {:result "callback response"})))
      (is (= {} (dap/evaluate! session nil "trigger")))
      (is (= {:result "callback response"} (dap-util/await! result)))
      (is (dap-util/await! second-output))
      (is (= ["first" "second"] @order)))))

;; Callbacks retain event snapshots and wire order even when events arrive together.
(deftest callbacks-observe-event-state-test
  (let [observed (atom [])
        closed (promise)]
    (dap-util/with-adapter
      "/project"
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
        (dap-util/respond! out request {})
        (dap-util/event! out "stopped" {:threadId 7})
        (dap-util/event! out "continued" {})
        (dap-util/event! out "terminated" {}))
      (is (= {} (dap/evaluate! session nil "trigger")))
      (is (dap-util/await! closed))
      (dap/close! session)
      (is (= [[:stopped :suspended {:threadId 7}]
              [:continued :running]
              [:closed :closed]]
             @observed))
      (is (= :closed (dap/status session))))))

;; An unanswered public request times out, closes the session and rejects subsequent work.
(deftest request-timeout-closes-session-test
  (dap-util/with-adapter
    "/project"
    {}
    (fn [_ _ _ _])
    (is (thrown-with-msg? IOException #"timed out" (dap/evaluate! session nil "wait")))
    (let [[event exception] (dap-util/take-event! events)]
      (is (= :error event))
      (is (instance? IOException exception)))
    (is (= [:closed] (dap-util/take-event! events)))
    (is (= :closed (dap/status session)))
    (is (thrown? IOException (dap/evaluate! session nil "after-close")))))

;; Malformed/truncated wire messages fail waiting requests and report an error rather than clean EOF.
(deftest protocol-error-closes-session-test
  (doseq [fragment ["\r\n\r\n" "Content-Length: 2" "Content-Length: 4\r\n\r\n{}"
                    "Content-Length: 2\r\ncontent-length: 2\r\n\r\n{}"
                    "Content-Length: invalid\r\n\r\n{}" "Content-Length: 0\r\n\r\n"
                    "Content-Length: -1\r\n\r\n" "Content-Length: 9223372036854775808\r\n\r\n"
                    "Content-Length: 1048577\r\n\r\n"
                    (str "X-Header: " (.repeat "x" 4096) "\r\n\r\n")
                    "Content-Length: 1\r\n\r\n{"
                    "Content-Length: 2\r\n\r\n[]"
                    "Content-Length: 2\r\n\r\n{}"
                    "Content-Length: 21\r\n\r\n{\"type\":\"unexpected\"}"]]
    (dap-util/with-adapter
      "/project"
      {}
      (fn [_ _ ^OutputStream out ^Socket socket]
        (.write out (.getBytes ^String fragment StandardCharsets/US_ASCII))
        (.flush out)
        (.close socket))
      (is (thrown? IOException (dap/evaluate! session nil "waiting")))
      (let [notifications (loop [notifications []]
                            (let [event (dap-util/take-event! events)
                                  notifications (conj notifications event)]
                              (if (= :closed (first event))
                                notifications
                                (recur notifications))))]
        (is (= [:error :closed] (mapv first notifications)))
        (is (instance? Throwable (second (first notifications)))))
      (is (= :closed (dap/status session)))
      (is (thrown? IOException (dap/evaluate! session nil "after-close"))))))

;; Stack frames map source paths through the public inspection API.
(deftest source-paths-test
  (doseq [[root path expected] [["/project" "/project/a.script" "/a.script"]
                                ["/project/" "/project/a.script" "/a.script"]
                                ["/project" "/project-other/a.lua" "/project-other/a.lua"]
                                ["C:\\project" "c:/project/main.script" "/main.script"]
                                ["/project" nil nil]]]
    (dap-util/with-adapter
      root
      {}
      (fn [{:keys [command] :as request} _ out _]
        (case command
          "evaluate" (do (dap-util/event! out "stopped" {:threadId 7}) (dap-util/respond! out request {}))
          "stackTrace" (dap-util/respond! out request {:stackFrames [{:id 1 :name "Lua" :line 1 :source {:path path}}]})))
      (dap/evaluate! session nil "stop")
      (let [[_ snapshot] (dap-util/take-event! events)]
        (is (= [expected] (mapv :file (dap/stack session snapshot))))))))

;; Disconnect cancels pending port discovery without reporting a connection error.
(deftest connection-cancellation-test
  (let [closed (future/make)
        errors (atom [])
        session
        (dap/connect! "127.0.0.1" (constantly nil) "/project"
                      :on-closed (fn [session _] (future/complete! closed session))
                      :on-error (fn [_ _ error] (swap! errors conj error)))]
    (dap/disconnect! session)
    (is (identical? session (dap-util/await! closed)))
    (is (= :closed (dap/status session)))
    (is (= [] @errors))))

;; A server disappearing during initialization reports attachment failure and closes the session.
(deftest handshake-disconnect-test
  (with-open [server (ServerSocket. 0 1 (InetAddress/getLoopbackAddress))]
    (let [adapter
          (future/io
            (with-open [socket (.accept server)]
              (dap-util/receive! (DataInputStream. (.getInputStream socket)))))
          closed (future/make)
          error (future/make)
          session
          (dap/connect! "127.0.0.1" #(.getLocalPort server) "/project"
                        :on-closed (fn [session _] (future/complete! closed session))
                        :on-error (fn [_ _ exception] (future/complete! error exception)))]
      (try
        (is (instance? IOException (dap-util/await! error)))
        (is (identical? session (dap-util/await! closed)))
        (is (= :closed (dap/status session)))
        (finally
          (dap/close! session)
          (dap-util/await! adapter))))))

;; Closing a stalled handshake closes the socket and rejects concurrent evaluation.
(deftest close-during-initialization-test
  (with-open [server (ServerSocket. 0 1 (InetAddress/getLoopbackAddress))]
    (let [initializing (promise)
          adapter (future/io
                    (with-open [socket (.accept server)]
                      (let [in (DataInputStream. (.getInputStream socket))]
                        (is (= "initialize" (:command (dap-util/receive! in))))
                        (deliver initializing true)
                        (is (thrown? EOFException (dap-util/receive! in))))))
          session (dap/connect! "127.0.0.1" #(.getLocalPort server) "/project")]
      (try
        (dap-util/await! initializing)
        (let [evaluation (future/io (dap/evaluate! session nil "concurrent"))]
          (dap-util/await! (future/io (dap/close! session)))
          (is (thrown? IOException (dap-util/await! evaluation)))
          (is (= :closed (dap/status session))))
        (dap-util/await! adapter)
        (finally (.close server) (dap-util/await! (future/io (dap/close! session))))))))

;; Breakpoint edits during initialization reach the adapter; a stop during configuration stays suspended.
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
              (let [in (DataInputStream. (.getInputStream socket))
                    out (.getOutputStream socket)
                    request (dap-util/receive! in)]
                (is (= "initialize" (:command request)))
                (future/complete! initializing true)
                (dap-util/await! initialize)
                (dap-util/respond! out request {:supportsConfigurationDoneRequest true})
                (let [attach (dap-util/receive! in)]
                  (is (= "attach" (:command attach)))
                  (dap-util/event! out "initialized" {})
                  (let [first-request (dap-util/receive! in)]
                    (is (= "setBreakpoints" (:command first-request)))
                    (is (= [{:line 9}] (get-in first-request [:arguments :breakpoints])))
                    (future/complete! first-edit true)
                    (dap-util/await! acknowledge)
                    (dap-util/respond! out first-request {})
                    (let [second-request (dap-util/receive! in)]
                      (is (= "setBreakpoints" (:command second-request)))
                      (is (= [{:line 11}] (get-in second-request [:arguments :breakpoints])))
                      (dap-util/respond! out second-request {})
                      (future/complete! second-edit true)))
                  (let [configuration (dap-util/receive! in)]
                    (is (= "configurationDone" (:command configuration)))
                    (dap-util/event! out "stopped" {:threadId 7 :reason "entry"})
                    (dap-util/respond! out configuration {})
                    (dap-util/respond! out attach {}))
                  (let [disconnect (dap-util/receive! in)]
                    (is (= "disconnect" (:command disconnect)))
                    (is (false? (get-in disconnect [:arguments :terminateDebuggee])))
                    (dap-util/respond! out disconnect {}))))))
          session (dap/connect! "127.0.0.1" #(.getLocalPort server) "/project"
                                :breakpoints {"/main.script" [{:line 5}]}
                                :on-connected (fn [session _] (future/complete! ready session))
                                :on-error (fn [_ _ error] (future/fail! ready error)))]
      (try
        (dap-util/await! initializing)
        (dap/set-breakpoints! session {"/main.script" [{:line 9}]})
        (future/complete! initialize true)
        (dap-util/await! first-edit)
        (let [updated (dap/set-breakpoints! session {"/main.script" [{:line 11}]})]
          (is (identical? session updated))
          (future/complete! acknowledge true)
          (dap-util/await! second-edit))
        (is (identical? session (dap-util/await! ready)))
        (is (= :suspended (dap/status session)))
        (dap/disconnect! session)
        (dap-util/await! adapter)
        (finally
          (future/complete! initialize true)
          (future/complete! acknowledge true)
          (dap/close! session))))))

;; Closing releases an outstanding evaluation, rejects later work and notifies once.
(deftest close-settles-work-test
  (let [received (promise)]
    (dap-util/with-adapter
      "/project"
      {}
      (fn [_ _ _ _] (deliver received true))
      (let [evaluation (future/io (dap/evaluate! session nil "waiting"))]
        (dap-util/await! received)
        (dap-util/await! (future/io (dap/close! session)))
        (is (thrown? IOException (dap-util/await! evaluation)))
        (is (thrown? IOException (dap/evaluate! session nil "late")))
        (is (= [:closed] (dap-util/take-event! events)))
        (dap/close! session)
        (ui/run-now nil)
        (is (nil? (.poll ^LinkedBlockingQueue events)))
        (is (= :closed (dap/status session)))))))

;; Pending breakpoint edits coalesce and a following evaluation observes completed synchronization.
(deftest pending-breakpoint-edits-coalesce-test
  (let [started (promise)
        release (CountDownLatch. 1)]
    (dap-util/with-adapter
      "/project"
      {:breakpoints {"/main.script" [{:line 5}]}}
      (fn [{:keys [command arguments] :as request} _ out _]
        (when (and (= "setBreakpoints" command) (= 9 (get-in arguments [:breakpoints 0 :line])))
          (deliver started true)
          (.await release))
        (dap-util/respond! out request {}))
      (try
        (dap/set-breakpoints! session {"/main.script" [{:line 9}]})
        (dap-util/await! started)
        (dap/set-breakpoints! session {"/main.script" [{:line 11}]})
        (dap/set-breakpoints! session {"/main.script" [{:line 13}]})
        (finally (.countDown release)))
      (is (= {} (dap/evaluate! session nil "after-edit")))
      (is (= [5 9 13] (into [] (comp (filter #(= "setBreakpoints" (:command %)))
                                     (map #(get-in % [:arguments :breakpoints 0 :line]))) @requests)))
      (is (= "evaluate" (:command (peek @requests)))))))

;; Close rejects work immediately but waits for an interrupted resolver's cleanup.
(deftest close-joins-connection-test
  (let [started (promise)
        interrupted (promise)
        release (CountDownLatch. 1)
        closed (promise)
        errors (atom [])
        finished (atom false)
        session
        (dap/connect! "127.0.0.1"
                      (fn []
                        (deliver started true)
                        (try
                          (.await (CountDownLatch. 1))
                          nil
                          (catch InterruptedException exception
                            (deliver interrupted true)
                            (.await release)
                            (reset! finished true)
                            (throw exception))))
                      "/project"
                      :on-closed (fn [_ _] (deliver closed true))
                      :on-error (fn [_ _ exception] (swap! errors conj exception)))]
    (try
      (dap-util/await! started)
      (let [closing (future/io
                      (let [result (dap/close! session)]
                        [result @finished]))]
        (dap-util/await! interrupted)
        (is (thrown? IOException (dap/evaluate! session nil "late")))
        (.countDown release)
        (is (= [nil true] (dap-util/await! closing)))
        (is (dap-util/await! closed))
        (is (= :closed (dap/status session)))
        (is (= [] @errors)))
      (finally
        (.countDown release)
        (dap-util/await! (future/io (dap/close! session)))))))

;; Closing interrupts an unanswered breakpoint workflow and rejects concurrent evaluation.
(deftest close-during-breakpoint-sync-test
  (let [started (promise)]
    (dap-util/with-adapter
      "/project"
      {}
      (fn [request _ _ _]
        (is (= "setBreakpoints" (:command request)))
        (deliver started true))
      (dap/set-breakpoints! session {"/main.script" [{:line 9}]})
      (dap-util/await! started)
      (let [evaluation (future/io (dap/evaluate! session nil "concurrent"))]
        (dap-util/await! (future/io (dap/close! session)))
        (is (thrown? IOException (dap-util/await! evaluation)))
        (is (= :closed (dap/status session)))
        (is (coll/not-any? #(= "evaluate" (:command %)) @requests))
        (is (= [:closed] (dap-util/take-event! events)))))))

;; A failed breakpoint synchronization reports the adapter error, closes the session,
;; and rejects later evaluation instead of sending it to the adapter.
(deftest breakpoint-failure-closes-session-test
  (let [started (promise)
        release (CountDownLatch. 1)]
    (dap-util/with-adapter
      "/project"
      {}
      (fn [request _ out _]
        (is (= "setBreakpoints" (:command request)))
        (deliver started true)
        (.await release)
        (dap-util/reject! out request "Breakpoint rejected"))
      (try
        (dap/set-breakpoints! session {"/main.script" [{:line 9}]})
        (dap-util/await! started)
        (finally (.countDown release)))
      (let [[event exception] (dap-util/take-event! events)]
        (is (= :error event))
        (is (= "Breakpoint rejected" (ex-message exception))))
      (is (= [:closed] (dap-util/take-event! events)))
      (is (= :closed (dap/status session)))
      (is (thrown? IOException (dap/evaluate! session nil "after-failure")))
      (is (coll/not-any? #(= "evaluate" (:command %)) @requests)))))

;; A UI callback can schedule background close without deadlocking transport cleanup.
(deftest close-scheduled-from-callback-test
  (let [closed (promise)]
    (dap-util/with-adapter
      "/project"
      {:on-output (fn [session _ _]
                    (is (ui/on-ui-thread?))
                    (future/io
                      (dap/close! session)
                      (deliver closed true)))}
      (fn [request _ out _]
        (dap-util/respond! out request {})
        (dap-util/event! out "output" {:output "close"}))
      (is (= {} (dap/evaluate! session nil "trigger")))
      (is (true? (dap-util/await! closed)))
      (is (= :closed (dap/status session)))
      (is (= [:closed] (dap-util/take-event! events))))))

;; Close finishes while the UI is held, then queued notifications are delivered.
(deftest close-does-not-join-notifications-test
  (let [started (promise)
        notified (promise)
        proceed (CountDownLatch. 1)]
    (try
      (dap-util/with-adapter
        "/project"
        {:on-output (fn [_ _ _] (deliver notified true))}
        (fn [request _ out _]
          ;; Queue the notification before the response releases the caller to close.
          (dap-util/event! out "output" {:output "queued notification"})
          (dap-util/respond! out request {}))
        (ui/run-later
          (deliver started true)
          (.await proceed))
        (dap-util/await! started)
        (is (= {} (dap/evaluate! session nil "trigger")))
        (is (nil? (dap-util/await! (future/io (dap/close! session)))))
        (is (= :closed (dap/status session)))
        (is (not (realized? notified))))
      (finally (.countDown proceed)))
    (is (dap-util/await! notified))))
