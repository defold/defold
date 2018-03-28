(ns editor.code.syntax
  (:require [clojure.string :as string]
            [editor.code.util :as util :refer [pair]])
  (:import (java.util.regex MatchResult Pattern)))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(defrecord AnalysisContext [parent-pattern end-re])
(defrecord Match [type contexts ^MatchResult match-result pattern])

(defn- replace-back-references
  ^Pattern [^Pattern re ^MatchResult input]
  (re-pattern (reduce (fn [re-string group]
                        (if-some [capture (.group input group)]
                          (string/replace re-string
                                          (re-pattern (str "\\\\" group "(?!\\d)"))
                                          (string/re-quote-replacement (Pattern/quote capture)))
                          re-string))
                      (.pattern re)
                      (range 0 (inc (.groupCount input))))))

(defn- nested-context
  ^AnalysisContext [match-result parent-pattern]
  (let [end-re (some-> (:end parent-pattern) (replace-back-references match-result))]
    (->AnalysisContext parent-pattern end-re)))

(defn- first-match
  ^Match [contexts line start]
  (let [context ^AnalysisContext (peek contexts)]
    (reduce (fn [^Match best pattern]
              (let [[type re] (or (find pattern :match) (find pattern :begin))
                    matcher (util/re-matcher-from re line start)]
                (if (and (.find matcher)
                         (or (nil? best)
                             (< (.start matcher)
                                (.start ^MatchResult (.match-result best)))))
                  (let [match-result (.toMatchResult matcher)
                        contexts (case type
                                   :match contexts
                                   :begin (conj contexts (nested-context match-result pattern)))]
                    (->Match type contexts match-result pattern))
                  best)))
            (when-some [end-matcher (some-> context .end-re (util/re-matcher-from line start))]
              (when (.find end-matcher)
                (->Match :end (pop contexts) (.toMatchResult end-matcher) (.parent-pattern context))))
            (some-> context .parent-pattern :patterns))))

(defn- append-run! [transient-runs [index :as run]]
  (let [last-coll-index (dec (count transient-runs))
        prev-run-index (first (get transient-runs last-coll-index))]
    (if (= index prev-run-index)
      (assoc! transient-runs last-coll-index run)
      (conj! transient-runs run))))

(defn- append-match-result! [transient-runs ^MatchResult match-result scope captures end-match-result-scope?]
  (let [parent-scope (second (get transient-runs (dec (count transient-runs))))
        sorted-scope-changes (cond-> (into [(pair (.start match-result) scope)]
                                           (sort-by first
                                                    (mapcat (fn [^long group]
                                                              (let [scope (:name (get captures group))
                                                                    start (.start match-result group)
                                                                    end (.end match-result group)]
                                                                (when (and (some? scope)
                                                                           (not= -1 start))
                                                                  (pair (pair start scope)
                                                                        (pair end :end)))))
                                                            (range 0 (inc (.groupCount match-result))))))
                                     end-match-result-scope? (conj (pair (.end match-result) :end)))]
    (prn "--------------------")
    (prn "append-match-result!" scope)
    (prn "  parent-scope" parent-scope)
    (prn "  sorted-scope-changes" sorted-scope-changes)
    (prn)
    (prn "  loop:")
    (loop [parent-scopes (list parent-scope)
           scope-changes sorted-scope-changes
           transient-runs transient-runs]
      (prn "    parent-scopes" parent-scopes)
      (prn "    transient-runs" (mapv #(get transient-runs %)
                                      (range 0 (count transient-runs))))
      (if-some [scope-change (first scope-changes)]
        (let [[index scope-or-end] scope-change
              parent-scopes (if (= :end scope-or-end)
                              (pop parent-scopes)
                              (conj parent-scopes scope-or-end))
              run (if (= :end scope-or-end)
                    (pair index (peek parent-scopes))
                    scope-change)]
          (recur parent-scopes
                 (next scope-changes)
                 (if (some? run)
                   (append-run! transient-runs run)
                   transient-runs)))
        transient-runs))))

(defn- append-match! [transient-runs ^MatchResult match-result pattern]
  (prn "append-match!")
  (let [scope (:name pattern)
        captures (:captures pattern)]
    (append-match-result! transient-runs match-result scope captures true)))

(defn- append-begin! [transient-runs ^MatchResult match-result pattern]
  (prn "append-begin!")
  (let [scope (:name pattern)
        content-scope (:content-name pattern)
        captures (or (:begin-captures pattern) (:captures pattern))]
    (cond-> (append-match-result! transient-runs match-result scope captures false)
            content-scope (append-run! (pair (.start match-result) content-scope)))))

(defn- append-end! [transient-runs ^MatchResult match-result pattern]
  (prn "append-end!")
  (let [scope (:name pattern)
        content-scope (:content-name pattern)
        captures (or (:end-captures pattern) (:captures pattern))]
    (cond-> transient-runs
            content-scope (append-run! (pair (.start match-result) content-scope))
            true (append-match-result! match-result scope captures true))))

(defn- find-parent-scope [^AnalysisContext context]
  (when-some [parent-pattern (.parent-pattern context)]
    (or (:content-name parent-pattern)
        (:name parent-pattern))))

(defn make-context [scope patterns]
  (->AnalysisContext {:name scope :patterns patterns} nil))

(defn analyze [contexts line]
  (let [root-scope (some find-parent-scope contexts)]
    (loop [start 0
           contexts contexts
           runs (transient [(pair 0 root-scope)])]
      (if-some [match (first-match contexts line start)]
        (let [match-result ^MatchResult (.match-result match)]
          (recur (.end match-result)
                 (.contexts match)
                 (case (.type match)
                   :match (append-match! runs match-result (.pattern match))
                   :begin (append-begin! runs match-result (.pattern match))
                   :end (append-end! runs match-result (.pattern match)))))
        (pair contexts (persistent! runs))))))
