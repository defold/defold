(ns editor.code-view-ux-syntax-test
  (:require [clojure.test :refer :all]
            [clojure.java.io :as io]
            [dynamo.graph :as g]
            [editor.code-view :as cv :refer :all]
            [editor.code-view-ux :as cvx :refer :all]
            [editor.code-completion :as code-completion]
            [editor.handler :as handler]
            [editor.lua :as lua]
            [editor.script :as script]
            [editor.gl.shader :as shader]
            [editor.code-view-test :as cvt :refer [setup-code-view-nodes load-buffer expect-buffer buffer-commands should-be]]
            [support.test-support :refer [with-clean-system tx-nodes]]
            [integration.test-util :as test-util]
            [clojure.string :as string])
  (:import [com.sun.javafx.tk Toolkit]
           [javafx.scene.input KeyEvent KeyCode]))


;; ----------------------------------------
;; Simulate commands

(defn- ->context [source-viewer]
  {:name :code-view :env {:source-viewer source-viewer}})

(defn- enter! [source-viewer]
  (cvx/handler-run :enter [(->context source-viewer)] {}))

(defn- tab! [source-viewer]
  (cvx/handler-run :tab [(->context source-viewer)] {}))

(defn- shift-tab! [source-viewer]
  (cvx/handler-run :backwards-tab-trigger [(->context source-viewer)] {}))

(defn- key-typed! [source-viewer key-typed]
  (cvx/handler-run :key-typed [(->context source-viewer)] {:key-typed key-typed
                                                           :key-event (KeyEvent. KeyEvent/KEY_TYPED
                                                                                 key-typed
                                                                                 ""
                                                                                 (KeyCode/getKeyCode key-typed)
                                                                                 false
                                                                                 false
                                                                                 false
                                                                                 false)}))

(defn- toggle-comment! [source-viewer]
  (cvx/handler-run :toggle-comment [(->context source-viewer)] {}))

(defn- propose! [source-viewer]
  (cvx/handler-run :proposals [(->context source-viewer)] {}))

(defn- accept-proposal! [source-viewer insert-str]
  (do-proposal-replacement source-viewer {:insert-string insert-str}))

(defn- indent! [source-viewer]
  (cvx/handler-run :reindent [(->context source-viewer)]{}))


;; ----------------------------------------
;; Test comment/uncomment

(defn do-toggle-line-comment [node-type opts comment-str]
  (with-clean-system
    (buffer-commands (load-buffer world node-type opts)
                     (should-be "hello world|")

                     (toggle-comment!)
                     (should-be (str comment-str "hello world|"))

                     (toggle-comment!)
                     (should-be "hello world|"))))

(deftest toggle-comment-line-test
  (testing "lua syntax"
    (do-toggle-line-comment script/ScriptNode lua/lua "-- "))
  (testing "glsl syntax"
    (do-toggle-line-comment shader/ShaderNode (:code shader/glsl-opts) "// ")))

(defn do-toggle-region-comment [node-type opts comment-str]
  (with-clean-system
    (testing "toggle region comment selecting down"
      (buffer-commands (load-buffer world node-type opts)
                       (should-be "line1"
                                  "li>ne2"
                                  "line|3"
                                  "line4")

                       (toggle-comment!)
                       (should-be "line1"
                                  (str comment-str "li>ne2")
                                  (str comment-str "line|3")
                                  "line4")

                       (toggle-comment!)
                       (should-be "line1"
                                  "li>ne2"
                                  "line|3"
                                  "line4")))

    (testing "toggle region comment selecting up"
      (buffer-commands (load-buffer world node-type opts)
                       (should-be "l|ine1"
                                  "line2"
                                  "line3"
                                  "line4<")

                       (toggle-comment!)
                       (should-be (str comment-str "l|ine1")
                                  (str comment-str "line2")
                                  (str comment-str "line3")
                                  (str comment-str "line4<"))

                       (toggle-comment!)
                       (should-be "l|ine1"
                                  "line2"
                                  "line3"
                                  "line4<")))))

