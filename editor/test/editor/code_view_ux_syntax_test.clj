(ns editor.code-view-ux-syntax-test
  (:require [clojure.test :refer :all]
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
            [clojure.string :as str]))

;; ----------------------------------------
;; Simulate commands

(defn- enter! [source-viewer]
  (cvx/handler-run :enter [{:name :code-view :env {:selection source-viewer}}] {}))

(defn- tab! [source-viewer]
  (cvx/handler-run :tab [{:name :code-view :env {:selection source-viewer}}] {}))

(defn- shift-tab! [source-viewer]
  (cvx/handler-run :backwards-tab-trigger [{:name :code-view :env {:selection source-viewer}}] {}))

(defn- key-typed! [source-viewer key-typed]
  (cvx/handler-run :key-typed [{:name :code-view :env {:selection source-viewer :key-typed key-typed}}] {}))

(defn- toggle-comment! [source-viewer]
  (cvx/handler-run :toggle-comment [{:name :code-view :env {:selection source-viewer}}] {}))

(defn- propose! [source-viewer]
  (cvx/handler-run :proposals [{:name :code-view :env {:selection source-viewer}}] {}))

(defn- indent! [source-viewer]
  (cvx/handler-run :indent [{:name :code-view :env {:selection source-viewer}}]{}))


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

(defn- set-code-and-caret! [source-viewer code]
  (text! source-viewer code)
  (caret! source-viewer (count code) false)
  (changes! source-viewer))

(defn- should-propose
  [source-viewer expectations]
  (loop [result       (propose source-viewer)
         expectations expectations]
    (when (or (first result) (first expectations))
      (let [res (first result)]
        (is (=  (first expectations) [(:name res) (:display-string res) (:insert-string res)]))
        (recur (next result) (next expectations))))))

(defn- completions-for [world opts initial-contents & expectations]
  (let [[viewer code-node viewer-node] (load-buffer world script/ScriptNode opts initial-contents)]
    (should-propose viewer expectations)))

(defmacro completions [world & pairs]
  (assert (even? (count pairs)))
  (list* `do (mapcat (fn [[given expect]] `[(completions-for ~world lua/lua ~given ~@expect)]) (partition 2 pairs))))

