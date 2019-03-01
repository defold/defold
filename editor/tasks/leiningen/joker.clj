(ns leiningen.joker
  (:require [clojure.java.io :as io]
            [clojure.java.shell :as shell])
  (:import [java.io File]))

(defn- is-clj?
  [^File f]
  (.endsWith (.getName f) ".clj"))

(defn joker
  "Lint all the clj source files using Joker."
  [project & ignore]
  (doseq [f (filter is-clj? (mapcat file-seq (map io/file (concat (:source-paths project)
                                                                  (:test-paths project)))))]
    (let [{:keys [exit out err]} (shell/sh "joker" "--lint" (.getCanonicalPath ^File f))]
      (when (not= 0 exit)
        (when (not-empty out)
          (println out))
        (when (not-empty err)
          (binding [*out* *err*]
            (println err)))))))
