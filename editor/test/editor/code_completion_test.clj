(ns editor.code-completion-test
(:require [clojure.java.io :as io]
          [clojure.test :refer :all]
          [dynamo.graph :as g]
          [editor.script :as script]
          [editor.resource :as resource]
          [editor.code-completion :refer :all]
          [support.test-support :refer [tx-nodes with-clean-system]])
(:import [editor.resource FileResource]))

(deftest script-node-with-no-required-modules
  (with-clean-system
    (let [var-resource (FileResource. world (io/file (io/resource "lua/variable_test.lua")) nil)
          [script-node] (tx-nodes (g/make-node world script/ScriptNode
                                                     :resource var-resource
                                                     :code (slurp var-resource)))
          completions (g/node-value script-node :completions)
          go-names (set (map :name (get completions "go")))
          var-names (set (map :name (get completions "")))
          vars-in-file #{"a" "b" "c" "d" "x" }]
      (is (contains? go-names "go.set_position"))
      (is (contains? var-names "go"))
      (is (= vars-in-file (set (filter vars-in-file var-names)))))))


(deftest script-node-with-required-modules
  (with-clean-system
    (let [foo-resource (FileResource. world (io/file (io/resource "lua/foo.lua")) nil)
          foo-code (slurp foo-resource)
          mymath-resource (FileResource. world (io/file (io/resource "lua/mymath.lua")) nil)
          [script-node mymath-node] (tx-nodes [(g/make-node world script/ScriptNode
                                                            :resource foo-resource
                                                            :code foo-code)
                                               (g/make-node world script/ScriptNode
                                                            :resource mymath-resource
                                                            :code (slurp mymath-resource))])
          project-search-count (atom 0)
          test-find-in-project (constantly (do (swap! project-search-count inc) mymath-node))
          completions (with-redefs [find-module-node-in-project test-find-in-project
                                    resource-node-path (constantly "mymath.lua")]
                        (g/node-value script-node :completions))
          var-names (set (map :name (get completions "")))
          mymath-names (set (map :name (get completions "foo")))]
      (is (=  #{"x" "foo"} (set (filter  #{"x" "foo"} var-names))))
      (is (= #{"foo.add" "foo.sub"} mymath-names))
      (is (= 1 @project-search-count))
      (testing "searches in connected modules first before looking in the project "
        (g/transact (g/set-property script-node :code (str foo-code "\n y=3")))
        (let [completions  (with-redefs [find-module-node-in-project test-find-in-project
                                             resource-node-path (constantly "/mymath.lua")]
                                 (g/node-value script-node :completions))
              var-names (set (map :name (get completions "")))]
          (is (=  #{"x" "foo" "y"} (set (filter  #{"x" "foo" "y"} var-names))))
          (is (= 1 @project-search-count))))

      (testing "bare requires"
        (g/transact (g/set-property script-node :code "require(\"mymath\")"))
        (let [completions  (with-redefs [find-module-node-in-project test-find-in-project
                                             resource-node-path (constantly "/mymath.lua")]
                                 (g/node-value script-node :completions))
              mymath-names (set (map :name (get completions "mymath")))]
          (is (= #{"mymath.add" "mymath.sub"} mymath-names))
          (is (= 1 @project-search-count)))))))
