;; Copyright 2020-2022 The Defold Foundation
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

(ns editor.console-test
  (:require [clojure.test :refer :all]
            [editor.console :as console]))

(def ^:private line-sub-regions-pattern (var-get #'console/line-sub-regions-pattern))
(def ^:private line-sub-regions-pattern-partial (var-get #'console/line-sub-regions-pattern-partial))

(deftest line-sub-regions-pattern-test
  (are [line matches]
    (= matches (next (re-find line-sub-regions-pattern line)))

    "/main.lua"            ["/main.lua" nil]
    "/main.lua:"           ["/main.lua" nil]
    "/main.lua:12"         ["/main.lua" "12"]
    "/dir/main.lua"        ["/dir/main.lua" nil]
    "/dir/main.lua:"       ["/dir/main.lua" nil]
    "/dir/main.lua:12"     ["/dir/main.lua" "12"]

    " /main.lua "          ["/main.lua" nil]
    " /main.lua: "         ["/main.lua" nil]
    " /main.lua:12 "       ["/main.lua" "12"]
    " /dir/main.lua "      ["/dir/main.lua" nil]
    " /dir/main.lua: "     ["/dir/main.lua" nil]
    " /dir/main.lua:12 "   ["/dir/main.lua" "12"]

    "</main.lua>"          ["/main.lua" nil]
    "</main.lua:12>"       ["/main.lua" "12"]
    "</dir/main.lua>"      ["/dir/main.lua" nil]
    "</dir/main.lua:12>"   ["/dir/main.lua" "12"]

    "'/main.lua'"          ["/main.lua" nil]
    "'/main.lua:12'"       ["/main.lua" "12"]
    "'/dir/main.lua'"      ["/dir/main.lua" nil]
    "'/dir/main.lua:12'"   ["/dir/main.lua" "12"]

    "`/main.lua`"          ["/main.lua" nil]
    "`/main.lua:12`"       ["/main.lua" "12"]
    "`/dir/main.lua`"      ["/dir/main.lua" nil]
    "`/dir/main.lua:12`"   ["/dir/main.lua" "12"]

    "\"/main.lua\""        ["/main.lua" nil]
    "\"/main.lua:12\""     ["/main.lua" "12"]
    "\"/dir/main.lua\""    ["/dir/main.lua" nil]
    "\"/dir/main.lua:12\"" ["/dir/main.lua" "12"]))

(deftest line-sub-regions-partial-pattern-test
  (are [line matches]
    (= matches (next (re-find line-sub-regions-pattern-partial line)))

    "ERROR:SCRIPT: e_name_is_quite_long_how_will_you_deal_with_that_huh.script:2: attempt to call field 'balooba' (a nil value)" ["e_name_is_quite_long_how_will_you_deal_with_that_huh.script" "2"]))

(defn- on-region-click! [_region _event]
  nil)

(defn- resource-reference-region
  ([row col text resource-proj-path]
   (#'console/make-resource-reference-region row col (+ col (count text)) [resource-proj-path] on-region-click!))
  ([row col text resource-proj-path resource-row]
   (#'console/make-resource-reference-region row col (+ col (count text)) [resource-proj-path] resource-row on-region-click!)))

(defn- whole-line-region [type row text]
  (#'console/make-whole-line-region type row text))

(def ^:private resource-map {"/main.lua" 100
                             "/dir/main.lua" 200
                             "/module.lua" 300
                             "/main/yes_this_is_untitled_and_the_file_name_is_quite_long_how_will_you_deal_with_that_huh.script" 400})

(defn- append-entries [props entries]
  (#'console/append-entries props entries resource-map on-region-click!))

(defn- append-lines [props lines]
  ;; Entries are [type line] pairs. We use nil for untyped regular entries.
  (let [entries (mapv (partial vector nil) lines)]
    (append-entries props entries)))

(deftest append-entries-test
  (is (= {:lines ["123"]
          :regions [(whole-line-region :eval-result 0 "123")]
          :invalidated-row 0}
         (append-entries {:lines [""]
                          :regions []}
                         [[:eval-result "123"]])))
  (is (= {:lines ["/main.lua"]
          :regions [(resource-reference-region 0 0 "/main.lua" "/main.lua")]
          :invalidated-row 0}
         (append-lines {:lines [""]
                        :regions []}
                       ["/main.lua"])))
  (is (= {:lines ["/main.lua"
                  "/module.lua"]
          :regions [(resource-reference-region 0 0 "/main.lua" "/main.lua")
                    (resource-reference-region 1 0 "/module.lua" "/module.lua")]
          :invalidated-row 0}
         (append-lines {:lines [""]
                        :regions []}
                       ["/main.lua"
                        "/module.lua"])))
  (is (= {:lines ["/non-existing-resource.lua"]
          :regions []
          :invalidated-row 0}
         (append-lines {:lines [""]
                        :regions []}
                       ["/non-existing-resource.lua"])))
  (is (= {:lines ["Syntax error"
                  "  from </dir/main.lua:33>"]
          :regions [(resource-reference-region 1 (count "  from <") "/dir/main.lua:33" "/dir/main.lua" 32)]}
         (append-lines {:lines ["Syntax error"]
                        :regions []}
                       ["  from </dir/main.lua:33>"])))
  (is (= {:lines ["Syntax error"
                  "  from </dir/main.lua:33>"
                  "   via '/main.lua:8', '/main.lua:13'"]
          :regions [(resource-reference-region 1 (count "  from <") "/dir/main.lua:33" "/dir/main.lua" 32)
                    (resource-reference-region 2 (count "   via '") "/main.lua:8" "/main.lua" 7)
                    (resource-reference-region 2 (count "   via '/main.lua:8', '") "/main.lua:13" "/main.lua" 12)]}
         (append-lines {:lines ["Syntax error"
                                "  from </dir/main.lua:33>"]
                        :regions [(resource-reference-region 1 (count "  from <") "/dir/main.lua:33" "/dir/main.lua" 32)]}
                       ["   via '/main.lua:8', '/main.lua:13'"]))))

(deftest three-matches-on-same-line-where-two-are-partial
  (is (= 3 (count
             (#'console/make-line-sub-regions {"/absolute/project/path/file.lua" :WIN
                                               "/some/really/long/path/to/some/resource/in/the/project/the_resource.script" :WIN}
                                              identity 10
                                              "urce/in/the/project/the_resource.script:25: in function <urce/in/the/project/the_resource.script:24> </absolute/project/path/file.lua:65>")))))
