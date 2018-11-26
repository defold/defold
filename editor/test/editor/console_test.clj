(ns editor.console-test
  (:require [clojure.test :refer :all]
            [editor.console :as console]))

(def ^:private line-sub-regions-pattern (var-get #'console/line-sub-regions-pattern))

(deftest line-sub-regions-pattern-test
  (are [line matches]
    (= matches (next (re-find line-sub-regions-pattern line)))

    "/main.lua"            ["/main.lua" nil]
    "/main.lua:12"         ["/main.lua" "12"]
    "/dir/main.lua"        ["/dir/main.lua" nil]
    "/dir/main.lua:12"     ["/dir/main.lua" "12"]

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

(defn- on-region-click! [_region]
  nil)

(defn- resource-reference-region
  ([row col text resource-proj-path]
   (#'console/make-resource-reference-region row col (+ col (count text)) resource-proj-path on-region-click!))
  ([row col text resource-proj-path resource-row]
   (#'console/make-resource-reference-region row col (+ col (count text)) resource-proj-path resource-row on-region-click!)))

(def ^:private resource-map {"/main.lua" 100
                             "/dir/main.lua" 200})

(defn- append-lines [props lines]
  ;; Entries are [type line] pairs. We use nil for untyped regular entries.
  (let [entries (mapv (partial vector nil) lines)]
    (#'console/append-entries props entries resource-map on-region-click!)))

(deftest append-entries-test
  (is (= {:lines ["/main.lua"]
          :regions [(resource-reference-region 0 0 "/main.lua" "/main.lua")]}
         (append-lines {:lines [""]
                        :regions []}
                       ["/main.lua"])))
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
