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

(ns editor.resource-dialog-test
  (:require [clojure.test :refer :all]
            [editor.resource :as resource]
            [editor.resource-dialog]))

(set! *warn-on-reflection* true)

;; Access the private compile-exclude-pred via var reference.
(def ^:private compile-exclude-pred @#'editor.resource-dialog/compile-exclude-pred)

(defn- fake-resource [path-str]
  (reify resource/Resource
    (proj-path [_] path-str)))

(defn- shown?
  "Returns true if the resource with the given proj-path passes the predicate.
  A nil predicate means no filtering — everything passes."
  [pred path-str]
  (let [r (fake-resource path-str)]
    (if (nil? pred) true (pred r))))

(deftest compile-exclude-pred-test

  (testing "nil patterns returns nil (no filtering)"
    (is (nil? (compile-exclude-pred nil))))

  (testing "empty patterns returns nil (no filtering)"
    (is (nil? (compile-exclude-pred []))))

  (testing "all disabled patterns returns nil (no filtering)"
    (is (nil? (compile-exclude-pred [["main" false] ["scripts" false]]))))

  (testing "single enabled pattern hides matching paths"
    (let [pred (compile-exclude-pred [["main" true]])]
      (is (some? pred))
      (is (false? (shown? pred "/main/game.go")))
      (is (false? (shown? pred "/main/sub/file.lua")))
      (is (true?  (shown? pred "/scripts/player.script")))
      (is (true?  (shown? pred "/other/player.go")))))

  (testing "disabled pattern shows everything"
    (let [pred (compile-exclude-pred [["main" false]])]
      (is (nil? pred))
      (is (true? (shown? pred "/main/game.go")))))

  (testing "multiple enabled patterns hide resources matching any pattern"
    (let [pred (compile-exclude-pred [["scripts" true] ["modules" true]])]
      (is (false? (shown? pred "/scripts/player.script")))
      (is (false? (shown? pred "/modules/colors.lua")))
      (is (true?  (shown? pred "/game_objects/player.go")))))

  (testing "mix of enabled and disabled: only enabled patterns filter"
    (let [pred (compile-exclude-pred [["scripts" true] ["modules" false]])]
      (is (false? (shown? pred "/scripts/player.script")))
      (is (true?  (shown? pred "/modules/colors.lua")))
      (is (true?  (shown? pred "/game_objects/player.go")))))

  (testing "pattern with no matching paths shows everything"
    (let [pred (compile-exclude-pred [["zzznomatch" true]])]
      (is (some? pred))
      (is (true? (shown? pred "/main/game.go")))
      (is (true? (shown? pred "/scripts/player.script")))))

  (testing "empty string pattern is ignored (seq check)"
    (is (nil? (compile-exclude-pred [["" true]]))))

  (testing "single enabled pattern returns a simple fn, not every-pred"
    ;; Regression: ensure (case 1 (first preds)) path works correctly.
    (let [pred (compile-exclude-pred [["scripts" true]])]
      (is (fn? pred))
      (is (false? (shown? pred "/scripts/foo.lua")))
      (is (true?  (shown? pred "/main/foo.lua")))))

  (testing "three enabled patterns all apply"
    (let [pred (compile-exclude-pred [["scripts" true] ["modules" true] ["game_objects" true]])]
      (is (false? (shown? pred "/scripts/foo.lua")))
      (is (false? (shown? pred "/modules/colors.lua")))
      (is (false? (shown? pred "/game_objects/player.go")))
      (is (true?  (shown? pred "/main/game.go")))))

  (testing "pattern matches whole path segments, not arbitrary substrings"
    ;; Regression: a pattern like "test" used to match via string/includes?,
    ;; which also matched unrelated names that merely contain it as a
    ;; substring, such as "latest_scores.lua" or a "/testing" directory.
    (let [pred (compile-exclude-pred [["test" true]])]
      (is (false? (shown? pred "/test/foo.lua")) "excludes files inside a /test directory")
      (is (false? (shown? pred "/main/test/foo.lua")) "excludes a /test directory nested deeper in the tree")
      (is (true?  (shown? pred "/latest_scores.lua")) "does not exclude a filename that merely contains \"test\"")
      (is (true?  (shown? pred "/testing/foo.lua")) "does not exclude a differently-named directory that starts with \"test\"")
      (is (true?  (shown? pred "/main/attest.lua")) "does not exclude a filename that merely contains \"test\"")))

  (testing "multi-segment pattern matches a nested directory path"
    (let [pred (compile-exclude-pred [["main/scripts" true]])]
      (is (false? (shown? pred "/main/scripts/player.script")))
      (is (true?  (shown? pred "/main/game.go")))
      (is (true?  (shown? pred "/scripts/player.script")) "the segments must be contiguous and in order"))))
