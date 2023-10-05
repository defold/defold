;; Copyright 2020-2023 The Defold Foundation
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

(ns editor.code-completion
  (:require [clj-antlr.core :as antlr]
            [clojure.java.io :as io]
            [clojure.spec.alpha :as s]
            [internal.util :as util])
  (:import [java.net URI]))

;; Completion spec
(s/def ::name string?) ;; used for filtering
(s/def ::display-string string?) ;; used for view
(s/def :editor.code-completion.insert/type #{:snippet :plaintext})
(s/def :editor.code-completion.insert/value string?)
(s/def ::insert
  (s/keys :req-un [:editor.code-completion.insert/type
                   :editor.code-completion.insert/value]))
(s/def ::type
  #{;; defold-specific
    :message
    ;; shared with vscode
    :text :method :function :constructor :field :variable :class :interface
    :module :property :unit :value :enum :keyword :snippet :color :file
    :reference :folder :enum-member :constant :struct :event :operator
    :type-parameter})
(defn- one-character-long? [^String s]
  (= 1 (.length s)))
(s/def ::commit-characters
  (s/coll-of (s/and string? one-character-long?) :kind set?))
;; Detail is rendered after :display-string less prominently, e.g. function
;; signature or type annotation
(s/def ::detail string?)
(s/def :editor.code-completion.doc/type #{:markdown :plaintext})
(s/def :editor.code-completion.doc/value string?)
(s/def :editor.code-completion.doc/base-url #(instance? URI %))
(s/def ::doc
  (s/keys :req-un [:editor.code-completion.doc/type
                   :editor.code-completion.doc/value]
          :opt-un [:editor.code-completion.doc/base-url]))
(s/def ::completion
  (s/keys :req-un [::name ::display-string ::insert]
          :opt-un [::type ::commit-characters ::detail ::doc]))

(defn make
  "Create a code completion

  Required arg:
    name    code completion identifier

  Optional kv-args:
    :insert-snippet       LSP-style snippet string for the code completion,
                          mutually exclusive with :insert-string
    :insert-string        literal snippet string for the code completion,
                          mutually exclusive with :snippet
    :display-string       code completion label shown in the completion popup
    :type                 completion type, either Defold-specific :message or
                          LSP-style :module, :function, :property etc.
    :commit-characters    a set of one character long strings that accept the
                          suggested completion if typed when the completion is
                          selected
    :detail               string with additional information to display in the
                          completion popup, e.g. type or symbol information
    :doc                  if a string, it is assumed to be a markdown
                          documentation, otherwise, it can be either:
                          - {:type :plaintext
                             :value \"literal doc\"}
                          - {:type :markdown
                             :value \"markdown doc\"
                             :base-url URI} (optional URI for resolving links)"
  [name & {:keys [insert-snippet insert-string display-string type commit-characters detail doc]}]
  {:pre [(not (and insert-string insert-snippet))]
   :post [(s/assert ::completion %)]}
  (cond-> {:name name
           :display-string (or display-string name)
           :insert (cond
                     insert-snippet {:type :snippet :value insert-snippet}
                     insert-string {:type :plaintext :value insert-string}
                     :else {:type :plaintext :value name})}
          type
          (assoc :type type)
          commit-characters
          (assoc :commit-characters commit-characters)
          detail
          (assoc :detail detail)
          doc
          (assoc :doc (if (string? doc)
                        {:type :markdown :value doc}
                        doc))))

(def ^:private snippet-parser
  (antlr/parser (slurp (io/resource "lsp-snippet.g4"))))

;; Insertion spec
(defn- monotonously-increasing-range? [{:keys [from to]}]
  (<= from to))
(s/def ::insert-string string?)
(s/def ::range (s/and (s/cat :from int? :to int?) monotonously-increasing-range?))
(defn- non-intersecting-sorted-ranges? [conformed-ranges]
  (->> conformed-ranges
       (partition 2 1)
       (every? (fn [[a b]]
                 (<= (:to a) (:from b))))))
(s/def ::ranges (s/and (s/coll-of ::range :kind vector?) non-intersecting-sorted-ranges?))
(s/def ::exit-ranges ::ranges)
(s/def ::choices (s/coll-of string? :kind vector?))
(s/def ::tab-trigger (s/keys :req-un [::ranges]
                             :opt-un [::choices]))
(s/def ::tab-triggers (s/coll-of ::tab-trigger :kind vector?))
(s/def ::insertion
  (s/keys :req-un [::insert-string]
          :opt-un [::tab-triggers ::exit-ranges]))

(defn evaluate-snippet
  "Convert LSP-style snippet string to an editor snippet

  The editor supports:
    - positional tab stops (tab triggers in editor terms), e.g. $1 and ${1}
    - escaping $, } and \\ with \\
    - exit tab stops (triggers in editor terms) $0
    - default values, e.g. ${1:default}
    - choices, e.g. ${1|one,two,three|} (first choice is inserted by default)

  The editor recognizes, but does not support:
    - nested placeholders, ${1:foo ${2:bar}} — gets converted to ${1:foo bar}
    - variables, ${TM_FILENAME:foo} — gets converted to foo
  The editor does not recognize:
    - variable transforms, e.g. ${TM_FILENAME/(.*)\\..+$/$1/} is error and gets
      inserted as is

  Returns insertion map (see [[insertion]] for details)

  See also:
    https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#snippet_syntax"
  [snippet-string]
  (let [ast (antlr/parse snippet-parser snippet-string)]
    ;; Parsing notes
    ;; Consider these 2 snippets: $1$1 and $1${1:fo}, what should the evaluated
    ;; snippet be? In vscode, the former snippet evaluates to a string "" with
    ;; a single tab trigger 0..0, while the latter evaluates to a string "fofo"
    ;; with 2 tab triggers 0..2 and 2..4.
    ;; To support this behavior, we need to process the ast 2 times:
    ;; 1. collect information about tab triggers in the snippet, e.g. build a
    ;;    map from tab trigger id to its choices and placeholder text. It is
    ;;    possible to define both, multiple times, for the same placeholder! We
    ;;    collect both, but otherwise use last-wins policy.
    ;; 2. build the insert string using the collected tab trigger information,
    ;;    and at the same time collect indices for tab triggers in the resulting
    ;;    string
    ;;
    ;; Additionally, we don't support nested placeholders like ${1:aa ${2:bb}},
    ;; but we still want to build meaningful default strings (equivalent to
    ;; ${1:aa bb}), we do this processing during the "collect tab trigger info"
    ;; step.
    (letfn [;; Part of step 1: convert ast of placeholder contents to string
            (content-string
              ([ast]
               (str (content-string (StringBuilder.) ast)))
              ([^StringBuilder sb [token & children]]
               (case token
                 ;; :snippet, :text, :text_esc and :err are unreachable because
                 ;; they only exist on the top level, while content-string is
                 ;; used to build string for choices, variables and placeholders
                 :rule (content-string sb (first children))
                 :newline (.append sb \newline)
                 ;; tab stop is LSP term, so we use it in the snippet
                 :tab_stop sb
                 ;; :naked_tab_stop and :curly_tab_stop rules are unreachable
                 ;; because we don't recurse into the :tab_stop
                 :placeholder (content-string sb (nth children 4))
                 :placeholder_content (reduce content-string sb children)
                 :placeholder_text (.append sb (first children))
                 :placeholder_esc (.append sb (second children))
                 :choice (content-string sb (nth children 4))
                 :choice_elements (if-let [ast (first children)]
                                    (content-string sb ast)
                                    sb)
                 :choice_element (reduce content-string sb children)
                 :choice_text (.append sb (first children))
                 :choice_esc (.append sb (second children))
                 :variable (content-string sb (first children))
                 :naked_variable sb
                 :curly_variable sb
                 :placeholder_variable (content-string sb (nth children 4))
                 sb)))
            ;; Step 1: collect tab trigger information
            (prepare-tab-triggers [tab-triggers [token & children]]
              (case token
                :snippet (reduce prepare-tab-triggers tab-triggers children)
                :rule (prepare-tab-triggers tab-triggers (first children))
                :tab_stop (reduce prepare-tab-triggers tab-triggers children)
                :naked_tab_stop (update tab-triggers (nth children 1) #(or % {}))
                :curly_tab_stop (update tab-triggers (nth children 2) #(or % {}))
                :placeholder (update tab-triggers (nth children 2) assoc
                                     :placeholder (content-string (nth children 4)))
                :choice (update tab-triggers (nth children 2) assoc
                                :choice (into []
                                              (comp (partition-all 2)
                                                    (map (comp content-string first)))
                                              (next (nth children 4))))
                tab-triggers))]
      (let [;; Execute step 1:
            prepared-tab-triggers (prepare-tab-triggers {} ast)
            ;; Step 2: use collected tab trigger info to produce the resulting string
            sb (StringBuilder.)]
        (letfn [(content-with-ranges [tab-triggers tab-id]
                  (let [start (.length sb)
                        {:keys [placeholder choice]} (get tab-triggers tab-id)
                        _ (.append sb (or placeholder (first choice) ""))
                        end (.length sb)]
                    (update-in tab-triggers [tab-id :ranges] (fnil conj []) [start end])))
                (complete-snippet [tab-triggers [token & children]]
                  (case token
                    :snippet (reduce complete-snippet tab-triggers children)
                    :text (do (.append sb (first children)) tab-triggers)
                    :text_esc (do (.append sb (second children)) tab-triggers)
                    :err (do (.append sb (first children)) tab-triggers)
                    :newline (do (.append sb \newline) tab-triggers)
                    :rule (complete-snippet tab-triggers (first children))
                    :tab_stop (complete-snippet tab-triggers (first children))
                    :naked_tab_stop (content-with-ranges tab-triggers (second children))
                    :curly_tab_stop (content-with-ranges tab-triggers (nth children 2))
                    :placeholder (content-with-ranges tab-triggers (nth children 2))
                    :choice (content-with-ranges tab-triggers (nth children 2))
                    :variable (complete-snippet tab-triggers (first children))
                    :naked_variable tab-triggers
                    :curly_variable tab-triggers
                    :placeholder_variable (do (content-string sb (nth children 4)) tab-triggers)
                    tab-triggers))]
          (let [;; Execute step 2 using prepared tab triggers
                tab-triggers (complete-snippet prepared-tab-triggers ast)
                insert-string (.toString sb)
                ;; Final post-processing:
                ;; handle the exit tab trigger separately (identified as $0),
                ;; sort the other tab triggers etc.
                non-exit-tab-triggers (dissoc tab-triggers "0")
                explicit-exit (get tab-triggers "0")]
            (cond->
              {:insert-string insert-string}
              explicit-exit
              (assoc :exit-ranges (vec (sort (into [] (dedupe) (:ranges explicit-exit)))))
              (pos? (count non-exit-tab-triggers))
              (assoc :tab-triggers
                     ;; We want to merge tab triggers sharing the same range
                     ;; into single tab trigger. We need to deduplicate the
                     ;; ranges over all tab triggers, since we can get a snippet
                     ;; like $1$2 in addition to $1$1.
                     (->> non-exit-tab-triggers
                          (eduction
                            (mapcat
                              (fn [[id tab-trigger]]
                                (eduction
                                  (map #(assoc tab-trigger :id id :range %))
                                  (:ranges tab-trigger))))
                            (util/distinct-by :range))
                          (group-by :id)
                          (sort-by key)
                          (mapv (fn [[_ coll]]
                                  (let [{:keys [choice]} (first coll)]
                                    (cond-> {:ranges (vec (sort (mapv :range coll)))}
                                            choice
                                            (assoc :choices choice))))))))))))))

(defn insertion
  "Convert completion item to insertion item

  Returns a map with following keys:
    :insert-string    required, the string to insert
    :exit-ranges      optional, non-empty sorted vector of non-intersecting
                      from+to index pairs in the range [0, string-length] that
                      defines the exit points for the snippet tab triggers. If
                      absent, it is assumed that selection should move to the
                      end of the inserted snippet
    :tab-triggers     optional, present only if the completion snippet defines
                      tab triggers, a vector of maps with following keys:
                        :ranges     non-empty sorted vector of non-intersecting
                                    from+to index pairs in the range
                                    [0, string-length]
                        :choices    optional, present only if the tab trigger
                                    defines choices, a vector of strings"
  [completion]
  {:pre [(s/assert ::completion completion)]
   :post [(s/assert ::insertion %)]}
  (let [{:keys [type value]} (:insert completion)]
    (case type
      :plaintext {:insert-string value}
      :snippet (evaluate-snippet value))))