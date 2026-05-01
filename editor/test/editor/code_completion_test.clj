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

(ns editor.code-completion-test
  (:require [clojure.test :refer :all]
            [editor.code-completion :as code-completion]))

(deftest evaluate-snippet-test
  (are [snippet-string evaluated-snippet]
    (= evaluated-snippet (code-completion/evaluate-snippet snippet-string))
    ;; tab triggers
    "$1$1" {:insert-string ""
            :tab-triggers [{:ranges [[0 0]]}]}
    "$1$2$3" {:insert-string ""
              :tab-triggers [{:ranges [[0 0]]}]}
    "$1${1:foo}" {:insert-string "foofoo"
                  :tab-triggers [{:ranges [[0 3] [3 6]]}]}

    ;; exits
    "function function_name($1)\n\t$0\nend" {:insert-string "function function_name()\n\t\nend"
                                             :exit-ranges [[26 26]]
                                             :tab-triggers [{:ranges [[23 23]]}]}
    "double $0=$0" {:insert-string "double ="
                    :exit-ranges [[7 7]
                                  [8 8]]}
    "$0$0" {:insert-string ""
            :exit-ranges [[0 0]]}

    ;; choices
    "${1|a,b,c|}" {:insert-string "a"
                   :tab-triggers [{:ranges [[0 1]]
                                   :choices ["a" "b" "c"]}]}
    "${1|a\\|b,c\\,d,e\\}f|}" {:insert-string "a|b"
                               :tab-triggers [{:ranges [[0 3]]
                                               :choices ["a|b" "c,d" "e}f"]}]}

    ;; errors
    "${1" {:insert-string "${1"}
    "${1|a,b,|}" {:insert-string "${1|a,b,|}"}
    "${1: $2" {:insert-string "${1: "
               :tab-triggers [{:ranges [[5 5]]}]}))

(deftest evaluate-snippet-insert-string-test
  (are [snippet-string insert-string]
    (= insert-string (:insert-string (code-completion/evaluate-snippet snippet-string)))
    ;; tab triggers
    "$1$2$3" ""
    "<$1> {${2}}" "<> {}"

    ;; placeholders
    "${1:}" ""
    "${1:foo}" "foo"
    "${1:foo($2)}" "foo()"
    "${1:foo${2:bar}}" "foobar"
    "${1:foo(${2|a,b,c|})}" "foo(a)"
    "${1:foo ${VAR:var}}" "foo var"
    "${1:foo ${VAR:var ${3|choice|}}}" "foo var choice"

    ;; choice
    "${1|a,b,c|}" "a"
    "${1||}" ""

    ;; variable
    "$VOO" ""
    "${VOO}" ""
    "${VOO:foo}" "foo"
    "${VOO:foo(${2:bar})}" "foo(bar)"

    ;; escaping
    "${1:{\\}}" "{}"
    "${VAR:\\$}" "$"
    "${1:a\nb}" "a\nb"
    "${1:a|b}" "a|b"
    "${1|a\\|b,c,d|}" "a|b"
    "${1|a\\,b,b\\,c,c\\,d|}" "a,b"

    ;; newlines
    "1\r2\n3\r\n4" "1\n2\n3\n4"
    "foo\n\r\n\r" "foo\n\n\n"
    "foo\r\n\n\n\n" "foo\n\n\n\n"))