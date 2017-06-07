(ns defold-robot.robot
  (:require [clojure.tools.cli :refer [parse-opts]]
    [clojure.java.io :as io]
    [clojure.data.json :as json]
    [clojure.string :as string])
  (:import [java.awt Dimension Rectangle Robot Toolkit]
    [java.awt.event KeyEvent]
    [java.io BufferedReader File FileFilter InputStream Reader StringWriter]
    [java.nio.file FileSystems PathMatcher]
    [javax.imageio ImageIO]
    [org.apache.commons.io IOUtils]
    [org.apache.commons.io.filefilter WildcardFileFilter])
  (:gen-class))

(set! *warn-on-reflection* true)

(def cli-options
  ;; An option with a required argument
  [["-s" "--script PATH" "Script file"]
   ["-o" "--output PATH" "Output directory for log" :default "."]
   ;; A non-idempotent option
   ["-v" nil "Verbosity level"
    :id :verbosity
    :default 0
    :assoc-fn (fn [m k _] (update-in m [k] inc))]
   ;; A boolean option defaulting to nil
   ["-h" "--help"]])

(defmulti exec-step (fn [_ step] (string/lower-case (first step))))

(defmethod exec-step "wait" [{:keys [^Robot robot log-fn]} [_ delay]]
  (let [delay (int delay)]
    (log-fn (format "<p>Wait %s</p>" delay))
    (.delay robot (int delay))
    true))

(def ^:private ^:dynamic *readers* nil)

(defn- open-log ^BufferedReader [log-descs log-id]
  (when-let [desc (get log-descs log-id)]
    (let [^String pattern (get desc "pattern")
          dir (io/as-file (get desc "dir"))
          ^FileFilter filter (WildcardFileFilter. pattern)
          ^File file (->> (.listFiles dir filter)
                       vec
                       (sort-by (fn [^File f] (.lastModified f)))
                       last)]
      (when file
        (->
          (swap! *readers* (fn [readers]
                             (if (contains? readers log-id)
                               readers
                               (let [r (doto (io/reader file)
                                         (.skip (.length file)))]
                                 (assoc readers log-id r)))))
          (get log-id))))))

(defmethod exec-step "await-log" [{:keys [^Robot robot log-fn log-descs]} [_ log-id timeout pattern]]
  (let [start-time (System/currentTimeMillis)
        end-time (+ (int timeout) start-time)
        delay 50
        re (re-pattern pattern)]
    (log-fn (format "<p>Await-log %s %d '%s'</p>" log-id timeout pattern))
    (log-fn "<div class=\"log\">")
    (log-fn "<code>")
    (let [result (loop [reader (open-log log-descs log-id)]
                   (if (< end-time (System/currentTimeMillis))
                     false
                     (let [result (when reader
                                    (loop [line (.readLine reader)]
                                      (if line
                                        (do
                                          (log-fn (format "%s<br>" line))
                                          (if (re-find re line)
                                            true
                                            (recur (.readLine reader))))
                                        false)))]
                       (if result
                         result
                         (do
                           (.delay robot delay)
                           (recur (or reader (open-log log-descs log-id))))))))]
      (log-fn "</code>")
      (log-fn "</div>")
      (if (not result)
        (log-fn "<p class=\"error\">ERROR Await-log timed out</p>")
        (log-fn (format "<p>Await-log %s %d '%s' ended in %d ms</p>" log-id timeout pattern (- (System/currentTimeMillis) start-time))))
      result)))

(defmethod exec-step "screen-capture" [{:keys [^Robot robot ^File out-dir log-fn]} [_ filename]]
  (let [dims (.getScreenSize (Toolkit/getDefaultToolkit))
        img (.createScreenCapture robot (Rectangle. dims))
        dest-name (format "%s.png" filename)
        dest (File. out-dir dest-name)]
    (log-fn (format "<p>Screen-capture %s</p>" dest-name))
    (log-fn (format "<img src=\"%s\"></img>" dest-name))
    (ImageIO/write img "png" dest))
  true)

(defn- key->key-code [^String key]
  (if (= 1 (count key))
    (KeyEvent/getExtendedKeyCodeForChar (.codePointAt key 0))
    (case (string/lower-case key)
      "shift" KeyEvent/VK_SHIFT
      "shortcut" KeyEvent/VK_META
      "alt" KeyEvent/VK_ALT
      "tab" KeyEvent/VK_TAB
      "enter" KeyEvent/VK_ENTER
      "up" KeyEvent/VK_UP
      "down" KeyEvent/VK_DOWN
      "left" KeyEvent/VK_LEFT
      "right" KeyEvent/VK_RIGHT
      nil)))

(defn- press [^Robot robot log-fn keys]
  (when keys
    (let [key (first keys)
          code (key->key-code key)]
      (try
        (if code
          (.keyPress robot code)
          (log-fn (format "<p>Skipped unrecognized key '%s'</p>" key)))
        (press robot log-fn (next keys))
        (finally
          (when code
            (.keyRelease robot code)))))))

(defmethod exec-step "press" [{:keys [robot log-fn]} [_ keys]]
  (let [keys (string/split keys #"\+")]
    (log-fn (format "<p>Press %s</p>" (string/join "+" keys)))
    (press robot log-fn keys))
  true)

(defmethod exec-step "type" [{:keys [robot log-fn]} [_ s]]
  (log-fn (format "<p>Type %s</p>" s))
  (doseq [c s]
    (press robot log-fn [(str c)]))
  true)

(defmethod exec-step "switch-focus" [{:keys [robot log-fn]} [_ times]]
  (dotimes [i times]
    (log-fn (format "<p>Switch-focus</p>"))
    ;; TODO - platform specific
    (press robot log-fn ["Shortcut" "Tab"]))
  true)

(defmethod exec-step :default [{:keys [log-fn]} step]
  (log-fn (format "<p>Unknown %s</p>" step))
  false)

(defn- store! [^File dir ^String resource args]
  (let [src-content (slurp (io/resource resource))
        out-content (reduce (fn [out [key value]] (string/replace out (format "$%s" key) value)) src-content args)]
    (spit (File. dir resource) out-content)))

(defn run [script output]
  (binding [*readers* (atom {})]
    (try
      (let [out-dir (doto (io/as-file output)
                      (.mkdirs))
            log-descs (get script "logs")
            robot (doto (Robot.)
                    (.setAutoDelay 40)
                    (.setAutoWaitForIdle true))
            log-writer (StringWriter.)
            log-fn (fn [s]
                     (println s)
                     (.write log-writer (format "%s\n" s)))
            ctx {:robot robot
                 :log-fn log-fn
                 :log-descs log-descs
                 :out-dir out-dir}
           result (loop [steps (get script "steps")]
                    (if-let [s (first steps)]
                      (if (exec-step ctx s)
                        (recur (rest steps))
                        false)
                      true))]
        (store! out-dir "index.html" {"log" (.toString (.getBuffer log-writer))})
        (store! out-dir "robot.css" {})
        result)
      (finally
        (doseq [[_ ^BufferedReader reader] @*readers*]
          (IOUtils/closeQuietly reader))))))

(defn- parse-script [script]
  (with-open [r (io/reader script)]
    (json/read r)))

(defn -main [& args]
  (let [opts (parse-opts args cli-options)]
    (if (get-in opts [:options :help])
      (println (:summary opts))
      (let [s (parse-script (get-in opts [:options :script]))]
        (when (not (run s (get-in opts [:options :output])))
          (System/exit 1))))))
