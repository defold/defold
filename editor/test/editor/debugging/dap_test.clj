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

(defn- await! [value]
  (let [result (deref value 10000 ::timeout)]
    (when (= ::timeout result)
      (throw (ex-info "Timed out waiting for debugger test" {})))
    result))

(defn- take-event! [^LinkedBlockingQueue events]
  (or (.poll events 10 TimeUnit/SECONDS)
      (throw (ex-info "Timed out waiting for debugger event" {}))))

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
  (send! out {:seq 1 :type "response" :request_seq (:seq request)
              :command (:command request) :success true :body body}))

(defn- event! [out event body]
  (send! out {:seq 1 :type "event" :event event :body body}))

(defn- with-adapter [handler f]
  (with-open [server (ServerSocket. 0 1 (InetAddress/getLoopbackAddress))]
    (let [ready (promise)
          events (LinkedBlockingQueue.)
          requests (atom [])
          adapter (future/io
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
                                "initialize" (respond! out request {:supportsConfigurationDoneRequest true})
                                "attach" (event! out "initialized" {})
                                "configurationDone" (do (respond! out request {}) (respond! out attach {}))
                                "setBreakpoints" (respond! out request {:breakpoints []})
                                "disconnect" (do (respond! out request {}) (event! out "terminated" {}))
                                (handler request in out socket))
                              (when-not (or (= "disconnect" command) (.isClosed socket))
                                (recur (if (= "attach" command) request attach)))))
                          (catch EOFException _)))))
          session (dap/connect! "127.0.0.1" #(.getLocalPort server)
                                {:local-root "/project" :breakpoints {"/main.script" [{:line 5 :condition "x > 2"}]}}
                                {:on-connected #(deliver ready %)
                                 :on-suspended (fn [session body] (.add events [:stopped (dap/suspension session) body]))
                                 :on-resumed (fn [_] (.add events [:continued]))
                                 :on-output (fn [_ body] (.add events [:output body]))
                                 :on-closed (fn [_] (.add events [:closed]))
                                 :on-error (fn [_ exception] (.add events [:error exception]))})]
      (try
        (is (identical? session (await! ready)))
        (f session requests events)
        (finally
          (dap/close! session)
          (await! adapter))))))

