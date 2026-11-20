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
            [clojure.java.io :as io]
            [fmtlint :as fmtlint]
            [lint.git :as git])
  (:import [java.io File StringReader]
           [java.nio.file Files]))

(set! *warn-on-reflection* true)

(defn- run-kondo [filenames]
  (kondo/run! {:lint filenames :config {:output {:summary false}}}))

(defn- run-kondo-with-stdin [filenames stdin]
  ;; The pod uses its own stdin for protocol messages.
  (let [file (File/createTempFile "lint-stdin-" ".clj")
        path (.getCanonicalPath file)]
    (try
      (spit file stdin)
      (update (run-kondo (replace {"-" path} (vec filenames)))
              :findings
              (fn normalize-stdin-findings [findings]
                (into []
                      (keep (fn [finding]
                              (if-not (= path (:filename finding))
                                finding
                                ;; Piped source has no filename to validate.
                                (when-not (= :namespace-name-mismatch (:type finding))
                                  (assoc finding :filename "STDIN")))))
                      findings)))
      (finally
        (Files/deleteIfExists (.toPath file))))))

(defn- check-kondo [filenames stdin]
  (let [result
        (if stdin
          (run-kondo-with-stdin filenames stdin)
          (run-kondo filenames))]
    (kondo/print! result)
    (reduce (fn [_ {:keys [level]}]
              (if (#{:error :warning} level) (reduced 1) 0))
            0
            (:findings result))))

(defn- collect-cljfmt-result [event context data]
  ;; The library reporter expects a File even for stdin.
  (let [data
        (cond-> data
          (and (= :check/file event) (not (instance? File (:file data))))
          (assoc :file (io/file "STDIN")))]
    (report/clojure event context data)))

(defn- check-cljfmt [filenames]
  (let [result
        (:results (cljfmt/check {:paths filenames
                                 :report collect-cljfmt-result}))

        counts (:counts result)]
    (doseq [[_ {:keys [diff]}] (:incorrect-files result)]
      (println diff))
    (doseq [[filename {:keys [exception]}] (:errored-files result)]
      (println (format "%s: error: %s" filename (ex-message exception))))
    (if (= 0 (:incorrect counts) (:error counts)) 0 1)))

(defn- run-check [label check]
  (println (str "Running " label))
  (try
    (check)
    (catch Exception exception
      (println (format "%s: error: %s" label (ex-message exception)))
      1)))

(defn check-files
  "Run clj-kondo, cljfmt, and custom layout checks for filenames.

  Use - to read Clojure source from stdin.
  Run every check and return a nonzero status if any check fails."
  [filenames]
  (let [stdin (when (contains? (set filenames) "-") (slurp *in*))
        kondo-status (run-check "clj-kondo" #(check-kondo filenames stdin))

        cljfmt-status
        (run-check "cljfmt"
                   #(binding [*in* (if stdin (StringReader. stdin) *in*)]
                      (check-cljfmt filenames)))

        layout-status
        (run-check "fmtlint" #(fmtlint/check-files filenames stdin))]
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
        (println "Usage: lein lint [--uncommitted] [<file> | -] ...")
        (System/exit 1)))))
