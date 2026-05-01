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

(ns defold-robot.robot
  (:require [clojure.edn :as edn]
            [clojure.java.io :as io]
            [clojure.tools.cli :as tools.cli]
            [clojure.string :as string])
  (:import [java.io BufferedReader File FileFilter StringWriter]
           [javafx.application Platform]
           [javafx.embed.swing SwingFXUtils]
           [javafx.scene.input KeyCode]
           [javafx.scene.robot Robot]
           [javafx.stage Screen]
           [javax.imageio ImageIO]
           [org.apache.commons.io IOUtils]
           [org.apache.commons.io.filefilter WildcardFileFilter])
  (:gen-class))

(set! *warn-on-reflection* true)

(def cli-options
  [["-s" "--script PATH" "Script file"]
   ["-o" "--output PATH" "Output directory for log" :default "."]
   ["-h" "--help"]])

(defmulti exec-step (fn [_ step] (first step)))

(defmethod exec-step :wait [{:keys [log-fn]} [_ delay]]
  (let [delay (int delay)]
    (log-fn (format "<p>Wait %s</p>" delay))
    (Thread/sleep (long delay))
    true))

(def ^:private ^:dynamic *readers* nil)

(defn- open-log ^BufferedReader [log-descs log-id]
  (let [desc (get log-descs log-id)
        ^String pattern (get desc :pattern)
        dir (io/as-file (get desc :dir))
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
        (get log-id)))))

(defmethod exec-step :await-log [{:keys [log-fn log-descs]} [_ log-id timeout pattern]]
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
                           (Thread/sleep (long delay))
                           (recur (or reader (open-log log-descs log-id))))))))]
      (log-fn "</code>")
      (log-fn "</div>")
      (if (not result)
        (log-fn "<p class=\"error\">ERROR Await-log timed out</p>")
        (log-fn (format "<p>Await-log %s %d '%s' ended in %d ms</p>"
                        log-id
                        timeout
                        pattern
                        (- (System/currentTimeMillis) start-time))))
      result)))

(defmethod exec-step :screen-capture [{:keys [^Robot robot ^File out-dir log-fn]} [_ filename]]
  (let [bounds (.getBounds (Screen/getPrimary))
        image (.getScreenCapture robot nil bounds)
        dest-name (format "%s.png" filename)
        dest (File. out-dir dest-name)]
    (log-fn (format "<p>Screen-capture %s</p>" dest-name))
    (log-fn (format "<img src=\"%s\"></img>" dest-name))
    (ImageIO/write (SwingFXUtils/fromFXImage image nil) "png" dest))
  true)

(defn- kw->key-code [kw]
  (let [parts (string/split (name kw) #"-")]
    (->> parts
         (map string/upper-case)
         (string/join "_")
         (Enum/valueOf KeyCode))))

(def char->key-code
  (into {}
        (map (juxt #(.charAt (.getChar ^KeyCode %) 0)
                   identity))
        (KeyCode/values)))

(defn- press [^Robot robot keys]
  (doseq [kw keys]
    (Thread/sleep 40)
    (.keyPress robot (kw->key-code kw)))
  (doseq [kw keys]
    (Thread/sleep 40)
    (.keyRelease robot (kw->key-code kw))))

(defmethod exec-step :press [{:keys [robot log-fn]} [_ & keys]]
  (log-fn (format "<p>Press %s</p>" (string/join "+" (map name keys))))
  (press robot keys)
  true)

(defmethod exec-step :type [{:keys [^Robot robot log-fn]} [_ s]]
  (log-fn (format "<p>Type %s</p>" s))
  (doseq [c (string/upper-case s)]
    (Thread/sleep 40)
    (.keyType robot (char->key-code c)))
  true)

(defmethod exec-step :switch-focus [{:keys [robot log-fn platform]} _]
  (log-fn (format "<p>Switch-focus</p>"))
  (if (= :mac platform)
    (press robot [:command :tab])
    (press robot [:alt :tab]))
  true)

(defmethod exec-step :open-asset [{:keys [robot log-fn action-key]} _]
  (log-fn (format "<p>Open-asset</p>"))
  (press robot [action-key :p])
  true)

(defmethod exec-step :build [{:keys [robot log-fn action-key]} _]
  (log-fn (format "<p>Build</p>"))
  (press robot [action-key :b])
  true)

(defmethod exec-step :hot-reload [{:keys [robot log-fn action-key]} _]
  (log-fn (format "<p>Hot-reload</p>"))
  (press robot [action-key :r])
  true)

(defmethod exec-step :undo [{:keys [robot log-fn action-key]} _]
  (log-fn (format "<p>Undo</p>"))
  (press robot [action-key :z])
  true)

(defmethod exec-step :quit [{:keys [robot log-fn action-key]} _]
  (log-fn (format "<p>Quit</p>"))
  (press robot [action-key :q])
  true)

(defmethod exec-step :default [{:keys [log-fn]} step]
  (log-fn (format "<p>Unknown %s</p>" step))
  false)

(defn- store! [^File dir ^String resource args]
  (let [src-content (slurp (io/resource resource))
        out-content (reduce (fn [out [key value]] (string/replace out (format "$%s" key) value)) src-content args)]
    (spit (File. dir resource) out-content)))

(defmacro run-later [& body]
  `(let [*result# (promise)]
     (Platform/runLater
       (fn []
         (deliver *result#
                  (try
                    [nil (do ~@body)]
                    (catch Exception e#
                      [e# nil])))))
     (delay
       (let [[err# val#] @*result#]
         (if err#
           (throw err#)
           val#)))))

(defn run [script output]
  @(run-later
     (binding [*readers* (atom {})]
       (try
         (let [out-dir (doto (io/as-file output)
                         (.mkdirs))
               log-descs (get script :logs)
               robot (Robot.)
               log-writer (StringWriter.)
               log-fn (fn [s]
                        (println s)
                        (.write log-writer (format "%s\n" s)))
               platform (let [name (System/getProperty "os.name")]
                          (cond
                            (.contains name "Mac") :mac
                            (.contains name "Linux") :linux
                            :else :windows))
               ctx {:robot robot
                    :log-fn log-fn
                    :log-descs log-descs
                    :out-dir out-dir
                    :platform platform
                    :action-key (if (= :mac platform) :command :control)}
               result (loop [steps (get script :steps)]
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
             (IOUtils/closeQuietly reader)))))))

(defn- parse-script [script]
  (edn/read-string (slurp script)))

(defn -main [& args]
  (let [opts (tools.cli/parse-opts args cli-options)]
    (if (get-in opts [:options :help])
      (println (:summary opts))
      (let [s (parse-script (get-in opts [:options :script]))]
        (Platform/startup (fn []))
        (if (run s (get-in opts [:options :output]))
          (Platform/exit)
          (System/exit 1))))))
