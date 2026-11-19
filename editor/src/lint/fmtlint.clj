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

;; This standalone tool has no dependency on the editor's util.coll namespace.
(ns ^{:clj-kondo/config '{:linters {:defold/prefer-coll-into-> {:level :off}}}} fmtlint
  (:require [clojure.java.io :as io]
            [clojure.string :as string]
            [rewrite-clj.node :as node]
            [rewrite-clj.parser :as parser]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(defn- forms [parent]
  (into []
        (remove (fn [child]
                  (contains? #{:whitespace :newline :comma :comment :uneval}
                             (node/tag child))))
        (node/children parent)))

(defn- call-name [head]
  (when (and head (= :token (node/tag head)))
    (let [value (node/sexpr head)]
      (when (and (symbol? value)
                 (contains? #{nil "clojure.core"} (namespace value)))
        (name value)))))

(defn- collect-aliases [aliases form]
  (let [aliases
        (if-not (= :vector (node/tag form))
          aliases
          (let [[library & options] (forms form)]
            (reduce (fn [aliases [option alias]]
                      (if (and library alias
                               (= :token (node/tag library))
                               (= :token (node/tag option))
                               (contains? #{:as :as-alias} (node/sexpr option)))
                        (assoc aliases (str (node/sexpr alias)) (str (node/sexpr library)))
                        aliases))
                    aliases
                    (eduction (partition-all 2) options))))]
    (if-not (node/inner? form)
      aliases
      (reduce collect-aliases aliases (forms form)))))

(defn- namespace-aliases [root]
  (reduce (fn [aliases form]
            (if (and (= :list (node/tag form))
                     (= "ns" (call-name (first (forms form)))))
              (collect-aliases aliases form)
              aliases))
          {}
          (forms root)))

(defn- call-symbol [aliases head]
  (when (and head (= :token (node/tag head)))
    (let [value (node/sexpr head)]
      (when (symbol? value)
        (let [prefix (namespace value)]
          (symbol (get aliases prefix (or prefix "clojure.core")) (name value)))))))

(declare walk)

(defn- binding-paths [children]
  ;; Check each reader-conditional alternative using its original positions.
  (reduce
    (fn [paths child]
      (let [alternatives
            (if (and (= :reader-macro (node/tag child))
                     (contains? #{"?" "?@"} (node/string (first (forms child)))))
              (let [[reader branches] (forms child)
                    splicing (= "?@" (node/string reader))]
                (into (if splicing [[]] [])
                      (mapcat (fn [[_ branch]]
                                (if splicing
                                  (binding-paths (forms branch))
                                  [[(with-meta branch (merge (meta branch) (meta child)))]])))
                      (eduction (partition-all 2) (forms branches))))
              [[child]])]
        (into []
              (mapcat (fn [path]
                        (into [] (map #(into path %)) alternatives)))
              paths)))
    [[]]
    children))

(defn- modifier? [binding-name]
  (and (= :token (node/tag binding-name))
       (keyword? (node/sexpr binding-name))))

(defn- multiline? [form]
  (let [{:keys [row end-row]} (meta form)]
    (and row end-row (< (long row) (long end-row)))))

(defn- argument-groups [arguments]
  ;; Only a suffix made entirely of keyword/value pairs gets paired layout.
  (loop [end (count arguments)
         suffix []]
    (if (and (>= end 2) (modifier? (nth arguments (- end 2))))
      (recur (- end 2) (conj suffix (subvec arguments (- end 2) end)))
      (into (mapv vector (subvec arguments 0 end)) (rseq suffix)))))

(defn- argument-findings [head arguments]
  (loop [groups (argument-groups arguments)
         index 0
         previous head
         vertical false
         findings []]
    (if (= index (count groups))
      findings
      (let [group (nth groups index)
            argument (first group)

            vertical
            (or vertical
                (multiline? previous)
                (not= (:row (meta head)) (:row (meta argument))))

            findings
            (cond-> findings
              (and vertical (= (:end-row (meta previous)) (:row (meta argument))))
              (conj (assoc (select-keys (meta argument) [:row :col])
                      :message "Start each vertically listed argument or keyword/value pair on its own line.")))]
        (recur groups (inc index) (peek group) vertical findings)))))

(defn- underscore-binding? [form]
  (if (= :meta (node/tag form))
    (recur (peek (forms form)))
    (and (= :token (node/tag form)) (= '_ (node/sexpr form)))))

(defn- initializer-findings [pairs]
  (into []
        (keep
          (fn [[binding-name value]]
            (when (and value
                       (= (:end-row (meta binding-name)) (:row (meta value)))
                       (not (underscore-binding? binding-name)))
              (let [message
                    (cond
                      (or (multiline? binding-name) (multiline? value))
                      "Start the initializer on a new line when either side is multiline."

                      (> (long (:end-col (meta value))) 81)
                      "Start the initializer on a new line when it extends past column 80.")]
                (when message
                  (assoc (select-keys (meta binding-name) [:row :col])
                    :message message))))))
        pairs))

(defn- blank-line-between? [lines previous-value binding-name]
  (reduce (fn [_ row]
            (if (string/blank? (nth lines row))
              (reduced true)
              false))
          false
          (range (long (:end-row (meta previous-value)))
                 (dec (long (:row (meta binding-name)))))))

(defn- binding-findings [lines pairs]
  (into []
        (comp
          (drop 1)
          (keep-indexed
            (fn [index [binding-name value]]
              (let [[previous-name previous-value] (nth pairs index)

                    message
                    (when-not (modifier? binding-name)
                      (cond
                        (= (:end-row (meta previous-value)) (:row (meta binding-name)))
                        "Start each binding on its own line."

                        (and (not (modifier? previous-name))
                             (or (not= (:row (meta previous-name)) (:end-row (meta previous-value)))
                                 (not= (:row (meta binding-name)) (:end-row (meta value))))
                             (not (blank-line-between? lines previous-value binding-name)))
                        "Separate bindings with a blank line when either binding is multiline."))]
                (when message
                  (assoc (select-keys (meta binding-name) [:row :col])
                    :message message))))))
        pairs))

(defn- walk-bindings [context bindings check-initializers]
  (into []
        (mapcat
          (fn [path]
            (let [pairs (into [] (partition-all 2) path)]
              (into (cond-> (binding-findings (:lines context) pairs)
                      check-initializers (into (initializer-findings pairs)))
                    (mapcat (fn [[binding-name value]]
                              (if (and (= :token (node/tag binding-name))
                                       (= :let (node/sexpr binding-name))
                                       (= :vector (node/tag value)))
                                (walk-bindings context value true)
                                (into (walk context binding-name) (walk context value)))))
                    pairs))))
        (binding-paths (forms bindings))))

(defn- walk-function [context children]
  ;; Multi-arity clauses are declarations, not calls through a vector.
  (into []
        (mapcat (fn [child]
                  (if (and (= :list (node/tag child))
                           (= :vector (some-> child forms first node/tag)))
                    (into [] (mapcat #(walk context %)) (forms child))
                    (walk context child))))
        children))

(defn- walk-methods [context children]
  (into []
        (mapcat (fn [child]
                  (if (= :list (node/tag child))
                    (walk-function context (subvec (forms child) 1))
                    (walk context child))))
        children))

(defn- walk-reader [context form]
  (let [[reader branches] (forms form)]
    (if-not (contains? #{"?" "?@"} (node/string reader))
      (into [] (mapcat #(walk context %)) (forms form))
      (into []
            (mapcat (fn [[_ branch]]
                      (if (= "?@" (node/string reader))
                        (into [] (mapcat #(walk context %)) (forms branch))
                        (walk context branch))))
            (eduction (partition-all 2) (forms branches))))))

(defn- walk-list [context form]
  (let [[head bindings & body] (forms form)
        function-symbol (call-symbol (:aliases context) head)

        function-name
        (when (= "clojure.core" (some-> function-symbol namespace))
          (name function-symbol))]
    (cond
      (or (nil? head) (contains? #{"quote" "var" "ns"} function-name))
      []

      (or (contains? #{"fn" "fn*" "defn" "defn-" "defmacro" "defmethod"} function-name)
          (contains? #{'dynamo.graph/fnk 'dynamo.graph/defnk 'util.fn/defn-cached}
                     function-symbol))
      (walk-function context (subvec (forms form) 1))

      (contains? #{"defrecord" "deftype" "definterface" "defprotocol"
                   "reify" "proxy" "extend-type" "extend-protocol"}
                 function-name)
      (walk-methods context (subvec (forms form) 1))

      (= "letfn" function-name)
      (into (walk-methods context (forms bindings)) (mapcat #(walk context %)) body)

      (= "case" function-name)
      (into (walk context bindings)
            (mapcat (fn [[test result]] (walk context (or result test))))
            (eduction (partition-all 2) body))

      (and (contains? #{"let" "let*" "loop" "loop*" "binding" "with-open"
                        "with-local-vars" "for" "doseq"
                        "if-let" "if-some" "when-let" "when-some" "when-first"}
                      function-name)
           bindings
           (= :vector (node/tag bindings)))
      (into (walk-bindings context bindings (contains? #{"let" "let*"} function-name))
            (mapcat #(walk context %))
            body)

      ;; These forms have branches, bindings, declarations, or body syntax.
      (or (contains? #{"cond" "condp" "if" "if-not" "when" "when-not" "when-first"
                       "do" "try" "catch" "finally" "def" "defonce" "defmulti"
                       "dotimes" "with-redefs" "with-out-str" "with-in-str"
                       "with-precision" "locking" "time" "comment" "lazy-seq"
                       "->" "->>" "some->" "some->>" "cond->" "cond->>" "as->" "doto" ".."}
                     function-name)
          (contains? #{'dynamo.graph/defnode 'editor.graph-util/set-properties-from-pb-map}
                     function-symbol))
      (into [] (mapcat #(walk context %)) (subvec (forms form) 1))

      :else
      (into (into []
                  (mapcat #(argument-findings head %))
                  (binding-paths (subvec (forms form) 1)))
            (mapcat #(walk context %))
            (forms form)))))

(defn- walk [context form]
  (when form
    (case (node/tag form)
      (:quote :syntax-quote :uneval) []
      (:list :fn) (walk-list context form)
      :reader-macro (walk-reader context form)
      :meta (walk context (peek (forms form)))
      (if (node/inner? form)
        (into [] (mapcat #(walk context %)) (forms form))
        []))))

(defn lint
  "Return layout findings with one-based rows and columns for source."
  [source]
  (let [root (parser/parse-string-all source)

        context
        {:lines (string/split-lines source)
         :aliases (namespace-aliases root)}]
    (into [] (distinct) (walk context root))))

(defn check-files
  "Print layout diagnostics for filenames and return an exit status.

  Returns 0 for clean files, 1 for layout warnings, or 2 for input errors."
  [filenames]
  (reduce
    (fn [status filename]
      (try
        (let [findings (lint (slurp (io/file filename)))]
          (doseq [{:keys [row col message]} findings]
            (println (format "%s:%s:%s: warning: %s" filename row col message)))
          (max (long status) (if (pos? (count findings)) 1 0)))
        (catch Exception exception
          (println (format "%s: error: %s" filename (ex-message exception)))
          2)))
    0
    filenames))

