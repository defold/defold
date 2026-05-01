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

(ns editor.debugging.mobdebug
  (:refer-clojure :exclude [run!])
  (:require [clojure.edn :as edn]
            [clojure.string :as string]
            [editor.error-reporting :as error-reporting]
            [editor.lua :as lua]
            [editor.luajit :refer [luajit-path-to-chunk-name chunk-name-to-luajit-path]]
            [service.log :as log])
  (:import [clojure.lang Counted ILookup MapEntry Seqable]
           [java.io BufferedReader InputStreamReader IOException PrintWriter]
           [java.net InetSocketAddress Socket]
           [java.util Stack]
           [java.util.concurrent Executors ExecutorService]))

(set! *warn-on-reflection* true)

(def ^:private ^ExecutorService executor (Executors/newCachedThreadPool))

(defn- thread-call
  [^Runnable f]
  (.submit executor f))

(defmacro thread
  [& body]
  `(thread-call (^{:once true} fn* [] (error-reporting/catch-all! ~@body))))

(defn- re-match
  "Find the first match of `re` in `s` and return a vector of all the captured
  groups, and the remaining unmatched part of `s`, if non-empty. Returns `nil`
  if `re` does not match."
  [re ^CharSequence s]
  (let [m (re-matcher re s)]
    (when (.find m)
      (loop [i 1
             ret []]
        (if (< (.groupCount m) i)
          (let [last (.end m)]
            (cond-> ret
              (< last (.length s))
              (conj (subs s last))))
          (recur (inc i) (conj ret (.group m i))))))))

;;; Protocol
;;
;; Line based over TCP.
;;
;; Commands:
;;
;; - RUN
;; - SUSPEND
;; - SETB file line
;; - DELB file line
;; - EXEC string
;; - SETW line expr
;; - DELW line
;; - STEP
;; - OVER
;; - OUT

(defn- parse-status
  [^String s]
  (if-some [[status rest] (re-match #"^(\d+)\s+" s)]
    [status rest]
    (throw (ex-info (format "Unable to parse status from reply: '%s'" s)
                    {:input s}))))

(defn- read-status
  [^BufferedReader reader]
  (if-some [line (.readLine reader)]
    (parse-status line)
    (throw (IOException. "Unable to read line from reader"))))

(defn- read-data
  [^BufferedReader reader n]
  (let [buf (char-array n)]
    (.read reader buf 0 n)
    (String. buf)))

(defn- send-command!
  [^PrintWriter out command]
  (.println out command)
  (.flush out))

(defn- remove-filename-prefix
  [^String s]
  (if (or (.startsWith s "=") (.startsWith s "@"))
    (subs s 1)
    s))

(defn lua-module?
  [^String s]
  (.matches (re-matcher #"([^\./]+)(\.[^\./]+)*" s)))

(defn- module->path
  [^String s]
  (if (lua-module? s)
    (lua/lua-module->path s)
    s))

(def sanitize-path (comp module->path chunk-name-to-luajit-path remove-filename-prefix))

;;------------------------------------------------------------------------------
;; session management

(defprotocol IDebugSession
  (-state [this] "return the session state")
  (-set-state! [this state] "set the session state")
  (-push-suspend-callback! [this f])
  (-pop-suspend-callback! [this]))

(deftype DebugSession [^Object lock
                       ^:volatile-mutable state
                       ^Socket socket
                       ^BufferedReader in
                       ^PrintWriter out
                       ^Stack suspend-callbacks
                       on-closed]
  IDebugSession
  (-state [this] state)
  (-set-state! [this new-state] (set! state new-state))
  (-push-suspend-callback! [this f] (.push suspend-callbacks f))
  (-pop-suspend-callback! [this] (.pop suspend-callbacks)))

(defn- make-debug-session
  [^Socket socket on-closed]
  (let [in  (BufferedReader. (InputStreamReader. (.getInputStream socket)))
        out (PrintWriter. (.getOutputStream socket))]
    (DebugSession. (Object.) :suspended socket in out (Stack.) on-closed)))

(defn- try-connect!
  [address port]
  (try
    (doto (Socket.)
      (.connect (InetSocketAddress. ^String address (int port)) 2000))
    (catch java.lang.Exception _ nil)))

(defn connect!
  [address port on-connected on-closed on-error]
  (thread
    (try
      (loop [retries 0]
        (if-some [socket (try-connect! address port)]
          (let [debug-session (make-debug-session socket on-closed)]
            (on-connected debug-session))
          (if (< retries 50)
            (do (Thread/sleep 200) (recur (inc retries)))
            (throw (ex-info (format "Failed to connect to debugger on %s:%d." address port)
                            {:address address
                             :port port})))))
      (catch Exception e
        (on-error e)))))

(defn close!
  ([debug-session]
   (close! debug-session :closed))
  ([^DebugSession debug-session end-state]
   (when-some [^Socket socket (.socket debug-session)]
     (try
       (.close socket)
       (catch IOException _)))
   (-set-state! debug-session end-state)
   (when-some [on-closed (.on-closed debug-session)]
     (on-closed debug-session))))

(defmacro with-session
  [debug-session & body]
  `(locking (.lock ~(with-meta debug-session {:tag `DebugSession}))
     (try
       ~@body
       (catch IOException ex#
         (close! ~debug-session))
       (catch Throwable t#
         (log/error :exception t#)
         (close! ~debug-session :error)))))

(defn state
  [^DebugSession debug-session]
  (-state debug-session))

;;--------------------------------------------------------------------
;; data decoding

;; Custom lua type those structure can't be encoded into edn, displayed as a
;; string

(defrecord LuaBlackBox [tag string])

;; Ref is an internal part of LuaStructure, needed to express circular
;; references. Shouldn't be exposed as a public API

(defrecord LuaRef [address])

(defn- tuple->map-entry
  ^MapEntry [[k v]]
  (MapEntry. k v))

(defn- sequence->map-entries [coll]
  (sequence (comp (partition-all 2)
                  (map tuple->map-entry))
            coll))

(defn- classify-type [x]
  (cond (number? x) :number
        (string? x) :string
        (instance? Comparable x) :comparable
        :else :other))

(defn- compare-keys [a b]
  (let [a-type (classify-type a)
        b-type (classify-type b)]
    (case a-type
      :number (case b-type
                :number (compare a b)
                (:string :comparable :other) -1)
      :string (case b-type
                :number 1
                :string (compare a b)
                (:comparable :other) -1)
      :comparable (case b-type
                    (:number :string) 1
                    :comparable (compare a b)
                    :other -1)
      :other (case b-type
               (:number :string :comparable) 1
               :other (compare (System/identityHashCode a)
                               (System/identityHashCode b))))))

(defn- read-lua-table [{:keys [content string]}]
  (with-meta (into (sorted-map-by compare-keys)
                   (sequence->map-entries content))
             {:string string}))

(declare ->LuaStructure)

(defn- decode-serialized-data
  [debug-session edn-string]
  (try
    (edn/read-string
      {:readers {'lua/table read-lua-table
                 'lua/ref ->LuaRef
                 'lua/structure (fn [{:keys [value refs]}]
                                  (->LuaStructure debug-session value refs))}
       :default ->LuaBlackBox}
      edn-string)
    (catch Exception e
      (throw (ex-info "Error decoding serialized data" {:input edn-string} e)))))

;; A representation of lua table that supports circular references. Acts like a
;; read-only map (no assoc/dissoc) that creates/loads sub-structures on demand.

(defn- maybe-ref->structure [^DebugSession debug-session x refs]
  (if-not (instance? LuaRef x)
    x
    (if (contains? refs x)
      (->LuaStructure debug-session x refs)
      ;; We have a reference that was not serialized, load it:
      (with-session debug-session
        (when (= :suspended (-state debug-session))
          (let [out (.-out debug-session)
                in (.-in debug-session)]
            (send-command! out (format "REF %s --{maxlevel=4}" (:address x)))
            (let [[status rest] (read-status in)]
              (case status
                "200"
                (when-let [[edn-string] (re-match #"^OK\s+" rest)]
                  (let [loaded-refs (::refs (decode-serialized-data debug-session edn-string))]
                    (->LuaStructure debug-session x (into refs loaded-refs))))

                "401"
                (when-let [[size] (re-match #"^Error in Execution\s+(\d+)$" rest)]
                  (let [message (read-data in (Integer/parseInt size))]
                    (log/warn :message "Failed to serialize the ref"
                              :ref (:address x)
                              :error-message message)
                    nil))

                "400"
                (do (log/warn :message "Couldn't load lua table for a ref: not registered on a server"
                              :ref (:address x)
                              :error-message rest)
                    nil)))))))))

(deftype LuaStructure [^DebugSession debug-session value refs]
  ILookup
  (valAt [this k]
    (.valAt this k nil))
  (valAt [_ k not-found]
    (case k
      ::refs refs
      (let [lookup-key (if (instance? LuaStructure k)
                         (.-value ^LuaStructure k)
                         k)
            ret (get-in refs [value lookup-key] not-found)]
        (maybe-ref->structure debug-session ret refs))))

  Seqable
  (seq [_]
    (seq (map (fn [[k v]]
                (MapEntry. (maybe-ref->structure debug-session k refs)
                           (maybe-ref->structure debug-session v refs)))
              (get refs value))))

  Counted
  (count [_]
    (count (get refs value))))

(defn- lua-ref->identity-string [ref structure-refs]
  (if-let [lua-table (get structure-refs ref)]
    (format "<%s>" (:string (meta lua-table)))
    (str "..." (:address ref))))

(defn lua-value->identity-string
  "Returns string representing identity of a lua value. Does not show internal
  structure of lua tables"
  [x]
  (cond
    (instance? LuaStructure x)
    (let [^LuaStructure ls x]
      (lua-ref->identity-string (.-value ls) (.-refs ls)))

    (instance? LuaBlackBox x)
    (format "<%s>" (:string x))

    (= ##Inf x)
    "inf"

    (= ##-Inf x)
    "-inf"

    (and (number? x) (Double/isNaN (double x)))
    "nan"

    :else
    (pr-str x)))

(defn- blanks [n]
  (apply str (repeat n \space)))

(defn lua-value->structure-string
  "Returns string representing structure of a lua value. Tries to show contents
  of lua tables as close to lua data literals as possible. When same lua table
  is referenced multiple times, prints only first occurrence as structure, other
  are displayed as identities"
  [x]
  (cond
    (instance? LuaStructure x)
    (let [^LuaStructure ls x
          structure-refs (.-refs ls)]
      (letfn [(structure-value->str+seen-refs [indent x seen-refs]
                (cond
                  (and (instance? LuaRef x) (not (contains? seen-refs x)))
                  (ref->str+seen-refs indent x seen-refs)

                  (instance? LuaRef x)
                  [(lua-ref->identity-string x structure-refs) seen-refs]

                  :else
                  [(lua-value->identity-string x) seen-refs]))
              (ref->str+seen-refs [indent ref seen-refs]
                (loop [acc-str-entries []
                       acc-seen-refs (conj seen-refs ref)
                       [e & es] (get structure-refs ref)
                       i 1]
                  (if (nil? e)
                    [(str "{ -- " (lua-ref->identity-string ref structure-refs)
                          (->> acc-str-entries
                               (map #(str \newline (blanks (+ indent 2)) %))
                               (string/join \,))
                          \newline (blanks indent) \})
                     acc-seen-refs]
                    (let [[k v] e
                          [k-str k-seen-refs] (structure-value->str+seen-refs (+ indent 2) k acc-seen-refs)
                          [v-str v-seen-refs] (structure-value->str+seen-refs (+ indent 2) v k-seen-refs)]
                      (recur (conj acc-str-entries
                                   (cond
                                     (= k i)
                                     v-str

                                     (and (string? k)
                                          (some? (re-match #"^[a-zA-Z_][a-zA-Z_0-9]*$" k)))
                                     (format "%s = %s" k v-str)

                                     :else
                                     (format "[%s] = %s" k-str v-str)))
                             v-seen-refs
                             es
                             (inc i))))))]
        (first (ref->str+seen-refs 0 (.-value ls) #{}))))

    :else
    (lua-value->identity-string x)))

;;--------------------------------------------------------------------
;; commands

(defn suspend!
  ([^DebugSession debug-session]
   (suspend! debug-session nil))
  ([^DebugSession debug-session on-suspended]
   (with-session debug-session
     (when (= :running (-state debug-session))
       (when on-suspended (-push-suspend-callback! debug-session on-suspended))
       (send-command! (.out debug-session) "SUSPEND")))))

(defn- await-suspend
  [^DebugSession debug-session]
  (let [in (.in debug-session)]
    (let [[status rest] (read-status in)]
      (case status
        "202" (when-let [[file line] (re-match #"^Paused\s+(.+)\s+(\d+)\s*$" rest)]
                {:type :breakpoint
                 :file (sanitize-path file)
                 :line (Integer/parseInt line)})
        "203" (when-let [[file line watch] (re-match #"^Paused\s+(.+)\s+(\d+)\s+(\d+)\s*$" rest)]
                {:type  :watch
                 :watch (Integer/parseInt watch)
                 :file  file
                 :line  (Integer/parseInt line)})
        "204" (when-let [[stream size] (re-match #"^Output (\w+) (\d+)$" rest)]
                (let [n (Integer/parseInt size)]
                  {:type   :output
                   :stream stream
                   :output (read-data in n)}))
        "401" (when-let [[size] (re-match #"^Error in Execution\s+(\d+)$" rest)]
                (let [n (Integer/parseInt size)]
                  {:error (read-data in n)}))))))

(defn- resume!
  [^DebugSession debug-session command {:keys [on-resumed on-suspended]}]
  (with-session debug-session
    (when (= :suspended (-state debug-session))
      (let [in (.in debug-session)
            out (.out debug-session)]
        (try
          (send-command! out command)
          (let [[status rest] (read-status in)]
            (case status
              "200" (do
                      (-set-state! debug-session :running)
                      (when on-suspended (-push-suspend-callback! debug-session on-suspended))
                      (thread (when-some [suspend-event (try
                                                          (await-suspend debug-session)
                                                          (catch IOException t
                                                            (close! debug-session)
                                                            nil))]
                                (when-some [f (with-session debug-session
                                                (assert (= :running (-state debug-session)))
                                                (-set-state! debug-session :suspended)
                                                (-pop-suspend-callback! debug-session))]
                                  (f debug-session suspend-event))))
                      (when on-resumed (on-resumed debug-session)))))
          (catch Throwable t
            (close! debug-session :error)))))))

(defn run!
  [debug-session {:keys [on-resumed on-suspended] :as callbacks}]
  (resume! debug-session "RUN" callbacks))

(defn exit!
  [^DebugSession debug-session]
  (with-session debug-session
    (let [out (.out debug-session)]
      (send-command! out "EXIT")
      (close! debug-session))))

(defn done!
  [^DebugSession debug-session]
  (with-session debug-session
    (let [out (.out debug-session)]
      (send-command! out "DONE")
      (close! debug-session))))

(defn step-into!
  [debug-session {:keys [on-resumed on-suspended] :as callbacks}]
  (resume! debug-session "STEP" callbacks))

(defn step-over!
  [debug-session {:keys [on-resumed on-suspended] :as callbacks}]
  (resume! debug-session "OVER" callbacks))

(defn step-out!
  [debug-session {:keys [on-resumed on-suspended] :as callbacks}]
  (resume! debug-session "OUT" callbacks))

(defn- variables-data
  [variables-map]
  (into {}
        (map (fn [[k v]]
               [k (get v 1)]))
        variables-map))

(defn- stack-data
  [v]
  (into []
        (map (fn [frame]
               (let [location (get frame 1)
                     locals   (get frame 2)
                     upvalues (get frame 3)]
                 {:function      (get location 1 :unknown)
                  :file          (sanitize-path (get location 2))
                  :function-line (get location 3)
                  :line          (get location 4)
                  :type          (get location 5)
                  :locals        (variables-data locals)
                  :upvalues      (variables-data upvalues)})))
        (vals v)))

(defn stack
  [^DebugSession debug-session]
  (with-session debug-session
    (let [out (.out debug-session)
          in (.in debug-session)]
      (send-command! out "STACK --{maxlevel=2}")
      (let [[status rest] (read-status in)]
        (case status
          "200" (when-let [[stack] (re-match #"^OK\s+" rest)]
                  {:stack (stack-data (decode-serialized-data debug-session stack))})
          "401" (when-let [[size] (re-match #"^Error in Execution\s+(\d+)$" rest)]
                  (let [n (Integer/parseInt size)]
                    {:error (read-data in n)})))))))

(defn exec [^DebugSession debug-session code frame]
  (with-session debug-session
    (let [out (.out debug-session)
          in (.in debug-session)]
      (send-command! out (format "EXEC return %s --{stack=%s}" code frame))
      (let [[status rest] (read-status in)]
        (case status
          "200" (when-let [[size] (re-match #"^OK\s+(\d+)$" rest)]
                  (let [n (Integer/parseInt size)]
                    {:result (decode-serialized-data debug-session (read-data in n))}))
          "400" {:error :bad-request}
          "401" (when-let [[size] (re-match #"^Error in Expression\s+(\d+)$" rest)]
                  (let [n (Integer/parseInt size)]
                    {:error (read-data in n)})))))))


;; A note on "SETB file line condition":
;; * In LuaBuilder.java we add '@' in front of the filename to tell Lua to
;; truncate the short_src name to the last 60 characters of the filename.
;; * mobdebug.lua will remove the '@' in the debug hook which means that we
;; must send SETB without the '@' in front of the filename
(defn set-breakpoint!
  "Set a breakpoint in Lua runtime

  Args:
    debug-session    DebugSession instance
    file             lua file path, a string
    line             1-indexed line in the file
    condition        optional condition, a string or nil"
  [^DebugSession debug-session file line condition]
  (with-session debug-session
    (assert (= :suspended (-state debug-session)))
    (let [in (.in debug-session)
          out (.out debug-session)]
      (send-command! out (format "SETB %s %d%s"
                                 (luajit-path-to-chunk-name file)
                                 line
                                 (if condition (str " " condition) "")))
      (let [[status rest :as line] (read-status in)]
        (case status
          "200" :ok
          "400" {:error :bad-request})))))

(defn remove-breakpoint!
  [^DebugSession debug-session file line]
  (with-session debug-session
    (assert (= :suspended (-state debug-session)))
    (let [in (.in debug-session)
          out (.out debug-session)]
      (send-command! out (format "DELB %s %d" (luajit-path-to-chunk-name file) line))
      (let [[status rest :as line] (read-status in)]
        (case status
          "200" :ok
          "400" {:error :bad-request})))))

(defn with-suspended-session
  [^DebugSession debug-session f]
  (with-session debug-session
    (case (-state debug-session)
      :suspended (f debug-session)
      :running   (suspend! debug-session (fn [debug-session suspend-event]
                                           (f debug-session)
                                           (run! debug-session nil))))))
