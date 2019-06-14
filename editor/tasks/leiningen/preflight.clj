(ns leiningen.preflight
  (:require [cljfmt.core :as cljfmt]
            [clojure.java.io :as io]
            [clojure.java.shell :as shell]
            [clojure.string :as string]
            [kibit.driver :as kibit]
            [leiningen.core.main :as main])
  (:import [java.io InputStreamReader BufferedReader]))

(defn- print-stream-in-background
  [stream]
  (future
    (let [isr (InputStreamReader. stream)
          chars (char-array 4096)
          sb (StringBuilder.)
          flush-sb (fn []
                     (print (.toString sb))
                     (flush)
                     (.setLength sb 0))]
      (loop []
        (if (.ready isr)
          (let [c (.read isr)]
            (if (> c -1)
              (do (.append sb (char c))
                  (recur))
              (flush-sb)))
          (do (flush-sb)
              (Thread/sleep 10)
              (recur)))))))

(defn- run-preflight-check
  []
  (let [proc (.exec (Runtime/getRuntime)
                    (into-array ["java" "-jar" "editor-preflight-1.0.0.jar"])
                    nil
                    (io/file "."))]
    (print-stream-in-background (.getInputStream proc))
    (print-stream-in-background (.getErrorStream proc))
    (.waitFor proc)))

(defn preflight
  [project & rest]
  (run-preflight-check)
  (main/resolve-and-apply project ["test"]))
