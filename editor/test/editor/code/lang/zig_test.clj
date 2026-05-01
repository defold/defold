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

(ns editor.code.lang.zig-test
  (:require [clojure.test :refer :all]
            [editor.code.lang.zig :refer [grammar]]
            [editor.code.syntax :as syntax]))

(defn- analyze-runs [line]
  (let [contexts (list (syntax/make-context (:scope-name grammar) (:patterns grammar)))]
    (second (syntax/analyze contexts line))))

(deftest import-test
  (is (= [[0 "keyword.default.zig"]
          [5 "source.zig"]
          [6 "variable.zig"]
          [9 "source.zig"]
          [12 "support.function.builtin.zig"]
          [19 "source.zig"]
          [20 "punctuation.definition.string.quoted.begin.zig"]
          [21 "string.quoted.double.zig"]
          [24 "punctuation.definition.string.quoted.end.zig"]
          [25 "source.zig"]]
         (analyze-runs "const std = @import(\"std\");"))))

(deftest function-declaration-test
  (is (= [[0 "keyword.storage.zig"]
          [3 "source.zig"]
          [4 "storage.type.function.zig"]
          [6 "source.zig"]
          [7 "entity.name.function.zig"]
          [11 "source.zig"]
          [14 "keyword.operator.bitwise.zig"]
          [15 "keyword.type.zig"]
          [19 "source.zig"]]
         (analyze-runs "pub fn main() !void {"))))

(deftest declaration-and-assignment-test
  (is (= [[0 "keyword.default.zig"]
          [5 "source.zig"]
          [6 "variable.zig"]
          [12 "source.zig"]
          [15 "variable.zig"]
          [18 "source.zig"]
          [19 "variable.zig"]
          [21 "source.zig"]
          [22 "entity.name.function.zig"]
          [31 "source.zig"]
          [34 "entity.name.function.zig"]
          [40 "source.zig"]]
         (analyze-runs "const stdOut = std.io.getStdOut().writer();"))))

(deftest instantiation-test
  (is (= [[0 "keyword.default.zig"]
          [3 "source.zig"]
          [4 "variable.zig"]
          [19 "source.zig"]
          [22 "variable.zig"]
          [25 "source.zig"]
          [26 "entity.name.type.zig"]
          [35 "source.zig"]
          [36 "keyword.type.integer.zig"]
          [38 "source.zig"]
          [40 "entity.name.function.zig"]
          [44 "source.zig"]
          [45 "variable.zig"]
          [54 "source.zig"]]
         (analyze-runs "var response_buffer = std.ArrayList(u8).init(allocator);"))))

(deftest invocation-test
  (is (= [[0 "keyword.control.trycatch.zig"]
          [3 "source.zig"]
          [4 "variable.zig"]
          [10 "source.zig"]
          [11 "entity.name.function.zig"]
          [19 "source.zig"]
          [20 "punctuation.definition.string.quoted.begin.zig"]
          [21 "string.quoted.double.zig"]
          [32 "constant.character.escape.zig"]
          [34 "punctuation.definition.string.quoted.end.zig"]
          [35 "source.zig"]]
         (analyze-runs "try stdOut.writeAll(\"Hello World\\n\");"))))
