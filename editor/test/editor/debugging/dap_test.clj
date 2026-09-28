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
            [util.coll :as coll])
  (:import [java.io ByteArrayInputStream ByteArrayOutputStream DataInputStream EOFException IOException OutputStream]
           [java.net InetAddress ServerSocket Socket]
           [java.nio.charset StandardCharsets]
           [java.nio.file Files]
           [java.util.concurrent LinkedBlockingQueue TimeUnit]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(defn- await! [value]
  (let [result (deref value 10000 ::timeout)]
    (when (= ::timeout result)
      (throw (IOException. "Timed out waiting for debugger test")))
    result))

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
                        {:local-root "/project"
                         :breakpoints {"/main.script" [{:line 5
                                                        :condition "x > 2"}]}}
                        (merge {:on-connected #(future/complete! ready %)
                                :on-suspended (fn [session body] (.add events [:stopped (dap/suspension session) body]))
                                :on-resumed (fn [_] (.add events [:continued]))
                                :on-output (fn [_ body] (.add events [:output body]))
                                :on-closed (fn [_] (.add events [:closed]))
                                :on-error (fn [_ exception] (.add events [:error exception]))}
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
      (let [index (atom -1)
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
        (is (= message (#'dap/read-message! in)))
        (is (= message (#'dap/read-message! in)))
        (is (thrown? EOFException (#'dap/read-message! in))))))
  (testing "Reject incomplete framing without treating it as a response"
    (doseq [text ["\r\n\r\n" "Content-Length: 4\r\n\r\n{}"]]
      (is (thrown? IOException
                   (#'dap/read-message! (ByteArrayInputStream. (.getBytes ^String text StandardCharsets/UTF_8))))))))

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
        (let [updates (subvec @requests 4)]
          (is (= #{["/main.script" []] ["/other.lua" [{:line 9}]]}
                 (into #{} (map #(vector (get-in % [:arguments :source :path])
                                         (get-in % [:arguments :breakpoints]))) updates)))
          (is (coll/every? #(= "setBreakpoints" (:command %)) updates)))
        (dap/set-breakpoints! session
                              {"/other.lua" [{:line 9}]})
        (is (= 6 (count @requests))))
      (dap/disconnect! session)
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
        (is (nil? (dap/suspension session)))
        (is (nil? (dap/stack session nil)))))))

;; Console table results retain nested contents, Lua key names, empty tables,
;; and scalar values without repeatedly loading shared or cyclic references.
(deftest evaluation-result-contents-test
  (let [session {:state (atom {:status :suspended :generation 1 :thread-id 7})}
        snapshot (dap/suspension session)
        requests (atom [])
        children
        {1 [{:name "[1]" :value "false" :variablesReference 0}
            {:name "nested" :value "table: nested" :variablesReference 2}
            {:name "alias" :value "table: nested" :variablesReference 2}
            {:name "self" :value "table: root" :variablesReference 1}
            {:name "empty" :value "table: empty" :variablesReference 3}]
         2 [{:name "[\"end\"]" :value "42" :variablesReference 0}]
         3 []}]
    (with-redefs [dap/request!
                  (fn [_ command {:keys [variablesReference]}]
                    (is (= "variables" command))
                    (swap! requests conj variablesReference)
                    {:variables (get children variablesReference)})]
      (doseq [value ["false" "nil" "42" "\"hello\""]]
        (is (= value (dap/evaluation-result->string session snapshot {:result value :variablesReference 0}))))
      (is (= [] @requests))
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
             (dap/evaluation-result->string session snapshot {:result "table: root" :variablesReference 1})))
      (is (= [1 2 3] @requests)))))

;; Bound recursive console inspection and stop loading references when execution
;; resumes, rather than printing a partially refreshed table from another stop.
(deftest evaluation-result-lifetime-test
  (let [session {:state (atom {:status :suspended :generation 1 :thread-id 7})}
        snapshot (dap/suspension session)
        result {:result "table: 1" :variablesReference 1}
        requests (atom [])]
    (with-redefs [dap/request!
                  (fn [_ _ {:keys [variablesReference]}]
                    (swap! requests conj variablesReference)
                    {:variables [{:name "child"
                                  :value (str "table: " (inc (long variablesReference)))
                                  :variablesReference (inc (long variablesReference))}]})]
      (is (string/includes? (dap/evaluation-result->string session snapshot result) "child = table: 17"))
      (is (= (vec (range 1 17)) @requests)))
    (reset! requests [])
    (with-redefs [dap/request!
                  (fn [_ _ {:keys [variablesReference]}]
                    (swap! requests conj variablesReference)
                    (swap! (:state session) assoc :status :running :generation 2)
                    {:variables [{:name "stale" :value "42" :variablesReference 0}]})]
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
            (is (nil? (dap/suspension session)))))))))

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
      (is (thrown-with-msg? IOException #"Evaluation failed" (dap/request! session "bad" {})))
      (is (= :running (dap/status session)))
      (is (thrown? IOException (dap/request! session "close" {})))
      (is (= [:closed] (take-event! events)))
      (is (= :closed (dap/status session)))
      (is (= {} (:pending @(:state session)))))))

(deftest callbacks-can-make-blocking-requests-test
  (let [result (future/make)
        order (atom [])]
    (with-adapter
      {:on-output
       (fn [session {:keys [output]}]
         (swap! order conj output)
         (if-not (= "first" output)
           (future/complete! result @order)
           (swap! order conj (dap/request! session "from-callback" {}))))}
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
        (is (= {} (dap/request! session "trigger" {})))
        (is (= ["first" {:result "callback response"} "second"] (await! result)))))))

(deftest concurrent-requests-test
  (with-adapter
    {}
    (fn [request _ out _]
      (respond! out request (:arguments request)))
    (fn [session requests _]
      (let [responses
            (mapv (fn [index]
                    (future/io (dap/request! session "echo" {:index index})))
                  (range 32))]
        (is (= (mapv #(hash-map :index %) (range 32)) (mapv await! responses)))
        (let [sequences (mapv :seq @requests)]
          (is (= (count sequences) (count (set sequences))))
          (is (apply < sequences)))
        (is (= {} (:pending @(:state session))))))))

(deftest request-timeout-closes-session-test
  (with-adapter
    {}
    (fn [_ _ _ _])
    (fn [session _ events]
      (with-redefs-fn {#'dap/request-timeout-ms 100}
        #(is (thrown-with-msg? IOException #"timed out: wait"
                               (dap/request! session "wait" {}))))
      (let [[event exception] (take-event! events)]
        (is (= :error event))
        (is (instance? IOException exception)))
      (is (= [:closed] (take-event! events)))
      (is (= :closed (dap/status session)))
      (is (nil? (:socket @(:state session))))
      (is (= {} (:pending @(:state session))))
      (is (thrown-with-msg? IOException #"disconnected" (dap/request! session "after-close" {}))))))

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
                       :breakpoints {}}
                      {:on-closed #(future/complete! closed %)
                       :on-error (fn [_ error] (swap! errors conj error))})]
    (dap/disconnect! session)
    (is (identical? session (await! closed)))
    (is (= :closed (dap/status session)))
    (is (= [] @errors))
    (testing "A queued stopped event cannot reopen a cancelled session"
      (#'dap/handle-message! session
                             {:type "event"
                              :event "stopped"
                              :body {:threadId 7}})
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
                         :breakpoints {}}
                        {:on-closed #(future/complete! closed %)
                         :on-error (fn [_ exception] (future/complete! error exception))})]
      (try
        (is (instance? IOException (await! error)))
        (is (identical? session (await! closed)))
        (is (= :closed (dap/status session)))
        (finally
          (dap/close! session)
          (await! adapter))))))

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
                                   :breakpoints {(.getAbsolutePath source) [{:line 8}]}}
                                  {:on-connected #(future/complete! ready %)
                                   :on-suspended (fn [session _] (.add stopped (dap/suspension session)))
                                   :on-error (fn [_ exception] (future/fail! ready exception))})]
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
