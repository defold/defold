(ns editor.debugging.mobdebug
  (:refer-clojure :exclude [eval run!])
  (:require
   [clojure.edn :as edn]
   [editor.error-reporting :as error-reporting]
   [editor.lua :as lua]
   [service.log :as log])
  (:import
   (java.util Stack)
   (java.util.concurrent Executors ExecutorService)
   (java.io IOException PrintWriter BufferedReader InputStreamReader)
   (java.net Socket InetSocketAddress)))

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


;;--------------------------------------------------------------------
;; data decoding

(def ^:private lua-readers
  {'lua/number (fn [v]
                 (if (number? v)
                   v
                   (case v
                     :nan  Double/NaN
                     :-inf Double/NEGATIVE_INFINITY
                     :+inf Double/POSITIVE_INFINITY)))
   'lua/table (fn [{:keys [tostring data]}]
                (with-meta data {:tostring tostring}))
   'lua/tableref (fn [{:keys [tostring data]}]
                    tostring)
   'lua/ref   (fn [{:keys [tostring]}]
                (format "Circular reference to: %s" tostring))})

(defn- decode-serialized-data
  [^String s]
  (try
    (edn/read-string {:readers lua-readers
                      :default (fn [tag val] val)} s)
    (catch Exception e
      (throw (ex-info "Error decoding serialized data"
                      {:input s}
                      e)))))

(defn- remove-filename-prefix
  [^String s]
  (if (.startsWith s "=")
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

(def sanitize-path (comp module->path remove-filename-prefix))


;;------------------------------------------------------------------------------
;; session management

(defprotocol IDebugSession
  (-state [this] "return the session state")
  (-state! [this state] "set the session state")
  (-push-suspend-callback! [this f])
  (-pop-suspend-callback! [this]))

(deftype DebugSession [^Object lock
                       ^:unsynchronized-mutable state
                       ^Socket socket
                       ^BufferedReader in
                       ^PrintWriter out
                       ^Stack suspend-callbacks
                       on-closed]
  IDebugSession
  (-state [this] state)
  (-state! [this new-state] (set! state new-state))
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
    (catch java.net.ConnectException _ nil)))

(defn connect!
  [address port on-connected on-closed]
  (thread
    (loop [retries 0]
      (if-some [socket (try-connect! address port)]
        (let [debug-session (make-debug-session socket on-closed)]
          (on-connected debug-session))
        (if (< retries 10)
          (do (Thread/sleep 200) (recur (inc retries)))
          (throw (ex-info (format "Failed to connect to debugger on %s:%d" address port)
                          {:address address
                           :port port})))))))

(defn close!
  ([debug-session]
   (close! debug-session :closed))
  ([^DebugSession debug-session end-state]
   (when-some [^Socket socket (.socket debug-session)]
     (try
       (.close socket)
       (catch IOException _))) 
   (-state! debug-session end-state)
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
  (with-session debug-session
    (-state debug-session)))


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
                      (-state! debug-session :running)
                      (when on-suspended (-push-suspend-callback! debug-session on-suspended))
                      (thread (when-some [suspend-event (try
                                                          (await-suspend debug-session)
                                                          (catch IOException t
                                                            (close! debug-session)
                                                            nil))]
                                (when-some [f (with-session debug-session
                                                (assert (= :running (-state debug-session)))
                                                (-state! debug-session :suspended)
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
      (send-command! out "STACK")
      (let [[status rest] (read-status in)]
        (case status
          "200" (when-let [[stack] (re-match #"^OK\s+(.*)$" rest)]
                  {:stack (stack-data (decode-serialized-data stack))})
          "401" (when-let [[size] (re-match #"^Error in Execution\s+(\d+)$" rest)]
                  (let [n (Integer/parseInt size)]
                    {:error (read-data in n)})))))))

(defn exec
  ([debug-session code]
   (exec debug-session code 0))
  ([^DebugSession debug-session code frame]
   (with-session debug-session
     (let [out (.out debug-session)
           in (.in debug-session)]
       (send-command! out (format "EXEC %s --{stack=%s}" code frame))
       (let [[status rest] (read-status in)]
         (case status
           "200" (when-let [[size] (re-match #"^OK\s+(\d+)$" rest)]
                   (let [n (Integer/parseInt size)]
                     {:result (decode-serialized-data (read-data in n))}))
           "400" {:error :bad-request}
           "401" (when-let [[size] (re-match #"^Error in Expression\s+(\d+)$" rest)]
                   (let [n (Integer/parseInt size)]
                     {:error (read-data in n)}))))))))

(defn eval
  ([debug-session code]
   (eval debug-session code 0))
  ([debug-session code frame]
   (exec debug-session (str "return " code) frame)))

(defn set-breakpoint!
  [^DebugSession debug-session file line]
  (with-session debug-session
    (assert (= :suspended (-state debug-session)))
    (let [in (.in debug-session)
          out (.out debug-session)]
      (send-command! out (format "SETB =%s %d" file line))
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
      (send-command! out (format "DELB =%s %d" file line))
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
