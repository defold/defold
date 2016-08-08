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

(defn- toggle-comment! [source-viewer]
  (cvx/handler-run :toggle-comment [{:name :code-view :env {:selection source-viewer}}]{}))

(defn do-toggle-line-comment [node-type opts comment-str]
  (with-clean-system
    (let [code "hello world"
          source-viewer (setup-source-viewer opts)
          [code-node viewer-node] (setup-code-view-nodes world source-viewer code node-type)]
      (testing "toggle line comment"
        (toggle-comment! source-viewer)
        (is (= (str comment-str "hello world") (text source-viewer)))
        (toggle-comment! source-viewer)
        (is (= "hello world" (text source-viewer)))))))

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

(defn- set-code-and-caret! [source-viewer code]
  (text! source-viewer code)
  (caret! source-viewer (count code) false)
  (changes! source-viewer))

(deftest lua-completions-propose-test
  (with-clean-system
    (let [code ""
          source-viewer (setup-source-viewer lua/lua)
          [code-node viewer-node] (setup-code-view-nodes world source-viewer code script/ScriptNode)]
      (testing "global lua std lib"
        (set-code-and-caret! source-viewer "asser")
        (let [result (propose source-viewer)]
         (is (= ["assert"] (map :name result)))
         (is (= ["assert(v[,message])"] (map :display-string result)))
         (is (= ["assert(v)"] (map :insert-string result)))))
      (testing "global lua base functions"
        (set-code-and-caret! source-viewer "if")
        (let [result (propose source-viewer)]
         (is (= ["if"] (map :name result)))
         (is (= ["if                                if cond then"] (map :display-string result)))
         (is (= ["if cond then\n\t--do things\nend"] (map :insert-string result)))))
      (testing "global defold package"
        (set-code-and-caret! source-viewer "go")
        (is (= ["go"] (map :name (propose source-viewer))))
        (set-code-and-caret! source-viewer "vm")
        (is (= ["vmath"] (map :name (propose source-viewer)))))
      (testing "global lua std lib package"
        (set-code-and-caret! source-viewer "mat")
        (is (= ["math"] (map :name (propose source-viewer)))))
      (testing "go.property function - required params"
        (set-code-and-caret! source-viewer "go.proper")
        (let [result (propose source-viewer)]
         (is (= ["go.property"] (map :name result)))
         (is (= ["go.property(name,value)"] (map :display-string result)))
         (is (= ["go.property(name,value)"] (map :insert-string result)))))
      (testing "go.delete functions - required params"
        (set-code-and-caret! source-viewer "go.delete")
        (let [result (propose source-viewer)]
         (is (= ["go.delete" "go.delete_all"] (map :name result)))
         (is (= ["go.delete([id])" "go.delete_all([ids])"] (map :display-string result)))
         (is (= ["go.delete()" "go.delete_all()"] (map :insert-string result)))))
      (testing "local var"
        (set-code-and-caret! source-viewer "local foo=1 \n foo")
        (is (= ["foo"] (map :name (propose source-viewer)))))
      (testing "globar var"
        (set-code-and-caret! source-viewer "bar=1 \n ba")
        (is (= ["bar"] (map :name (propose source-viewer)))))
      (testing "local function"
        (set-code-and-caret! source-viewer "local function cake(x,y) return x end\n cak")
        (is (= ["cake"] (map :name (propose source-viewer)))))
      (testing "global function"
        (set-code-and-caret! source-viewer "function ice_cream(x,y) return x end\n ice")
        (is (= ["ice_cream"] (map :name (propose source-viewer)))))
      (testing "var after a scoped function"
        (set-code-and-caret! source-viewer "self.velocity = vm")
        (is (= ["vmath"] (map :name (propose source-viewer)))))
      (testing "proposal at the start of the line"
        (set-code-and-caret! source-viewer "go.propert = vm")
        (caret! source-viewer 10 false)
        (is (= ["go.property"] (map :name (propose source-viewer)))))
      (testing "requires"
        (with-redefs [code-completion/resource-node-path (constantly "/mymodule.lua")]
          (let [module-code "local mymodule={} \n function mymodule.add(x,y) return x end \n return mymodule"
                [module-node] (tx-nodes (g/make-node world script/ScriptNode :code module-code))]
            (g/connect! module-node :_node-id code-node :module-nodes)
            (testing "bare requires"
              (set-code-and-caret! source-viewer "require(\"mymodule\") \n mymodule.")
              (is (= ["mymodule.add"] (map :name (propose source-viewer)))))
            (testing "require with global var"
              (set-code-and-caret! source-viewer "foo = require(\"mymodule\") \n foo.")
              (is (= ["foo.add"] (map :name (propose source-viewer)))))
            (testing "require with local var"
              (set-code-and-caret! source-viewer "local bar = require(\"mymodule\") \n bar.")
              (is (= ["bar.add"] (map :name (propose source-viewer)))))
            (testing "module var bare requires"
              (set-code-and-caret! source-viewer "require(\"mymodule\") \n mymod")
              (is (= ["mymodule"] (map :name (propose source-viewer)))))
            (testing "module var require with global var"
              (set-code-and-caret! source-viewer "foo = require(\"mymodule\") \n foo")
              (is (= ["foo"] (map :name (propose source-viewer)))))
            (testing "module var require with local var"
              (set-code-and-caret! source-viewer "local bar = require(\"mymodule\") \n ba")
              (is (= ["bar"] (map :name (propose source-viewer)))))))))))

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

(defn- propose! [source-viewer]
  (cvx/handler-run :proposals [{:name :code-view :env {:selection source-viewer}}]{}))

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