(deftest framing-test
  (let [message {:seq 1 :type "event" :event "output" :body {:output "héj 🦊\n"}}
        bytes (wire-bytes message)
        out (ByteArrayOutputStream.)]
    (#'dap/write-message! out message)
    (is (= message (receive! (DataInputStream. (ByteArrayInputStream. (.toByteArray out))))))
    (testing "A UTF-8 message can arrive one byte at a time, followed immediately by another"
      (let [index (atom -1)
            twice (byte-array (into (vec bytes) bytes))
            next-byte (fn []
                        (let [i (swap! index inc)]
                          (if (< i (alength twice))
                            (bit-and 255 (aget twice i))
                            -1)))
            in (proxy [java.io.InputStream] []
                 (read
                   ([] (next-byte))
                   ([buffer offset length]
                    (if (zero? length)
                      0
                      (let [value (next-byte)]
                        (if (= -1 value)
                          -1
                          (do (aset-byte buffer offset (unchecked-byte value)) 1)))))))]
        (is (= message (#'dap/read-message! in)))
        (is (= message (#'dap/read-message! in)))
        (is (thrown? EOFException (#'dap/read-message! in))))))
  (testing "Reject incomplete framing without treating it as a response"
    (doseq [text ["\r\n\r\n" "Content-Length: 4\r\n\r\n{}"]]
      (is (thrown? IOException
                   (#'dap/read-message! (ByteArrayInputStream. (.getBytes ^String text StandardCharsets/UTF_8))))))))

(deftest configuration-and-breakpoints-test
  (with-adapter
    (fn [request _ out _] (respond! out request {:threads [{:id 7 :name "Lua"}]}))
    (fn [session requests events]
      (is (= ["initialize" "attach" "setBreakpoints" "configurationDone"]
             (mapv :command @requests)))
      (is (= {:localRoot "/project" :stopOnEntry false} (:arguments (second @requests))))
      (is (= :running (dap/state session)))
      (testing "Replace changed sources and clear the last breakpoint without pausing Lua"
        (await! (dap/set-breakpoints! session {"/other.lua" [{:line 9}]}))
        (let [updates (subvec @requests 4)]
          (is (= #{["/main.script" []] ["/other.lua" [{:line 9}]]}
                 (into #{} (map #(vector (get-in % [:arguments :source :path])
                                        (get-in % [:arguments :breakpoints]))) updates)))
          (is (coll/every? #(= "setBreakpoints" (:command %)) updates)))
        (await! (dap/set-breakpoints! session {"/other.lua" [{:line 9}]}))
        (is (= 6 (count @requests))))
      (await! (dap/disconnect! session))
      (is (= [:closed] (take-event! events)))
      (is (= false (get-in (peek @requests) [:arguments :terminateDebuggee])))
      (dap/close! session)
      (is (nil? (.poll ^LinkedBlockingQueue events))))))

(deftest inspection-and-control-test
  (with-adapter
    (fn [{:keys [command arguments] :as request} _ out _]
      (case command
        "threads" (respond! out request {:threads [{:id 7 :name "Lua"}]})
        "pause" (do (respond! out request {})
                    (event! out "stopped" {:threadId 7 :reason "pause"}))
        "stackTrace" (respond! out request {:stackFrames [{:id 42 :name "update" :line 5 :source {:path "/project/main.script"}}
                                                          {:id 99 :name "native" :line 0}]})
        "scopes" (do (is (= 42 (:frameId arguments)))
                     (respond! out request {:scopes [{:name "Locals" :variablesReference 10 :expensive false}
                                                     {:name "Upvalues" :variablesReference 11 :expensive false}
                                                     {:name "Globals" :variablesReference 12 :expensive true}]}))
        "variables" (respond! out request {:variables (case (long (:variablesReference arguments))
                                                        10 [{:name "self" :value "table: 1" :variablesReference 20}]
                                                        11 [{:name "flag" :value "false" :variablesReference 0}]
                                                        20 [{:name "[\"café\"]" :value "42" :variablesReference 0}])})
        "evaluate" (do (is (= 42 (:frameId arguments)))
                       (is (= "repl" (:context arguments)))
                       (respond! out request {:result "false" :variablesReference 0}))
        ("next" "stepIn" "stepOut") (do
                                      (is (= 7 (:threadId arguments)))
                                      (respond! out request {})
                                      (event! out "continued" {:threadId 7})
                                      (event! out "stopped" {:threadId 7 :reason "step"}))
        "continue" (do (respond! out request {}) (event! out "continued" {:threadId 7}))))
    (fn [session _ events]
      (await! (dap/control! session "pause"))
      (let [[event snapshot] (take-event! events)]
        (is (= :stopped event))
        (is (= [{:id 42 :function "update" :file "/main.script" :line 5}
                {:id 99 :function "native" :file nil :line 0}]
               (dap/stack session snapshot)))
        (is (= ["self" "flag"] (mapv :name (dap/frame-variables session snapshot 42))))
        (is (= "42" (:value (first (dap/variables session snapshot 20)))))
        (is (= "false" (:result (dap/evaluate! session 42 "flag"))))
        (doseq [command ["next" "stepIn" "stepOut"]]
          (await! (dap/control! session command))
          (is (= [:continued] (take-event! events)))
          (is (= :stopped (first (take-event! events))))
          (is (nil? (dap/variables session snapshot 20))))
        (await! (dap/control! session "continue"))
        (is (= [:continued] (take-event! events)))
        (is (nil? (dap/suspension session)))
        (is (nil? (dap/stack session nil)))))))

(deftest responses-and-disconnect-test
  (with-adapter
    (fn [request in out ^Socket socket]
      (case (:command request)
        "first" (let [second-request (receive! in)]
                  (event! out "output" {:output "héj 🦊"})
                  (respond! out second-request {:result "second"})
                  (respond! out request {:result "first"}))
        "bad" (send! out {:seq 1 :type "response" :request_seq (:seq request)
                           :command "bad" :success false :message "Evaluation failed"})
        "close" (.close socket)))
    (fn [session _ events]
      (let [first-response (#'dap/send-request! session "first" {})
            second-response (#'dap/send-request! session "second" {})]
        (is (= {:result "first"} (#'dap/await-response! session "first" first-response)))
        (is (= {:result "second"} (#'dap/await-response! session "second" second-response)))
        (is (= [:output {:output "héj 🦊"}] (take-event! events))))
      (is (thrown-with-msg? Exception #"Evaluation failed" (dap/request! session "bad" {})))
      (is (= :running (dap/state session)))
      (is (thrown? IOException (dap/request! session "close" {})))
      (is (= [:closed] (take-event! events)))
      (is (= :closed (dap/state session)))
      (is (= {} @(:pending session))))))

(deftest source-paths-test
  (doseq [[root path expected] [["/project" "/project/a.script" "/a.script"]
                              ["/project/" "/project/a.script" "/a.script"]
                              ["/project" "/project-other/a.lua" "/project-other/a.lua"]
                              ["C:\\project" "c:/project/main.script" "/main.script"]
                              ["/project" nil nil]]]
    (is (= expected (#'dap/source-path->project-path root path)))))

(deftest connection-cancellation-test
  (let [closed (promise)
        errors (atom [])
        session (dap/connect! "127.0.0.1" (constantly nil)
                              {:local-root "/project" :breakpoints {}}
                              {:on-closed #(deliver closed %)
                               :on-error (fn [_ error] (swap! errors conj error))})]
    (await! (dap/disconnect! session))
    (is (identical? session (await! closed)))
    (is (= :closed (dap/state session)))
    (is (= [] @errors))
    (testing "A queued stopped event cannot reopen a cancelled session"
      (#'dap/handle-event! session {:event "stopped" :body {:threadId 7}})
      (is (= :closed (dap/state session))))))

(deftest handshake-disconnect-test
  (with-open [server (ServerSocket. 0 1 (InetAddress/getLoopbackAddress))]
    (let [adapter (future/io
                    (with-open [socket (.accept server)]
                      (receive! (DataInputStream. (.getInputStream socket)))))
          closed (promise)
          error (promise)
          session (dap/connect! "127.0.0.1" #(.getLocalPort server)
                                {:local-root "/project" :breakpoints {}}
                                {:on-closed #(deliver closed %)
                                 :on-error (fn [_ exception] (deliver error exception))})]
      (try
        (is (instance? IOException (await! error)))
        (is (identical? session (await! closed)))
        (is (= :closed (dap/state session)))
        (finally
          (dap/close! session)
          (await! adapter))))))

;; Set -Ddefold.dap.debuggee to a built dap_debuggee or dap_debuggee_engine
;; executable to exercise this editor client against the real native server.
(deftest native-debugger-test
  (when-let [binary (System/getProperty "defold.dap.debuggee")]
    (let [directory (.toFile (Files/createTempDirectory "editor-dap-" (make-array java.nio.file.attribute.FileAttribute 0)))
          source (io/file directory "main.lua")
          ready (promise)
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
              (let [port (loop []
                           (let [line (.readLine reader)]
                             (when-not line (throw (IOException. "Native debugger exited before listening")))
                             (if-let [[_ port] (re-find #"(?:PORT |listening on [^:]+:)(\d+)" line)]
                               (parse-long port)
                               (recur))))
                    session (dap/connect! "127.0.0.1" (constantly port)
                                          {:local-root (.getAbsolutePath directory)
                                           :breakpoints {(.getAbsolutePath source) [{:line 8}]}}
                                          {:on-connected #(deliver ready %)
                                           :on-suspended (fn [session _] (.add stopped (dap/suspension session)))
                                           :on-error (fn [_ exception] (deliver ready exception))})]
                (try
                  (is (identical? session (await! ready)))
                  (let [snapshot (take-event! stopped)
                        frame (first (dap/stack session snapshot))
                        locals (dap/frame-variables session snapshot (:id frame))
                        data (first (filterv #(= "data" (:name %)) locals))
                        nested (first (dap/variables session snapshot (:variablesReference data)))]
                    (is (= "/main.lua" (:file frame)))
                    (is (= 8 (:line frame)))
                    (is (= "42" (:result (dap/evaluate! session (:id frame) "data.nested.value"))))
                    (is (= "42" (:value (first (dap/variables session snapshot (:variablesReference nested)))))))
                  (await! (dap/control! session "stepIn"))
                  (is (= "work" (:function (first (dap/stack session (take-event! stopped))))))
                  (doseq [[line expected] [[4 "43"] [5 "44"] [6 "45"]]]
                    (await! (dap/control! session "next"))
                    (let [snapshot (take-event! stopped)
                          frame (first (dap/stack session snapshot))
                          locals (dap/frame-variables session snapshot (:id frame))]
                      (is (= ["work" line] [(:function frame) (:line frame)]))
                      (is (= expected (:value (coll/first-where #(= "result" (:name %)) locals))))
                      (is (= expected (:result (dap/evaluate! session (:id frame) "result"))))))
                  (await! (dap/disconnect! session))
                  (is (.waitFor process 10 TimeUnit/SECONDS))
                  (is (zero? (.exitValue process)))
                  (finally (dap/close! session)))))
            (finally (.destroyForcibly process))))
        (finally
          (.delete source)
          (.delete directory))))))
