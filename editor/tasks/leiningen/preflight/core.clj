(ns preflight.core
  (:require [clojure.java.io :as io]
            [preflight.fmt :as fmt]
            [preflight.kibit :as kibit]
            [preflight.test :as test])
  (:import [java.io File]))

(defn- find-files-to-check
  []
  (filter #(.isFile ^File %) (file-seq (io/file "src/clj/"))))

(defn- make-doter
  []
  (let [state (atom 0)]
    (fn []
      (print ".")
      (flush)
      (swap! state #(rem (inc %) 4)))))

(defn- dots-until-done
  [fut]
  (let [doter (make-doter)]
    (while (not (future-done? fut))
      (doter)
      (Thread/sleep 500))
    (newline))
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
  [["clj-fmt" fmt/check-files {:print-average true}]
   ["kibit" kibit/check {:print-average true}]
   ;;["clj-kondo" kondo {:print-average true}]
   ["tests" test/run {:print-average false}]])

(defn- run
  [files]
  (print "Collecting source files.")
  (let [files (or (seq files) (dots-until-done (future (take 1 (find-files-to-check)))))
        nfiles (count files)]
    (println)
    (println (str " Found " nfiles " files."))
    (into [] (map #(run-linter % files nfiles)) linters)))

(defn- strings->files
  [strings]
  (into
    []
    (comp
      (map io/file)
      (filter #(.exists ^File %)))
    strings))

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
    (println "Results saved to preflight-report.txt")))
