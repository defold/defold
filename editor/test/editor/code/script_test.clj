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

(ns editor.code.script-test
  (:require [clojure.string :as string]
            [clojure.test :refer :all]
            [editor.code.data :as data]
            [editor.code.data-test :refer [layout-info]]
            [editor.code.script :as script]))

(def ^:private indent-string "    ")
(def ^:private indent-level-pattern (data/indent-level-pattern (count indent-string)))

(defn- insert-lines [lines cursor-ranges inserted-lines]
  (#'data/insert-lines-seqs indent-level-pattern "    " script/lua-grammar lines cursor-ranges nil (layout-info lines) (repeat inserted-lines)))

(def ^:private xform-test-lines->lines
  (map #(string/replace % #"\|" "")))

(def ^:private xform-test-lines->cursors
  (comp (map-indexed vector)
        (mapcat (fn [[row line]]
                  (let [matcher (re-matcher #"\|" line)]
                    (loop [prior-cursor-count 0
                           result (transient [])]
                      (if-not (.find matcher)
                        (persistent! result)
                        (recur (inc prior-cursor-count)
                               (conj! result (data/->Cursor row (- (.start matcher) prior-cursor-count)))))))))))

(def ^:private xform-test-lines->cursor-ranges
  (comp xform-test-lines->cursors
        (map data/Cursor->CursorRange)))

(deftest insert-indentation-test
  (are [inserted-lines before after]
    (= {:lines           (into [] xform-test-lines->lines after)
        :cursor-ranges   (into [] xform-test-lines->cursor-ranges after)
        :invalidated-row (:row (first (sequence xform-test-lines->cursors before)))}
       (insert-lines (into [] xform-test-lines->lines before)
                     (into [] xform-test-lines->cursor-ranges before)
                     inserted-lines))
    [" "]
    ["|"]
    [" |"]

    [" "]
    [" |"]
    ["  |"]

    [""
     ""]
    ["function foo()|"]
    ["function foo()"
     "    |"]

    [""
     ""]
    ["for i = 0, 10 do|"]
    ["for i = 0, 10 do"
     "    |"]

    [""
     ""]
    ["for k, v in pairs(t) do|"]
    ["for k, v in pairs(t) do"
     "    |"]

    [""
     ""]
    ["if i == 0 then|"]
    ["if i == 0 then"
     "    |"]

    [""
     ""]
    ["if i == 0 then"
     "    print(i)|"]
    ["if i == 0 then"
     "    print(i)"
     "    |"]

    [""
     "elseif i == 1 then"
     ""]
    ["if i == 0 then"
     "    print(i)|"]
    ["if i == 0 then"
     "    print(i)"
     "elseif i == 1 then"
     "    |"]

    ["d"]
    ["if i == 0 then"
     "    print(i)"
     "    en|"]
    ["if i == 0 then"
     "    print(i)"
     "end|"]

    ["d"]
    ["if i == 0 then"
     "    en|"]
    ["if i == 0 then"
     "end|"]

    [""
     ""]
    ["for x = 0, 10 do"
     "    for y = 0, 10 do|"]
    ["for x = 0, 10 do"
     "    for y = 0, 10 do"
     "        |"]

    ["d"]
    ["for x = 0, 10 do"
     "    for y = 0, 10 do"
     "        en|"]
    ["for x = 0, 10 do"
     "    for y = 0, 10 do"
     "    end|"]

    [""
     ""]
    ["for x = 0, 10 do"
     "    for y = 0, 10 do"
     "    end|"]
    ["for x = 0, 10 do"
     "    for y = 0, 10 do"
     "    end"
     "    |"]

    ["d"]
    ["for x = 0, 10 do"
     "    for y = 0, 10 do"
     "    end"
     "    en|"]
    ["for x = 0, 10 do"
     "    for y = 0, 10 do"
     "    end"
     "end|"]

    [""
     ""]
    ["local opts = {|"]
    ["local opts = {"
     "    |"]

    [""
     ""]
    ["local opts = {"
     "    verbose = true|"]
    ["local opts = {"
     "    verbose = true"
     "    |"]

    ["}"]
    ["local opts = {"
     "    verbose = true"
     "    |"]
    ["local opts = {"
     "    verbose = true"
     "}|"]

    [""
     ""]
    ["first| second"]
    ["first"
     "| second"]

    [""
     ""]
    ["    first| second"]
    ["    first"
     "    | second"]

    [""
     ""]
    ["    first| second| third"]
    ["    first"
     "    | second"
     "    | third"]

    ["e"]
    ["function foo()"
     "|    -- comment"]
    ["function foo()"
     "e|    -- comment"]

    [""
     ""]
    ["    -- will do something|"]
    ["    -- will do something"
     "    |"]

    [""
     ""]
    ["    s = \"will do something\"|"]
    ["    s = \"will do something\""
     "    |"]

    [""
     ""]
    ["    s = 'will do something'|"]
    ["    s = 'will do something'"
     "    |"]

    [""
     ""]
    ["    -- will end something|"]
    ["    -- will end something"
     "    |"]

    [""
     ""]
    ["    s = \"will end something\"|"]
    ["    s = \"will end something\""
     "    |"]

    [""
     ""]
    ["    s = 'will end something'|"]
    ["    s = 'will end something'"
     "    |"]))
