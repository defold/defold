;; Copyright 2020-2024 The Defold Foundation
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

(ns editor.code.syntax
  (:require [clojure.string :as string]
            [editor.code.util :as util]
            [util.coll :refer [pair]])
  (:import (java.util.regex MatchResult Pattern)))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

;; This is a simplified implementation of TextMate-style syntax highlighting
;; that allows us to adopt rules from existing .tmLanguage files. At some point
;; we might want to implement full support for TextMate grammars, since it has
;; been widely adopted by modern text editors. To fully support them would take
;; a bit of work. Here are a few useful resources that might help guide this
;; effort in the future:
;;
;; Writing a TextMate Grammar: Some Lessons Learned
;;   https://www.apeth.com/nonblog/stories/textmatebundle.html
;;
;; The Language Grammars section from the TextMate manual
;;   https://macromates.com/manual/en/language_grammars
;;
;; The Indentation Rules section from the TextMate manual appendix
;;   https://macromates.com/manual/en/appendix#indentation_rules

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

(defn- append-run! [transient-runs index scope]
  (let [last-coll-index (dec (count transient-runs))
        prev-run-index (first (get transient-runs last-coll-index))
        run (pair index scope)]
    (if (= index prev-run-index)
      (assoc! transient-runs last-coll-index run)
      (conj! transient-runs run))))

(defn- append-captures! [transient-runs parent-scope ^MatchResult match-result captures]
  (reduce (fn [runs group]
            (or (when-some [capture (captures group)]
                  (when (not= -1 (.start match-result group))
                    (when-some [scope (:name capture)]
                      (-> runs
                          (append-run! (.start match-result group) scope)
                          (append-run! (.end match-result group) parent-scope)))))
                runs))
          transient-runs
          (range 0 (inc (.groupCount match-result)))))

(defn- append-match! [transient-runs parent-scope ^MatchResult match-result pattern]
  (let [scope (:name pattern)
        captures (:captures pattern)]
    (cond-> transient-runs
            scope (append-run! (.start match-result) scope)
            captures (append-captures! (or scope parent-scope) match-result captures)
            scope (append-run! (.end match-result) parent-scope))))

(defn- append-begin! [transient-runs parent-scope ^MatchResult match-result pattern]
  (let [scope (:name pattern)
        content-scope (:content-name pattern)
        captures (or (:begin-captures pattern) (:captures pattern))]
    (cond-> transient-runs
            scope (append-run! (.start match-result) scope)
            captures (append-captures! (or scope parent-scope) match-result captures)
            content-scope (append-run! (.end match-result) content-scope))))

(defn- append-end! [transient-runs parent-scope ^MatchResult match-result pattern]
  (let [scope (:name pattern)
        content-scope (:content-name pattern)
        captures (or (:end-captures pattern) (:captures pattern))]
    (cond-> transient-runs
            content-scope (append-run! (.start match-result) content-scope)
            captures (append-captures! (or scope parent-scope) match-result captures)
            scope (append-run! (.end match-result) parent-scope))))

(defn- find-parent-scope [^AnalysisContext context]
  (when-some [parent-pattern (.parent-pattern context)]
    (or (:content-name parent-pattern)
        (:name parent-pattern))))

(defn make-context [scope patterns]
  (assert (string? (not-empty scope)))
  (assert (seq patterns))
  (->AnalysisContext {:name scope :patterns patterns} nil))

(defn analyze [contexts line]
  (let [root-scope (some find-parent-scope contexts)]
    ;; Skip very long lines since these might lock up the regex engine.
    (if (< 1024 (count line))
      (pair contexts [(pair 0 root-scope)])
      (loop [start 0
             contexts contexts
             runs (transient [(pair 0 root-scope)])]
        (if-some [match (first-match contexts line start)]
          (let [parent-scope (some find-parent-scope (.contexts match))
                match-result ^MatchResult (.match-result match)]
            (recur (.end match-result)
                   (.contexts match)
                   (case (.type match)
                     :match (append-match! runs parent-scope match-result (.pattern match))
                     :begin (append-begin! runs parent-scope match-result (.pattern match))
                     :end (append-end! runs parent-scope match-result (.pattern match)))))
          (pair contexts (persistent! runs)))))))
