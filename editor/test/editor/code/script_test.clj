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
  (:require [clojure.java.io :as io]
            [clojure.string :as string]
            [clojure.test :refer :all]
            [editor.code.data :as data]
            [editor.code.data-test :refer [layout-info]]
            [editor.code.script :as script]
            [editor.code.util :as code-util]))

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

(defn- reindent-with [indent-string tab-spaces lines]
  (let [last-row (dec (count lines))
        cursor-range (data/->CursorRange (data/->Cursor 0 0)
                                         (data/->Cursor last-row (count (lines last-row))))]
    (or (:lines (data/reindent (data/indent-level-pattern tab-spaces) indent-string script/lua-grammar
                               lines [cursor-range] nil (layout-info lines)))
        lines)))

(defn- reindent [lines]
  (reindent-with indent-string (count indent-string) lines))

(defn- reindent-rows [lines from-row to-row]
  (let [cursor-range (data/->CursorRange (data/->Cursor from-row 0)
                                         (data/->Cursor to-row (count (lines to-row))))]
    (or (:lines (data/reindent indent-level-pattern indent-string script/lua-grammar
                               lines [cursor-range] nil (layout-info lines)))
        lines)))

(deftest reindent-preserves-correct-indentation-test
  ;; Correctly indented code must come back unchanged.
  (are [lines] (= lines (reindent lines))

    ;; Callback argument closed with `end)`.
    ["function init(self)"
     "    timer.delay(1, false, function(self, handle, dt)"
     "        print(dt)"
     "    end)"
     "    print(\"after\")"
     "end"]

    ;; Table argument closed with `})`.
    ["function init(self)"
     "    local id = factory.create(pos, nil, {"
     "        scale = 2"
     "    })"
     "    print(id)"
     "end"]

    ;; Function signature split over two lines, aligned under the first parameter.
    ["function foo(a,"
     "             b)"
     "    print(a)"
     "end"]

    ["local function on_msg(self,"
     "                      message_id, message)"
     "    print(message_id)"
     "end"]

    ["repeat"
     "    print(i)"
     "until done(i)"
     "print(\"x\")"]

    ["function foo()"
     "    local id = factory.create(pos, nil, {})"
     "    print(id)"
     "end"]

    ;; An unmatched parenthesis inside a long string opens nothing.
    ["local s = [[ see the note (below ]]"
     "print(s)"]

    ;; Nor does one inside a comment or a quoted string.
    ["local text = \"(\""
     "print(text)"]

    ["print(pos) -- ("
     "print(1)"]

    ;; A block closed one bracket at a time. `end` pays off the callback and
    ;; `)` the call, so neither line may unwind both.
    ["msg.post(\"#\", \"hello\", function(self)"
     "    print(\"cb\")"
     "end"
     ")"
     "print(\"done\")"]

    ;; `else` closes a branch and opens one; `elseif` leaves the `then` to open.
    ["if a then"
     "    x()"
     "elseif b then"
     "    y()"
     "else"
     "    z()"
     "end"]

    ;; Two closers and an opener on the same line.
    ["for k in pairs({"
     "    a = 1"
     "}) do"
     "    print(k)"
     "end"]

    ;; Call arguments take a level, not a column, matching the language server.
    ["local x = foo(bar(a,"
     "    b),"
     "    c)"
     "print(x)"]

    ;; A parameter list does align, including an anonymous one.
    ["local f = function(x,"
     "                   y)"
     "    return x"
     "end"]

    ;; `end)` closes two frames opened on different lines. It lands at the level
    ;; of the line that opened the inner one, not the outer one.
    ["foo(a,"
     "    b, function()"
     "        x()"
     "    end)"
     "print(y)"]

    ;; The same, buried in nested callbacks: each `end)` unwinds to the line
    ;; that opened its own function, not to the call it is an argument of.
    ["function on_message(self, message)"
     "    if message.enter then"
     "        self.timer = timer.delay(0.3, true, function()"
     "            local bar = table.remove(self.bars)"
     "            go.animate(bar.id, \"position\", go.PLAYBACK_ONCE_FORWARD, self.target,"
     "                go.EASING_OUTSINE, 0.2, 0, function()"
     "                    msg.post(bar.body_id, \"enable\")"
     "                    timer.delay(0.6, false, function()"
     "                        audio.play(audio.DROP_ITEM)"
     "                    end)"
     "                end)"
     "        end)"
     "    end"
     "end"]

    ;; Single brackets still nest, even though doubled ones delimit strings.
    ["local x = t["
     "    key"
     "]"
     "print(x)"]))

