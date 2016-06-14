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
          var-names (set (map :name (get completions "")))]
      (is (contains? go-names "go.set_position"))
      (is (= #{"a" "b" "c" "d" "x"} var-names)))))


(deftest script-node-with-required-modules
  (with-clean-system
    (let [foo-resource (FileResource. world (io/file (io/resource "lua/foo.lua")) nil)
          mymath-resource (FileResource. world (io/file (io/resource "lua/mymath.lua")) nil)
          [script-node mymath-node] (tx-nodes [(g/make-node world script/ScriptNode
                                                            :resource foo-resource
                                                            :code (slurp foo-resource))
                                               (g/make-node world script/ScriptNode
                                                            :resource mymath-resource
                                                            :code (slurp mymath-resource))])
          completions (with-redefs [find-node-for-module (constantly mymath-node)]
                        (g/node-value script-node :completions))
          foo-names (set (map :name (get completions "")))
          mymath-names (set (map :name (get completions "foo")))]
      (is (= #{"x" "foo"} foo-names))
      (is (= #{"mymath.add" "mymath.sub"} mymath-names)))))

