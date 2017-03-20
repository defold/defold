(ns defold-robot.robot
  (:require [clojure.tools.cli :refer [parse-opts]]
    [clojure.java.io :as io]
    [clojure.data.json :as json]
    [clojure.string :as string])
  (:import [java.awt Dimension Rectangle Robot Toolkit]
    [java.awt.event KeyEvent]
    [java.io File]
    [javax.imageio ImageIO])
  (:gen-class))

(def cli-options
  ;; An option with a required argument
  [["-s" "--script PATH" "Script file"]
   ;; A non-idempotent option
   ["-v" nil "Verbosity level"
    :id :verbosity
    :default 0
    :assoc-fn (fn [m k _] (update-in m [k] inc))]
   ;; A boolean option defaulting to nil
   ["-h" "--help"]])

(defmulti exec-step (fn [_ step] (string/lower-case (first step))))

(defmethod exec-step "wait" [{:keys [robot log-fn]} [_ delay]]
  (let [delay (int delay)]
    (log-fn (format "Wait %s" delay))
    (.delay robot (int delay))))

(defmethod exec-step "screen-capture" [{:keys [robot log-fn]} [_ filename]]
  (let [dims (.getScreenSize (Toolkit/getDefaultToolkit))
        img (.createScreenCapture robot (Rectangle. dims))
        dest (File. (format "%s.png" filename))]
    (log-fn (format "Screen-capture %s" dest))
    (ImageIO/write img "png" dest)))

(defn- key->key-code [key]
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

(defn- press [robot log-fn keys]
  (when keys
    (let [key (first keys)
          code (key->key-code key)]
      (try
        (if code
          (.keyPress robot code)
          (log-fn (format "Skipped unrecognized key '%s'" key)))
        (press robot log-fn (next keys))
        (finally
          (when code
            (.keyRelease robot code)))))))

(defmethod exec-step "press" [{:keys [robot log-fn]} [_ keys]]
  (let [keys (string/split keys #"\+")]
    (log-fn (format "Press %s" (string/join "+" keys)))
    (press robot log-fn keys)))

(defmethod exec-step "type" [{:keys [robot log-fn]} [_ s]]
  (log-fn (format "Type %s" s))
  (doseq [c s]
    (press robot log-fn [(str c)])))

(defmethod exec-step "switch-focus" [{:keys [robot log-fn]} [_ times]]
  (dotimes [i times]
    (log-fn (format "Switch-focus"))
    ;; TODO - platform specific
    (press robot log-fn ["Shortcut" "Tab"])))

(defmethod exec-step :default [{:keys [log-fn]} step]
  (log-fn (format "Unknown %s" step)))

(defn- run [script]
  (let [steps (get script "steps")
        robot (doto (Robot.)
                (.setAutoDelay 40)
                (.setAutoWaitForIdle true))
        log-fn println
        ctx {:robot robot
             :log-fn log-fn}]
    (doseq [s steps]
      (exec-step ctx s))))

(defn- parse-script [path]
  (with-open [r (io/reader path)]
    (json/read r)))

(defn -main [& args]
  (let [opts (parse-opts args cli-options)]
    (if (get-in opts [:options :help])
      (println (:summary opts))
      (let [s (parse-script (get-in opts [:options :script]))]
        (run s)))))
