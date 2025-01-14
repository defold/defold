;; Copyright 2020-2025 The Defold Foundation
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

(ns editor.bundle
  (:require [clojure.java.shell :as shell]))

(set! *warn-on-reflection* true)

(defn- sh [& args]
  (try
    (let [result (apply shell/sh args)]
      (if (zero? (:exit result))
        result
        (throw (ex-info (format "Shell call failed:\n%s" args) result))))
    (catch Exception e
      (throw (ex-info (format "Shell call failed:\n%s" args) {} e)))))

;; For matching lines like:
;;   1) 0123456789ABCDEF0123456789ABCDEF01234567 "iPhone Developer: erik.angelin@king.com (0123456789)"
;;      ^-first group--------------------------^  ^-second group-------------------------------------^
(def ^:private identity-regexp #"\s+\d+\)\s+([0-9A-Z]+)\s+\"(.*?)\"")

(defn find-identities []
  (->> (.split ^String (:out (sh "security" "find-identity" "-v" "-p" "codesigning")) "\n")
       (map #(first (re-seq identity-regexp %)))
       (remove nil?)
       (map (fn [[_ id name]] [id name]))))