(deftest reindent-ignores-mismatched-closers-test
  (are [lines] (= lines (reindent lines))

    ;; A closer only closes its own kind, so half-typing a name that starts with
    ;; a keyword does not throw the line out of the table it sits in.
    ["local data = {"
     "    start_position = vmath.vector3(200, 100, 0),"
     "    end"
     "}"]

    ;; `until` belongs to `repeat`, not to `do`.
    ["while a do"
     "    until"
     "end"]

    ;; A stray closer takes the indentation of any other line, and leaves the
    ;; block it is sitting in on the stack for the lines below it.
    ["function f()"
     "    }"
     "    x()"
     "end"])

  ;; Conversely, a closer that does match still dedents.
  (is (= ["local t = {"
          "    a = 1,"
          "}"]
         (reindent ["local t = {"
                    "a = 1,"
                    "        }"]))))

(deftest reindent-matches-checked-in-fixture-test
  ;; The fixture is checked in already formatted by the bundled Lua language
  ;; server, so reindenting it must change nothing. Anything else means
  ;; indent-on-type and format-on-save disagree and the user's file flips
  ;; between the two on every keystroke and every save.
  ;;
  ;; Cases the two genuinely disagree on are commented out in the fixture, each
  ;; with the reason and the shape the language server produced.
  ;;
  ;; integration.lua-indent-test re-derives the fixture from the language server
  ;; itself; this is the half that runs without it.
  (let [lines (code-util/split-lines (slurp (io/resource "lua_indent_project/indent_test_cases.lua")))
        reindented (reindent lines)]
    (is (= (count lines) (count reindented)))
    (dotimes [row (count lines)]
      (is (= (lines row) (reindented row)) (str "row " row)))))

