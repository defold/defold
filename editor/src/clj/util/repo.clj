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
