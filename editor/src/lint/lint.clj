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

(ns ^{:clj-kondo/config '{:linters {:defold/prefer-coll-into-> {:level :off}}}} lint
  (:require [clj-kondo.core :as kondo]
            [cljfmt.report :as report]
            [cljfmt.tool :as cljfmt]
            [fmtlint :as fmtlint]
            [lint.git :as git]))

(set! *warn-on-reflection* true)

(defn- check-kondo [filenames]
  (let [result (kondo/run! {:lint filenames :config {:output {:summary false}}})
        summary (:summary result)]
    (kondo/print! result)
    (if (= 0 (:error summary) (:warning summary)) 0 1)))

(defn- check-cljfmt [filenames]
  (let [result
        (:results (cljfmt/check {:paths filenames
                                 :report report/clojure}))

        counts (:counts result)]
    (doseq [[_ {:keys [diff]}] (:incorrect-files result)]
      (println diff))
    (doseq [[filename {:keys [exception]}] (:errored-files result)]
      (println (format "%s: error: %s" filename (ex-message exception))))
    (if (= 0 (:incorrect counts) (:error counts)) 0 1)))

(defn- run-check [label check filenames]
  (println (str "Running " label))
  (try
    (check filenames)
    (catch Exception exception
      (println (format "%s: error: %s" label (ex-message exception)))
      1)))

(defn check-files
  "Run clj-kondo, cljfmt, and custom layout checks for filenames.

  Run every check and return a nonzero status if any check fails."
  [filenames]
  (let [kondo-status (run-check "clj-kondo" check-kondo filenames)
        cljfmt-status (run-check "cljfmt" check-cljfmt filenames)
        layout-status (run-check "fmtlint" fmtlint/check-files filenames)]
    (if (= 0 kondo-status cljfmt-status layout-status) 0 1)))

(defn -main [& filenames]
  (let [uncommitted (contains? (set filenames) "--uncommitted")

        filenames
        (into []
              (distinct)
              (cond-> (into [] (remove #{"--uncommitted"}) filenames)
                uncommitted (into (git/uncommitted-files "."))))]
    (cond
      (pos? (count filenames))
      (System/exit (int (check-files filenames)))

      uncommitted
      (println "No uncommitted Clojure or EDN files to lint.")

      :else
      (do
        (println "Usage: lein lint [--uncommitted] [<file> ...]")
        (System/exit 1)))))
