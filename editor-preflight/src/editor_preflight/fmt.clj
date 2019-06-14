(ns editor-preflight.fmt
  (:require [cljfmt.core :as cljfmt]
            [cljfmt.diff :as diff]
            [clojure.java.io :as io]
            [clojure.string :as str])
  (:import difflib.DiffUtils
           [java.io File PrintWriter StringWriter]))

(defn- reformat-string [options s]
  ((cljfmt/wrap-normalize-newlines #(cljfmt/reformat-string % options)) s))

(defn- unified-diff
  ([filename original revised]
   (unified-diff filename original revised 3))
  ([filename original revised context]
   (str/join \newline (DiffUtils/generateUnifiedDiff
                        (->> filename (str "a"))
                        (->> filename (str "b"))
                        (str/split-lines original)
                        (DiffUtils/diff (str/split-lines original) (str/split-lines revised))
                        context))))

(defn- format-diff
  (^String [options ^File file]
   (let [original (slurp (io/file file))]
     (format-diff options file original (reformat-string options original))))
  (^String [options ^File file original revised]
   (unified-diff (.getCanonicalPath file) original revised)))

(def ^:private ^:const zero-counts {:okay 0, :incorrect 0, :error 0})

(defn- check-one [options file]
  (let [original (slurp file)
        status   {:counts zero-counts :file file}]
    (try
      (let [revised (reformat-string options original)]
        (if (not= original revised)
          (-> status
              (assoc-in [:counts :incorrect] 1)
              (assoc :diff (format-diff options file original revised)))
          (assoc-in status [:counts :okay] 1)))
      (catch Exception ex
        (-> status
            (assoc-in [:counts :error] 1)
            (assoc :exception ex))))))

(defn- print-stack-trace
  ^String [^Exception ex]
  (let [sw (StringWriter.)
        pw (PrintWriter. sw)]
    (.printStackTrace ex pw)
    (.toString sw)))

(defn- print-file-status
  ^String [options status]
  (let [path (.getCanonicalPath ^File (:file status))]
    (when-let [ex (:exception status)]
      (str "Failed to format file: " path \newline (print-stack-trace ex)))
    (when-let [diff (:diff status)]
      (str path " has incorrect formatting" \newline diff))))

(defn- print-final-result [{:keys [report counts]}]
  (let [error (:error counts 0)
        incorrect (:incorrect counts 0)]
    (str
      report
      \newline
      \newline
      (when-not (zero? error)
        (str error " file(s) could not be parsed for formatting"))
      (when-not (zero? incorrect)
        (str incorrect " file(s) formatted incorrectly"))
      (when (and (zero? incorrect) (zero? error))
        "All source files formatted correctly"))))

(defn- merge-results
  ([] {:report ""
       :counts zero-counts})
  ([a] a)
  ([a b] {:report (str/join \newline [(:report a) (:report b)])
          :counts (merge-with + (:counts a) (:counts b))}))

(def ^:private ^:const default-options
  {:project-root "."
   :file-pattern #"\.clj[csx]?$"
   :indentation? true
   :insert-missing-whitespace?      true
   :remove-surrounding-whitespace?  true
   :remove-trailing-whitespace?     true
   :remove-consecutive-blank-lines? true
   :indents cljfmt/default-indents
   :alias-map {}})

(defn- merge-default-options [options]
  (-> (merge default-options options)
      (assoc :indents (merge (:indents default-options)
                             (:indents options {})))))

(def ^:private ^:const options
  (merge-default-options
    {:alias-map {"g" "dynamo.graph"
                 "handler" "editor.handler"
                 "profiler" "util.profiler"
                 "ui" "editor.ui"}
     :indents '{assoc [[:inner 0]]
                cond-> [[:block 0]]
                doto [[:inner 0]]

                dynamo.graph/defnk [[:inner 0]]
                dynamo.graph/defnode [[:inner 0]]
                dynamo.graph/fnk [[:inner 0]]
                dynamo.graph/make-nodes [[:inner 0]]
                dynamo.graph/precluding-errors [[:inner 0]]
                dynamo.graph/with-auto-evaluation-context [[:inner 0]]
                dynamo.graph/with-auto-or-fake-evaluation-context [[:inner 0]]
                editor.handler/defhandler [[:inner 0]]
                editor.ui/event-handler [[:inner 0]]
                editor.ui/with-controls [[:inner 0]]
                editor.ui/with-progress [[:inner 0]]
                util.profiler/profile [[:inner 0]]}}))

(defn check-files
  [files]
  (print-final-result
    (transduce
      (comp (map (fn [status]
                   {:report (print-file-status options status)
                    :counts (:counts status)})))
      merge-results
      (pmap (partial check-one options) files))))
