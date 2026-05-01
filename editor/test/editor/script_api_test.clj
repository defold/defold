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

(ns editor.script-api-test
  (:require [clojure.string :as string]
            [clojure.test :refer :all]
            [editor.script-api :as sapi]))

(defn std
  ([name type]
   (std name type nil))
  ([name type doc]
   (cond-> {:type type
            :name name
            :display-string name
            :insert {:type :plaintext :value name}}
           doc
           (assoc :doc {:type :markdown :value doc}))))

(def just-a-variable
  "
- name: other
  type: number")

(def just-a-variable-expected-result
  {"" [(std "other" :variable)]})

(def empty-table
  "
- name: table
  type: table")

(def empty-table-expected-result
  {"" [(std "table" :module)]})

(def table-with-members
  "
- name: other
  type: table
  desc: 'Another table'
  members:
    - name: Hello
      type: number")

(def table-with-members-expected-result
  {"" [(std "other" :module "Another table")]
   "other" [(std "Hello" :variable)]})

(def function-with-one-parameter
  "
- name: fun
  type: function
  desc: This is super great function!
  parameters:
    - name: plopp
      type: plupp")

(def function-with-one-parameter-expected-result
  {""
   [{:type :function
     :name "fun"
     :doc {:type :markdown :value "This is super great function!\n\n**Parameters:**<br><dl><dt><code>plopp <small>plupp</small></code></dt></dl>"}
     :display-string "fun(plopp)"
     :insert {:type :snippet :value "fun(${1:plopp})"}}]})

(def empty-top-level-definition
  "- ")

(def empty-top-level-definition-expected-result
  {})

(def broken-table-member-list
  "
- name: other
  type: table
  desc: 'Another table'
  members:
    - nam")

(def broken-table-member-list-expected-result
  {"" [(std "other" :module "Another table")]})

(def no-type-means-variable
  "
- name: hej")

(def no-type-means-variable-expected-result
  {"" [(std "hej" :variable)]})

(def function-with-optional-parameter
  "
- name: fun
  type: function
  parameters:
    - name: optopt
      type: integer
      optional: true")

(def function-with-optional-parameter-expected-result
  {""
   [{:type :function
     :name "fun"
     :display-string "fun([optopt])"
     :insert {:type :snippet
              :value "fun()"}
     :doc {:type :markdown
           :value "\n\n**Parameters:**<br><dl><dt><code>optopt <small>integer</small></code></dt></dl>"}}]})

(defn convert
  [source]
  (sapi/lines->completion-info (string/split-lines source)))

(def table-with-nested-members
  "
- name: other
  type: table
  desc: 'Another table'
  members:
    - name: Hello
      type: table
      members:
      - name: opt
        type: integer
        optional: true")

(def table-with-nested-members-expected-result
  {"" [(std "other" :module "Another table")]
   "other" [(std "Hello" :module)]
   "other.Hello" [(std "opt" :variable)]})

(deftest conversions
  (are [x y] (= x (convert y))
    just-a-variable-expected-result just-a-variable
    empty-table-expected-result empty-table
    table-with-members-expected-result table-with-members
    function-with-one-parameter-expected-result function-with-one-parameter
    empty-top-level-definition-expected-result empty-top-level-definition
    broken-table-member-list-expected-result broken-table-member-list
    function-with-optional-parameter-expected-result function-with-optional-parameter
    no-type-means-variable-expected-result no-type-means-variable
    table-with-nested-members-expected-result table-with-nested-members))