(defmacro completions-in [world module-code & pairs]
  (assert (even? (count pairs)))
  (let [module-node (gensym)]
    `(let [[~module-node] (tx-nodes (g/make-node ~world script/ScriptNode :code ~module-code))]
       ~@(map (fn [[given expect]]
                `(let [[viewer# code-node# viewer-node#] (load-buffer ~world script/ScriptNode lua/lua ~given)]
                   (g/connect! ~module-node :_node-id code-node# :module-nodes)
                   (should-propose viewer# ~expect)))
              (partition 2 pairs)))))

(deftest lua-completions-propose-test
  (with-clean-system
    (completions world
                 "asser|"                                       [["assert"        "assert(v[,message])"     "assert(v)"]]
                 "if|"                                          [["if"
                                                                  "if                                if cond then"
                                                                  "if cond then\n\t--do things\nend"]]
                 "go|"                                          [["go"            "go"                      "go"]]
                 "vm|"                                          [["vmath"         "vmath"                   "vmath"]]
                 "mat|"                                         [["math"          "math"                    "math"]]
                 "go.proper|"                                   [["go.property"   "go.property(name,value)" "go.property(name,value)"]]
                 "go.delete|"                                   [["go.delete"     "go.delete([id])"         "go.delete()"]
                                                                 ["go.delete_all" "go.delete_all([ids])"    "go.delete_all()"]]
                 "local foo=1 \n foo|"                          [["foo"           "foo"                     "foo"]]
                 "bar=1 \n ba|"                                 [["bar"           "bar"                     "bar"]]
                 "local function cake(x,y) return x end\n cak|" [["cake"          "cake(x,y)"               "cake(x,y)"]]
                 "function ice_cream(x,y) return x end\n ice|"  [["ice_cream"     "ice_cream(x,y)"          "ice_cream(x,y)"]]
                 "self.velocity = vm|"                          [["vmath"         "vmath"                   "vmath"]]
                 "go.propert| = vm"                             [["go.property"   "go.property(name,value)" "go.property(name,value)"]]))

  (with-redefs [code-completion/resource-node-path (constantly "/mymodule.lua")]
    (with-clean-system
      (completions-in world "local mymodule={} \n function mymodule.add(x,y) return x end \n return mymodule"
                      "require(\"mymodule\") \n mymodule.|"        [["mymodule.add" "mymodule.add(x,y)" "mymodule.add(x,y)"]]
                      "foo = require(\"mymodule\") \n foo.|"       [["foo.add"      "foo.add(x,y)"      "foo.add(x,y)"]]
                      "local bar = require(\"mymodule\") \n bar.|" [["bar.add"      "bar.add(x,y)"      "bar.add(x,y)"]]
                      "require(\"mymodule\") \n mymod|"            [["mymodule"     "mymodule"          "mymodule"]]
                      "foo = require(\"mymodule\") \n foo|"        [["foo"          "foo"               "foo"]]
                      "local bar = require(\"mymodule\") \n ba|"   [["bar"          "bar"               "bar"]]
                      ))))

(deftest test-do-proposal-replacement
  (with-clean-system
    (let [code ""
          opts lua/lua
          source-viewer (setup-source-viewer opts)
          [code-node viewer-node] (setup-code-view-nodes world source-viewer code script/ScriptNode)]
      (testing "basic replace"
        (do-proposal-replacement source-viewer {:insert-string "foo"})
        (is (= "foo" (text source-viewer))))
      (testing "with partial completion"
        (let [code "go.set"]
          (set-code-and-caret! source-viewer code)
          (do-proposal-replacement source-viewer {:insert-string "go.set_property()"})
          (is (= "go.set_property()" (text source-viewer)))))
      (testing "with partial completion and whitespace"
        (let [code "   go.set"]
          (set-code-and-caret! source-viewer code)
          (do-proposal-replacement source-viewer {:insert-string "go.set_property()"})
          (is (= "   go.set_property()" (text source-viewer)))))
      (testing "with whole completion"
        (let [code "   go.set_property()"]
          (set-code-and-caret! source-viewer code)
          (do-proposal-replacement source-viewer {:insert-string "go.set_property()"})
          (is (= "   go.set_property()" (text source-viewer)))))
      (testing "with whole completion within other function"
        (let [code "   assert(math.a"]
          (set-code-and-caret! source-viewer code)
          (do-proposal-replacement source-viewer {:insert-string "math.abs()"})
          (is (= "   assert(math.abs()" (text source-viewer)))))
      (testing "with replacement at end of line with scoped before"
        (let [code "    go.set_prop = vma"]
          (set-code-and-caret! source-viewer code)
          (do-proposal-replacement source-viewer {:insert-string "vmath"})
          (is (= "    go.set_prop = vmath" (text source-viewer)))))
      (testing "with replacement not at end of line with"
        (let [code "vmat = go.set"]
          (set-code-and-caret! source-viewer code)
          (caret! source-viewer 4 false)
          (do-proposal-replacement source-viewer {:insert-string "vmath"})
          (is (= "vmath = go.set" (text source-viewer)))))
      (testing "with replacement with simlar spelling"
        (let [code "vmatches = vm"]
          (set-code-and-caret! source-viewer code)
          (do-proposal-replacement source-viewer {:insert-string "vmath"})
          (is (= "vmatches = vmath" (text source-viewer))))))))

(deftest test-single-value-proposals
  (with-clean-system
    (let [code ""
          opts lua/lua
          source-viewer (setup-source-viewer opts)
          [code-node viewer-node] (setup-code-view-nodes world source-viewer code script/ScriptNode)]
      (testing "single result gets automatically inserted"
        (set-code-and-caret! source-viewer "math.ab")
        (propose! source-viewer)
        (is (= "math.abs(x)" (text source-viewer))))
      (testing "single result of defold lib gets automatically inserted"
        (set-code-and-caret! source-viewer "vmat")
        (propose! source-viewer)
        (is (= "vmath" (text source-viewer))))
      (testing "proper indentation is put in"
        (set-code-and-caret! source-viewer "function foo(x)\n\tif")
        (propose! source-viewer)
        (is (= "function foo(x)\n\tif cond then\n\t\t--do things\n\tend" (text source-viewer)))))))

(deftest test-proposal-tab-triggers
  (with-clean-system
    (let [code ""
          opts lua/lua
          source-viewer (setup-source-viewer opts)
          [code-node viewer-node] (setup-code-view-nodes world source-viewer code script/ScriptNode)]
      (testing "single arg"
        (set-code-and-caret! source-viewer "math.ab")
        (propose! source-viewer)
        (is (= "math.abs(x)" (text source-viewer)))
        (is (= "x" (text-selection source-viewer)))
        (tab! source-viewer)
        (is (= "" (text-selection source-viewer)))
        (is (= 11 (caret source-viewer))))
      (testing "multiple arg"
        (set-code-and-caret! source-viewer "string.sub")
        (propose! source-viewer)
        (is (= "string.sub(s,i)" (text source-viewer)))
        (is (= "s" (text-selection source-viewer)))
        (tab! source-viewer)
        (is (= "i" (text-selection source-viewer)))
        (tab! source-viewer)
        (is (= "" (text-selection source-viewer)))
        (is (= 15 (caret source-viewer))))
      (testing "no args"
        (set-code-and-caret! source-viewer "go.delete_all")
        (propose! source-viewer)
        (is (= "go.delete_all()" (text source-viewer)))
        (is (= "" (text-selection source-viewer)))
        (is (= 15 (caret source-viewer))))
      (testing "with typing for arg values"
        (set-code-and-caret! source-viewer "string.sub")
        (propose! source-viewer)
        (is (= "string.sub(s,i)" (text source-viewer)))
        (is (= "s" (text-selection source-viewer)))
        (key-typed! source-viewer "1")
        (tab! source-viewer)
        (is (= "string.sub(1,i)" (text source-viewer)))
        (is (= "i" (text-selection source-viewer)))
        (key-typed! source-viewer "2")
        (is (= "string.sub(1,2)" (text source-viewer)))
        (tab! source-viewer)
        (is (= "" (text-selection source-viewer)))
        (is (= 15 (caret source-viewer))))
      (testing "with if template"
        (set-code-and-caret! source-viewer "if")
        (propose! source-viewer)
        (is (= "if cond then\n\t--do things\nend" (text source-viewer)))
        (is (= "cond" (text-selection source-viewer)))
        (tab! source-viewer)
        (is (= "--do things" (text-selection source-viewer)))
        (tab! source-viewer)
        (is (= "end" (text-selection source-viewer)))
        (tab! source-viewer)
        (is (= "" (text-selection source-viewer))))
      (testing "with function template"
        (set-code-and-caret! source-viewer "function")
        (propose! source-viewer)
        (is (= "function function_name(self)\n\t--do things\nend"(text source-viewer)))
        (is (= "function_name" (text-selection source-viewer)))
        (tab! source-viewer)
        (is (= "self" (text-selection source-viewer)))
        (tab! source-viewer)
        (is (= "--do things" (text-selection source-viewer)))
        (tab! source-viewer)
        (is (= "" (text-selection source-viewer)))
        (is (= (count "function function_name(self)\n\t--do things\nend") (caret source-viewer))))
      (testing "shift tab backwards"
        (set-code-and-caret! source-viewer "if")
        (propose! source-viewer)
        (is (= "if cond then\n\t--do things\nend" (text source-viewer)))
        (is (= "cond" (text-selection source-viewer)))
        (tab! source-viewer)
        (is (= "--do things" (text-selection source-viewer)))
        (tab! source-viewer)
        (is (= "end" (text-selection source-viewer)))
        (shift-tab! source-viewer)
        (is (= "--do things" (text-selection source-viewer)))
        (shift-tab! source-viewer)
        (is (= "cond" (text-selection source-viewer)))
        (shift-tab! source-viewer)
        (is (= "cond" (text-selection source-viewer)))
        (tab! source-viewer)
        (is (= "--do things" (text-selection source-viewer)))
))))

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
