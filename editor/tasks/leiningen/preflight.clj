(ns leiningen.preflight
  (:require [clojure.java.io :as io]
            [leiningen.core.eval :as lein-eval])
  (:import [java.io InputStreamReader]))

(defn- print-stream-in-background
  [stream]
  (future
    (let [isr (InputStreamReader. stream)
          sb (StringBuilder.)
          flush-sb (fn []
                     (print (str sb))
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

(defn preflight
  [project & _rest]
  (lein-eval/eval-in-project
    project
    '(do
       (load-file "tasks/leiningen/preflight/fmt.clj")
       (load-file "tasks/leiningen/preflight/kibit.clj")
       (load-file "tasks/leiningen/preflight/test.clj")
       (load-file "tasks/leiningen/preflight/core.clj")
       (in-ns 'preflight.core)
       (main)
       (shutdown-agents)
       (System/exit 0))))
