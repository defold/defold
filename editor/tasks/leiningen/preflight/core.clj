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

(ns preflight.core
  (:require [clojure.java.io :as io]
            [preflight.fmt :as fmt]
            [preflight.kibit :as kibit]
            [preflight.test :as test])
  (:import [java.io File]))

(defn- find-files-to-check
  []
  (filter #(.isFile ^File %) (file-seq (io/file "src/clj/"))))

(defn- dots-until-done
  [fut]
  (while (not (future-done? fut))
    (print ".")
    (flush)
    (Thread/sleep 500))
  (newline)
  @fut)

(defn- run-linter
  [[name lintfn {:keys [:print-average]}] files nfiles]
  (print (str "Running " name))
  (let [start (System/nanoTime)
        result (dots-until-done (future (lintfn files)))
        time (/ (- (System/nanoTime) start) 1000000000.0)
        avg (/ time nfiles)]
    (when print-average
      (println (format " %.2f seconds. Average %.2f seconds per file." time avg)))
    [name result]))

#_(defn- kondo
    [files]
    (with-out-str
      (-> (kondo/run! {:lint (map #(.getCanonicalPath ^File %) files)}) kondo/print!)))

(def ^:private ^:const linters
  ;; Entry signature: [name, (fn [files]), {:print-average boolean}]
  [["clj-fmt" fmt/check-files {:print-average true}]
   ["kibit" kibit/check {:print-average true}]
   ;;["clj-kondo" kondo {:print-average true}]
   ["tests" test/run {:print-average false}]])

(defn- run
  [files]
  (print "Collecting source files.")
  (let [files (or (seq files) (dots-until-done (future (find-files-to-check))))
        nfiles (count files)]
    (println (str " Found " nfiles " files.\n"))
    (into [] (map #(run-linter % files nfiles)) linters)))

(defn main
  []
  (println "\n===== Defold Editor Preflight Check =====\n")
  (let [results (run nil)
        one? #(= 1 %)]
    (println "\n===== Preflight check done =====\n")
    (with-open [w (io/writer "preflight-report.txt")]
      (doseq [[name {:keys [report counts]}] results]
        (let [{:keys [incorrect error]} counts]
          (println (str name ":"))
          (println (str " " error (if (one? error) " error." " errors.")))
          (println (str " " incorrect (if (one? incorrect) " issue." " issues.")))
          (.write w (str "----- " name " report -----\n" report "\n\n")))))
    (println "\nResults saved to preflight-report.txt")))