(deftest toggle-comment-region-test
  (testing "lua syntax"
    (do-toggle-region-comment script/ScriptNode lua/lua "-- "))
  (testing "glsl syntax"
    (do-toggle-region-comment shader/ShaderNode (:code shader/glsl-opts) "// ")))

;; ----------------------------------------
;; Test autocomplete

(defn- should-propose
  [source-viewer expectations]
  (is (= expectations (map #(vector (:name %) (:display-string %) (:insert-string %)) (propose source-viewer)))))

;; We want keywords as part of completions in the new text editor, but that breaks these tests for the old.
;; This alternate lua def strips out the keywords again.
(def no-keyword-assist-lua (assoc lua/lua
                                  :assist (fn [completions ^String text offset ^String line]
                                            (let [filtered ((:assist lua/lua) completions text offset line)]
                                              (remove #(= (:type %) :keyword) filtered)))))

(defn- completions? [init expect]
  (with-clean-system
    (let [viewer (first (load-buffer world script/ScriptNode no-keyword-assist-lua init))]
      (should-propose viewer expect))))

(deftest lua-completions-propose-test
  (are [init completions] (completions? init completions)
    "asser|"                                        [["assert"        "assert(v, [message])"     "assert(v)"]]
    "if|"                                           [["if"
                                                      "if                                if cond then"
                                                      "if cond then\n\t-- do things\nend"]]
    "go|"                                           [["go"            "go"                       "go"]]
    "vm|"                                           [["vmath"         "vmath"                    "vmath"]]
    "mat|"                                          [["math"          "math"                     "math"]]
    "go.proper|"                                    [["go.property"   "go.property(name, value)" "go.property(name, value)"]]
    "go.delete|"                                    [["go.delete"     "go.delete([id])"          "go.delete()"]
                                                     ["go.delete_all" "go.delete_all([ids])"     "go.delete_all()"]]
    "local foo=1 \n foo|"                           [["foo"           "foo"                      "foo"]]
    "bar=1 \n ba|"                                  [["bar"           "bar"                      "bar"]]
    "local function cake(x, y) return x end\n cak|" [["cake"          "cake(x, y)"               "cake(x, y)"]]
    "function ice_cream(x, y) return x end\n ice|"  [["ice_cream"     "ice_cream(x, y)"          "ice_cream(x, y)"]]
    "self.velocity = vm|"                           [["vmath"         "vmath"                    "vmath"]]
    "go.propert| = vm"                              [["go.property"   "go.property(name, value)" "go.property(name, value)"]]))

(deftest test-do-proposal-replacement
  (with-clean-system
    (are [init accept expect] (buffer-commands (load-buffer world script/ScriptNode lua/lua)
                                               init
                                               (accept-proposal! accept)
                                               expect)
      "foo|"                   "food_glorious_food" "food_glorious_food|"
      "go.set|"                "go.set_property()"  "go.set_property()|"
      "  go.set|"              "go.set_property()"  "go.set_property()|"
      "  assert(math.a|"       "math.abs()"         "assert(math.abs()|"
      "    go.set_prop = vma|" "vmath"              "go.set_prop = vmath|"
      "vmat| = go.set"         "vmath"              "vmath| = go.set"
      "vmatches = vm|"         "vmath"              "vmatches = vmath|")))


(deftest test-single-value-proposals
  (with-clean-system
    (are [init expect] (buffer-commands (load-buffer world script/ScriptNode no-keyword-assist-lua)
                                        init
                                        (propose!)
                                        expect)
      "math.ab|"                "math.abs(>x|)"
      "vmat|"                   "vmath|"
      "function foo(x)\n\tif|"  "function foo(x)\n\tif >cond| then\n\t\t-- do things\n\tend")))

(deftest test-proposal-tab-triggers
  (with-clean-system
    (testing "single arg"
      (buffer-commands (load-buffer world script/ScriptNode lua/lua)
                       "math.ab|"

                       (propose!)
                       "math.abs(>x|)"

                       (tab!)
                       "math.abs(x)|"))
    (testing "multiple arg"
      (buffer-commands (load-buffer world script/ScriptNode lua/lua)
                       "string.sub|"

                       (propose!)
                       "string.sub(>s|, i)"

                       (tab!)
                       "string.sub(s, >i|)"

                       (tab!)
                       "string.sub(s, i)|"))
    (testing "no args"
      (buffer-commands (load-buffer world script/ScriptNode lua/lua)
                       "go.delete_all|"

                       (propose!)
                       "go.delete_all()|"))
    (testing "with typing for arg values"
      (buffer-commands (load-buffer world script/ScriptNode lua/lua)
                       "string.sub|"

                       (propose!)
                       "string.sub(>s|, i)"

                       (key-typed! "1")
                       "string.sub(1|, i)"

                       (tab!)
                       "string.sub(1, >i|)"

                       (key-typed! "2")
                       "string.sub(1, 2|)"

                       (tab!)
                       "string.sub(1, 2)|"))
    (testing "with if template"
      (buffer-commands (load-buffer world script/ScriptNode no-keyword-assist-lua)
                       "if|"

                       (propose!)
                       (should-be "if >cond| then"
                                  "\t-- do things"
                                  "end")

                       (tab!)
                       (should-be "if cond then"
                                  "\t>-- do things|"
                                  "end")

                       (tab!)
                       (should-be "if cond then"
                                  "\t-- do things"
                                  ">end|")

                       (tab!)
                       (should-be "if cond then"
                                  "\t-- do things"
                                  "end|")))
    (testing "with function template"
      (buffer-commands (load-buffer world script/ScriptNode no-keyword-assist-lua)
                       "function|"

                       (propose!)
                       (should-be "function >function_name|(self)"
                                  "\t-- do things"
                                  "end")

                       (tab!)
                       (should-be "function function_name(>self|)"
                                  "\t-- do things"
                                  "end")

                       (tab!)
                       (should-be "function function_name(self)"
                                  "\t>-- do things|"
                                  "end")

                       (tab!)
                       (should-be "function function_name(self)"
                                  "\t-- do things"
                                  "end|")))
    (testing "shift tab backwards"
      (buffer-commands (load-buffer world script/ScriptNode no-keyword-assist-lua)
                       "if|"

                       (propose!)
                       (should-be "if >cond| then"
                                  "\t-- do things"
                                  "end")

                       (tab!)
                       (should-be "if cond then"
                                  "\t>-- do things|"
                                  "end")

                       (tab!)
                       (should-be "if cond then"
                                  "\t-- do things"
                                  ">end|")

                       (shift-tab!)
                       (should-be "if cond then"
                                  "\t>-- do things|"
                                  "end")

                       (shift-tab!)
                       (should-be "if >cond| then"
                                  "\t-- do things"
                                  "end")

                       (shift-tab!)
                       (should-be "if >cond| then"
                                  "\t-- do things"
                                  "end")

                       (tab!)
                       (should-be "if cond then"
                                  "\t>-- do things|"
                                  "end")))))

;; ----------------------------------------
;; Test autoindentation

(deftest lua-enter-indentation-functions
  (with-clean-system
    (testing "indentation for functions"
      (testing "no indentation level"
        (buffer-commands (load-buffer world script/ScriptNode lua/lua)
                         (should-be "function test(x)|")

                         (enter!)
                         (should-be "function test(x)"
                                    "\t|")))
      (testing "some indentation exists"
        (buffer-commands (load-buffer world script/ScriptNode lua/lua)
                         (should-be "if true then"
                                    "\tfunction test(x)|")

                         (enter!)
                         (should-be "if true then"
                                    "\tfunction test(x)"
                                    "\t\t|")))
      (testing "maintains level in the function"
        (buffer-commands (load-buffer world script/ScriptNode lua/lua)
                         (should-be "function test(x)\n\tfoo|")

                         (enter!)
                         (should-be "function test(x)"
                                    "\tfoo"
                                    "\t|")))
      (testing "end deindents"
        (buffer-commands (load-buffer world script/ScriptNode lua/lua)
                         (should-be "function test(x)\n\tend|")

                         (enter!)
                         (should-be  "function test(x)"
                                     "end"
                                     "|")))
      (testing "nested function immedate end deindents"
        (buffer-commands (load-buffer world script/ScriptNode lua/lua)
                         (should-be "function test(x)"
                                    "\tfunction nested(y)"
                                    "\t\tend|")
                         (enter!)
                         (should-be "function test(x)"
                                    "\tfunction nested(y)"
                                    "\tend"
                                    "\t|")))
      (testing "enter after function signature containing end still indents"
        (buffer-commands (load-buffer world script/ScriptNode lua/lua)
                         (should-be "function foo(sender)|")
                         (enter!)
                         (should-be "function foo(sender)"
                                    "\t|")))
      (testing "no enter identation if not at an end of the line"
        (buffer-commands (load-buffer world script/ScriptNode lua/lua)
                         (should-be "fu|nction test(x)")

                         (enter!)
                         (should-be  "fu"
                                     "|nction test(x)"))))))

(deftest lua-enter-indentation-if-else
  (with-clean-system
    (testing "indentation for if else"
      (testing "if indents"
        (buffer-commands (load-buffer world script/ScriptNode lua/lua)
                         (should-be "if true|")

                         (enter!)
                         (should-be "if true"
                                    "\t|")))
      (testing "elseif dedents and indents the next level"
        (buffer-commands (load-buffer world script/ScriptNode lua/lua)
                         (should-be "if true"
                                    "\telseif|")

                         (enter!)
                         (should-be "if true"
                                    "elseif"
                                    "\t|")))
      (testing "else dedents and indents to the next level"
        (buffer-commands (load-buffer world script/ScriptNode lua/lua)
                         (should-be "if true"
                                    "\telse|")

                         (enter!)
                         (should-be "if true"
                                    "else"
                                    "\t|")))
      (testing "end dedents"
        (buffer-commands (load-buffer world script/ScriptNode lua/lua)
                         (should-be "if true"
                                    "\tend|")

                         (enter!)
                         (should-be "if true"
                                    "end"
                                    "|"))))))

(deftest lua-enter-indentation-while
  (with-clean-system
    (testing "indentation for while"
      (testing "while indents"
        (buffer-commands (load-buffer world script/ScriptNode lua/lua)
                         (should-be "while true|")

                         (enter!)
                         (should-be "while true"
                                    "\t|")))
      (testing "end dedents"
        (buffer-commands (load-buffer world script/ScriptNode lua/lua)
                         (should-be "while true"
                                    "\tend|")

                         (enter!)
                         (should-be "while true"
                                    "end"
                                    "|"))))))

(deftest lua-enter-indentation-tables
  (with-clean-system
    (testing "indentation for tables"
      (testing "{ indents"
        (buffer-commands (load-buffer world script/ScriptNode lua/lua)
                         (should-be "x = {|")

                         (enter!)
                         (should-be "x = {"
                                    "\t|")))
      (testing "} dedents"
        (buffer-commands (load-buffer world script/ScriptNode lua/lua)
                         (should-be "x = {"
                                    "\t1"
                                    "\t}|")

                         (enter!)
                         (should-be "x = {"
                                    "\t1"
                                    "}"
                                    "|"))))))

(deftest lua-enter-indent-line-and-region
  (with-clean-system
    (testing "indent line"
      (buffer-commands (load-buffer world script/ScriptNode lua/lua)
                       (should-be "if true then"
                                  "return 1|")

                       (indent!)
                       (should-be "if true then"
                                  "\treturn 1|")))

    (testing "indent region (selecting down)"
      (buffer-commands (load-buffer world script/ScriptNode lua/lua)
                       (should-be ">if true then"
                                  "return 1"
                                  "if false then"
                                  "return 2"
                                  "end"
                                  "end|")

                       (indent!)
                       (should-be ">if true then"
                                  "\treturn 1"
                                  "\tif false then"
                                  "\t\treturn 2"
                                  "\tend"
                                  "end|")))

    (testing "indent region (selecting up)"
      (buffer-commands (load-buffer world script/ScriptNode lua/lua)
                       (should-be "|if true then"
                                  "return 1"
                                  "if false then"
                                  "return 2"
                                  "end"
                                  "end<")

                       (indent!)
                       (should-be "|if true then"
                                  "\treturn 1"
                                  "\tif false then"
                                  "\t\treturn 2"
                                  "\tend"
                                  "end<")))))
