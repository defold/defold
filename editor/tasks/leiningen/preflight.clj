(ns leiningen.preflight
  (:require [clojure.java.io :as io]
            [leiningen.core.eval :as lein-eval])
  (:import [java.io InputStreamReader]))

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