(defn- tab! [source-viewer]
  (cvx/handler-run :tab [{:name :code-view :env {:selection source-viewer}}]{}))

(defn- shift-tab! [source-viewer]
  (cvx/handler-run :backwards-tab-trigger [{:name :code-view :env {:selection source-viewer}}]{}))

(defn- key-typed! [source-viewer key-typed]
  (cvx/handler-run :key-typed [{:name :code-view :env {:selection source-viewer :key-typed key-typed}}] {}))

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

(defn- enter! [source-viewer]
  (cvx/handler-run :enter [{:name :code-view :env {:selection source-viewer}}]{}))

(deftest lua-enter-indentation-functions
  (with-clean-system
    (let [code ""
          opts lua/lua
          source-viewer (setup-source-viewer opts)
          [code-node viewer-node] (setup-code-view-nodes world source-viewer code script/ScriptNode)]
      (testing "indentation for functions"
        (testing "no indentation level"
          (buffer-commands (load-buffer world script/ScriptNode opts)
                           (should-be "function test(x)|")

                           (enter!)
                           (should-be "function test(x)"
                                      "\t|")))
        (testing "some indentation exists"
          (buffer-commands (load-buffer world script/ScriptNode opts)
                           (should-be "if true then"
                                      "\tfunction test(x)|")

                           (enter!)
                           (should-be "if true then"
                                      "\tfunction test(x)"
                                      "\t\t|")))
        (testing "maintains level in the function"
          (buffer-commands (load-buffer world script/ScriptNode opts)
                           (should-be "function test(x)\n\tfoo|")

                           (enter!)
                           (should-be "function test(x)"
                                      "\tfoo"
                                      "\t|")))
        (testing "end deindents"
          (buffer-commands (load-buffer world script/ScriptNode opts)
                           (should-be "function test(x)\n\tend|")

                           (enter!)
                           (should-be  "function test(x)"
                                       "end"
                                       "|")))
        (testing "no enter identation if not at an end of the line"
          (buffer-commands (load-buffer world script/ScriptNode opts)
                           (should-be "fu|nction test(x)")

                           (enter!)
                           (should-be  "fu"
                                       "|nction test(x)")))))))

(deftest lua-enter-indentation-if-else
  (with-clean-system
    (let [code ""
          opts lua/lua
          source-viewer (setup-source-viewer opts)
          [code-node viewer-node] (setup-code-view-nodes world source-viewer code script/ScriptNode)]
      (testing "indentation for if else"
        (testing "if indents"
          (set-code-and-caret! source-viewer "if true")
          (enter! source-viewer)
          (is (= "if true\n\t" (text source-viewer))))
        (testing "elseif dedents and indents the next level"
          (set-code-and-caret! source-viewer "if true\n\telseif")
          (enter! source-viewer)
          (is (= "if true\nelseif\n\t" (text source-viewer))))
        (testing "else dedents and indents to the next level"
          (set-code-and-caret! source-viewer "if true\n\telse")
          (enter! source-viewer)
          (is (= "if true\nelse\n\t" (text source-viewer))))
        (testing "end dedents"
          (set-code-and-caret! source-viewer "if true\n\tend")
          (enter! source-viewer)
          (is (= "if true\nend\n" (text source-viewer))))))))

(deftest lua-enter-indentation-while
  (with-clean-system
    (let [code ""
          opts lua/lua
          source-viewer (setup-source-viewer opts)
          [code-node viewer-node] (setup-code-view-nodes world source-viewer code script/ScriptNode)]
      (testing "indentation for while"
        (testing "while indents"
          (set-code-and-caret! source-viewer "while true")
          (enter! source-viewer)
          (is (= "while true\n\t" (text source-viewer))))
        (testing "end dedents"
          (set-code-and-caret! source-viewer "while true\n\tend")
          (enter! source-viewer)
          (is (= "while true\nend\n" (text source-viewer))))))))

(deftest lua-enter-indentation-tables
  (with-clean-system
    (let [code ""
          opts lua/lua
          source-viewer (setup-source-viewer opts)
          [code-node viewer-node] (setup-code-view-nodes world source-viewer code script/ScriptNode)]
      (testing "indentation for tables"
        (testing "{ indents"
          (set-code-and-caret! source-viewer "x = {")
          (enter! source-viewer)
          (is (= "x = {\n\t" (text source-viewer))))
        (testing "} dedents"
          (set-code-and-caret! source-viewer "x = {\n\t1\n\t}")
          (enter! source-viewer)
          (is (= "x = {\n\t1\n}\n" (text source-viewer))))))))

(defn- indent! [source-viewer]
  (cvx/handler-run :indent [{:name :code-view :env {:selection source-viewer}}]{}))

(deftest lua-enter-indent-line-and-region
  (with-clean-system
    (let [code ""
          opts lua/lua
          source-viewer (setup-source-viewer opts)
          [code-node viewer-node] (setup-code-view-nodes world source-viewer code script/ScriptNode)]
      (testing "indent line"
        (set-code-and-caret! source-viewer "if true then\nreturn 1")
        (indent! source-viewer)
        (is (= "if true then\n\treturn 1" (text source-viewer))))
      (testing "indent region"
        (let [code "if true then\nreturn 1\nif false then\nreturn 2\nend\nend"]
          (set-code-and-caret! source-viewer code)
          (text-selection! source-viewer 0 (count code))
          (indent! source-viewer)
          (is (= "if true then\n\treturn 1\n\tif false then\n\t\treturn 2\n\tend\nend" (text source-viewer))))))))
