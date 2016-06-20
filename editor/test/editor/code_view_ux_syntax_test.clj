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
            [editor.code-view-test :as cvt :refer [setup-code-view-nodes]]
            [support.test-support :refer [with-clean-system tx-nodes]]))

(defn- toggle-comment! [source-viewer]
  (handler/run :toggle-comment [{:name :code-view :env {:selection source-viewer}}]{}))

(defn do-toggle-line-comment [node-type opts comment-str]
  (with-clean-system
    (let [code "hello world"
          source-viewer (setup-source-viewer opts false)
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
    (let [code "line1\nline2\nline3\nline4"
          source-viewer (setup-source-viewer opts false)
          [code-node viewer-node] (setup-code-view-nodes world source-viewer code node-type)]
      (testing "toggle region comment"
        (text-selection! source-viewer 8 9)
        (is (= "ne2\nline3" (text-selection source-viewer)))
        (toggle-comment! source-viewer)
        (is (= (str "line1\n" comment-str "line2\n" comment-str "line3\nline4") (text source-viewer)))
        (text-selection! source-viewer 11 12)
        (toggle-comment! source-viewer)
        (is (= code (text source-viewer)))))))

(deftest toggle-comment-region-test
  (testing "lua syntax"
    (do-toggle-region-comment script/ScriptNode lua/lua "-- "))
  (testing "glsl syntax"
    (do-toggle-region-comment shader/ShaderNode (:code shader/glsl-opts) "// ")))

(defn- set-completion-code! [source-viewer code]
  (text! source-viewer code)
  (caret! source-viewer (count code) false)
  (changes! source-viewer))

(deftest lua-completions-propose-test
  (with-clean-system
    (let [code ""
          source-viewer (setup-source-viewer lua/lua false)
          [code-node viewer-node] (setup-code-view-nodes world source-viewer code script/ScriptNode)]
      (testing "global lua std lib"
        (set-completion-code! source-viewer "asser")
        (let [result (propose source-viewer)]
         (is (= ["assert"] (map :name result)))
         (is (= ["assert(v[,message])"] (map :display-string result)))
         (is (= ["assert(v)"] (map :insert-string result)))))
      (testing "global defold package"
        (set-completion-code! source-viewer "go")
        (is (= ["go"] (map :name (propose source-viewer)))))
      (testing "global lua std lib package"
        (set-completion-code! source-viewer "mat")
        (is (= ["math"] (map :name (propose source-viewer)))))
      (testing "go.property function - required params"
        (set-completion-code! source-viewer "go.proper")
        (let [result (propose source-viewer)]
         (is (= ["go.property"] (map :name result)))
         (is (= ["go.property(name,value)"] (map :display-string result)))
         (is (= ["go.property(name,value)"] (map :insert-string result)))))
      (testing "go.delete functions - required params"
        (set-completion-code! source-viewer "go.delete")
        (let [result (propose source-viewer)]
         (is (= ["go.delete" "go.delete_all"] (map :name result)))
         (is (= ["go.delete([id])" "go.delete_all([ids])"] (map :display-string result)))
         (is (= ["go.delete()" "go.delete_all()"] (map :insert-string result)))))
      (testing "local var"
        (set-completion-code! source-viewer "local foo=1 \n fo")
        (is (= ["foo"] (map :name (propose source-viewer)))))
      (testing "globar var"
        (set-completion-code! source-viewer "bar=1 \n ba")
        (is (= ["bar"] (map :name (propose source-viewer)))))
      (testing "local function"
        (set-completion-code! source-viewer "local function cake(x,y) return x end\n cak")
        (is (= ["cake"] (map :name (propose source-viewer)))))
      (testing "global function"
        (set-completion-code! source-viewer "function ice_cream(x,y) return x end\n ice")
        (is (= ["ice_cream"] (map :name (propose source-viewer)))))
      (testing "requires"
        (with-redefs [code-completion/resource-node-path (constantly "/mymodule.lua")]
          (let [module-code "local mymodule={} \n function mymodule.add(x,y) return x end \n return mymodule"
                [module-node] (tx-nodes (g/make-node world script/ScriptNode :code module-code))]
            (g/connect! module-node :_node-id code-node :module-nodes)
            (testing "bare requires"
              (set-completion-code! source-viewer "require(\"mymodule\") \n mymodule.")
              (is (= ["mymodule.add"] (map :name (propose source-viewer)))))
            (testing "require with global var"
              (set-completion-code! source-viewer "foo = require(\"mymodule\") \n foo.")
              (is (= ["foo.add"] (map :name (propose source-viewer)))))
            (testing "require with local var"
              (set-completion-code! source-viewer "local bar = require(\"mymodule\") \n bar.")
              (is (= ["bar.add"] (map :name (propose source-viewer)))))
            (testing "module var bare requires"
              (set-completion-code! source-viewer "require(\"mymodule\") \n mymod")
              (is (= ["mymodule"] (map :name (propose source-viewer)))))
            (testing "module var require with global var"
              (set-completion-code! source-viewer "foo = require(\"mymodule\") \n fo")
              (is (= ["foo"] (map :name (propose source-viewer)))))
            (testing "module var require with local var"
              (set-completion-code! source-viewer "local bar = require(\"mymodule\") \n ba")
              (is (= ["bar"] (map :name (propose source-viewer)))))))))))

(deftest test-do-proposal-replacement
  (with-clean-system
    (let [code ""
          opts lua/lua
          source-viewer (setup-source-viewer opts false)
          [code-node viewer-node] (setup-code-view-nodes world source-viewer code script/ScriptNode)]
      (testing "basic replace"
        (do-proposal-replacement source-viewer {:insert-string "foo"})
        (is (= "foo" (text source-viewer))))
      (testing "with partial completion"
        (set-completion-code! source-viewer "go.set")
        (do-proposal-replacement source-viewer {:insert-string "go.set_property()"})
        (is (= "go.set_property()" (text source-viewer))))
      (testing "with partial completion and whitespace"
        (set-completion-code! source-viewer "   go.set")
        (do-proposal-replacement source-viewer {:insert-string "go.set_property()"})
        (is (= "   go.set_property()" (text source-viewer))))
      (testing "with whole completion"
        (set-completion-code! source-viewer "   go.set_property()")
        (do-proposal-replacement source-viewer {:insert-string "go.set_property()"})
        (is (= "   go.set_property()" (text source-viewer)))))))

(defn- propose! [source-viewer]
  (handler/run :proposals [{:name :code-view :env {:selection source-viewer}}]{}))

(deftest test-single-value-proposals
  (with-clean-system
    (let [code ""
          opts lua/lua
          source-viewer (setup-source-viewer opts false)
          [code-node viewer-node] (setup-code-view-nodes world source-viewer code script/ScriptNode)]
      (testing "single result gets automatically inserted"
        (set-completion-code! source-viewer "math.ab")
        (propose! source-viewer)
        (is (= "math.abs(x)" (text source-viewer)))))))