(deftest reindent-below-long-string-test
  ;; Reindenting part of a buffer replays from the nearest unindented line above
  ;; it, which must not be a line that only looks unindented because it is inside
  ;; a long string.
  (is (= ["function f()"
          "    local s = [["
          "SELECT ("
          "]]"
          "    print(s)"
          "end"]
         (reindent-rows ["function f()"
                         "    local s = [["
                         "SELECT ("
                         "]]"
                         "print(s)"
                         "end"]
                        4 5)))

  ;; Only a bracket of the same level ends one, so the [[ here is still string.
  (is (= ["function f()"
          "    local s = [=["
          "[[ x"
          "]=]"
          "    print(s)"
          "end"]
         (reindent-rows ["function f()"
                         "    local s = [=["
                         "[[ x"
                         "]=]"
                         "print(s)"
                         "end"]
                        4 5)))

  ;; Long strings do not nest, so a same-level opener inside one is just text.
  ;; Scanning backward stops at the real opener, never at this.
  (is (= ["function f()"
          "    local s = [["
          "[[ not a nested string"
          "]]"
          "    print(s)"
          "end"]
         (reindent-rows ["function f()"
                         "    local s = [["
                         "[[ not a nested string"
                         "]]"
                         "print(s)"
                         "end"]
                        4 5)))

  ;; The replay can start inside the string, where an unindented line of prose
  ;; is no more a top-level statement than one that looks like code.
  (is (= ["function f()"
          "    local s = [["
          "just some prose"
          "]]"
          "    print(s)"
          "end"]
         (reindent-rows ["function f()"
                         "    local s = [["
                         "just some prose"
                         "]]"
                         "print(s)"
                         "end"]
                        3 4))))

(deftest insert-indentation-below-long-string-test
  ;; Pressing Enter fixes the row the newline was typed on as well as the new
  ;; one, so the replay starts a row further up -- inside the string.
  (are [inserted-lines before after]
    (= (into [] xform-test-lines->lines after)
       (:lines (insert-lines (into [] xform-test-lines->lines before)
                             (into [] xform-test-lines->cursor-ranges before)
                             inserted-lines)))

    [""
     ""]
    ["function f()"
     "    local s = [["
     "just some prose"
     "]]|"]
    ["function f()"
     "    local s = [["
     "just some prose"
     "]]"
     "    |"]

    [""
     ""]
    ["function f()"
     "    local s = [["
     "[[ not a nested string"
     "]]|"]
    ["function f()"
     "    local s = [["
     "[[ not a nested string"
     "]]"
     "    |"]))

(deftest reindent-tab-alignment-test
  ;; An alignment column is where the text is drawn, so tabs both in the
  ;; indentation and inside the line count for their full width.
  (are [lines] (= lines (reindent-with "\t" 4 lines))

    ;; Column 13: three tabs reach column 12, a space covers the rest.
    ["function foo(a,"
     "\t\t\t b)"
     "\tprint(a)"
     "end"]

    ["function f()"
     "\tif a then"
     "\t\tlocal g = function(x,"
     "\t\t\t\t\t\t   y)"
     "\t\t\treturn x"
     "\t\tend"
     "\tend"
     "end"]

    ;; A tab inside the line moves the parenthesis to column 21.
    ["local g =\tfunction(x,"
     "\t\t\t\t\t y)"
     "\treturn x"
     "end"])

  ;; The same line indented with spaces aligns to the same column.
  (is (= ["local g =\tfunction(x,"
          "                     y)"
          "    return x"
          "end"]
         (reindent-with "    " 4 ["local g =\tfunction(x,"
                                  "y)"
                                  "return x"
                                  "end"]))))

(deftest reindent-long-string-test
  ;; Nothing between [[ and ]] is code, however many lines it spans.
  (are [lines] (= lines (reindent lines))

    ["function f()"
     "    local s = [[ see the note (below"
     "    this is a test thing("
     "    ]]"
     "    print(s)"
     "end"]

    ;; Same for a block comment.
    ["function f()"
     "    local s = --[[ see the note"
     "    this is a test thing("
     "    ]]"
     "    print(s)"
     "end"]

    ;; Whitespace inside one is part of the string, so it is left alone.
    ["local sql = [["
     "SELECT *"
     "  FROM t"
     "]]"
     "print(sql)"]

    ;; A block keyword inside one closes nothing.
    ["function f()"
     "    local s = [[ note"
     "    end"
     "    ]]"
     "    print(s)"
     "end"]))

(deftest reindent-equals-delimited-long-brackets-test
  ;; Whitespace and code-like text inside equals-delimited long strings and
  ;; block comments must not participate in indentation.
  (are [lines] (= lines (reindent lines))
    ["function f()"
     "    local s = [=[ note"
     "  end("
     "]=]"
     "    print(s)"
     "end"]

    ["function f()"
     "    --[==[ note"
     " end("
     "]==]"
     "    print('done')"
     "end"]))

(deftest reindent-parentheses-test
  (are [expected lines] (= expected (reindent lines))

    ;; A call closed on a later line does not indent what follows it.
    ["local pos = vmath.vector3("
     "    1, 2, 3)"
     "print(pos)"]
    ["local pos = vmath.vector3("
     "1, 2, 3)"
     "        print(pos)"]

    ;; Arguments continued after a trailing comma take a level.
    ["function update(self, dt)"
     "    local ax, ay = steering.combine_axes(dx, dy,"
     "        self.gamepad_axis.x, self.gamepad_axis.y)"
     "    print(ax)"
     "end"]
    ["function update(self, dt)"
     "local ax, ay = steering.combine_axes(dx, dy,"
     "self.gamepad_axis.x, self.gamepad_axis.y)"
     "print(ax)"
     "end"]

    ;; A closing parenthesis alone on its line.
    ["local x = foo("
     "    1, 2"
     ")"
     "print(x)"]
    ["local x = foo("
     "1, 2"
     ")"
     "        print(x)"]

    ;; Nested calls closed on the same line unwind both levels.
    ["function foo()"
     "    local x = math.max("
     "        1, math.min("
     "            2, 3))"
     "    print(x)"
     "end"]
    ["function foo()"
     "local x = math.max("
     "1, math.min("
     "2, 3))"
     "print(x)"
     "end"]

    ;; Condition split over two lines, block keyword on the closing line.
    ["if check(a,"
     "    b) then"
     "    print(a)"
     "end"]
    ["if check(a,"
     "b) then"
     "print(a)"
     "end"]

    ;; A parenthesis opened and closed on the same line changes nothing.
    ["local pos = vmath.vector3(1, 2, 3)"
     "print(pos)"]
    ["local pos = vmath.vector3(1, 2, 3)"
     "        print(pos)"]

    ;; The alignment column is read off the line as it will look after the fix,
    ;; not as it looks now.
    ["if a then"
     "    local f = function(x,"
     "                       y)"
     "        return x"
     "    end"
     "end"]
    ["if a then"
     "local f = function(x,"
     "y)"
     "return x"
     "end"
     "end"]

    ;; A trailing comment after the closing parenthesis is ignored.
    ["local pos = vmath.vector3("
     "    1, 2, 3) -- close call"
     "print(pos)"]
    ["local pos = vmath.vector3("
     "1, 2, 3) -- close call"
     "        print(pos)"]))

(deftest insert-indentation-parentheses-test
  ;; Pressing enter after these lines.
  (are [before after]
    (= (into [] xform-test-lines->lines after)
       (:lines (insert-lines (into [] xform-test-lines->lines before)
                             (into [] xform-test-lines->cursor-ranges before)
                             ["" ""])))

    ["local pos = vmath.vector3("
     "    1, 2, 3)|"]
    ["local pos = vmath.vector3("
     "    1, 2, 3)"
     "|"]

    ["if pos.x > 0 then"
     "    local pos = vmath.vector3("
     "        1, 2, 3)|"]
    ["if pos.x > 0 then"
     "    local pos = vmath.vector3("
     "        1, 2, 3)"
     "    |"]

    ;; A line ending in an open parenthesis indents the next line.
    ["local pos = vmath.vector3(|"]
    ["local pos = vmath.vector3("
     "    |"]

    ;; ...but a balanced one does not.
    ["local pos = vmath.vector3(1, 2, 3)|"]
    ["local pos = vmath.vector3(1, 2, 3)"
     "|"]

    ;; A callback closed with `end)` dedents once, not twice.
    ["function init(self)"
     "    timer.delay(1, false, function(self, handle, dt)"
     "        print(dt)"
     "    end)|"]
    ["function init(self)"
     "    timer.delay(1, false, function(self, handle, dt)"
     "        print(dt)"
     "    end)"
     "    |"]

    ;; Same for a table argument closed with `})`.
    ["function init(self)"
     "    local id = factory.create(pos, nil, {"
     "        scale = 2"
     "    })|"]
    ["function init(self)"
     "    local id = factory.create(pos, nil, {"
     "        scale = 2"
     "    })"
     "    |"]

    ;; The body of a function whose signature spans two lines. The parameters
    ;; align under the first one, the body does not.
    ["function foo(a,"
     "             b)|"]
    ["function foo(a,"
     "             b)"
     "    |"]

    ;; Parentheses in strings and comments are not counted.
    ["local text = \"(\"|"]
    ["local text = \"(\""
     "|"]

    ["print(pos) -- )|"]
    ["print(pos) -- )"
     "|"]

    ["local pos = vmath.vector3("
     "    1, 2, 3) -- close call|"]
    ["local pos = vmath.vector3("
     "    1, 2, 3) -- close call"
     "|"]))
