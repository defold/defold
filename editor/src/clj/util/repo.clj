(ns util.repo
  (:require [clojure.java.shell :as shell]
            [clojure.string :as string])
  (:import (java.io IOException)))

(defn- try-shell-command! [command & args]
  (try
    (let [{:keys [out err]} (apply shell/sh command args)]
      (when (empty? err)
        (string/trim-newline out)))
    (catch IOException _
      ;; The specified command does not exist.
      nil)))

(def detect-engine-sha1
  (memoize
    (fn detect-engine-sha1 []
      (some->> (try-shell-command! "git" "log" "--format=%H" "--max-count=5" "../VERSION")
               string/split-lines
               not-empty
               (keep #(try-shell-command! "git" "show" (str % ":VERSION")))
               (map string/trim-newline)
               (some (partial try-shell-command! "git" "rev-list" "-n" "1"))))))

(def detect-editor-sha1
  (memoize
    (fn detect-editor-sha1 []
      (try-shell-command! "git" "rev-list" "-n" "1" "HEAD"))))
