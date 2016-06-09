(ns editor.code-completion-test
(:require [clojure.java.io :as io]
          [clojure.test :refer :all]
          [dynamo.graph :as g]
          [editor.script :as script]
          [editor.resource :as resource]
          [editor.code-completion :refer :all]
          [support.test-support :refer [tx-nodes with-clean-system]])
(:import [editor.resource FileResource]))

(deftest code-completion-node-without-connected-resources
  (with-clean-system
    (let [[code-complete-node] (tx-nodes (g/make-node world CodeCompletionNode))
          defold-docs (g/node-value code-complete-node :defold-docs)
          completions (g/node-value code-complete-node :completions)]
      (is (= "go.set_position" (get-in defold-docs ["go" 2 :name])))
      (is (= "go.set_position" (get-in completions [:script "go" 2 :name]))))))

(deftest code-completion-node-with-connected-resources
  (with-clean-system
    (let [var-resource (FileResource. world (io/file (io/resource "lua/variable_test.lua")) nil)
          func-resource (FileResource. world (io/file (io/resource "lua/function_test.lua")) nil)
          [code-complete-node
           var-script-node
           func-script-node] (tx-nodes [(g/make-node world CodeCompletionNode)
                                        (g/make-node world script/ScriptNode
                                                     :resource var-resource
                                                     :code (slurp var-resource))
                                        (g/make-node world script/ScriptNode
                                                     :resource func-resource
                                                     :code (slurp func-resource))])]
      (g/transact (concat
                   (g/connect var-script-node :completion-info code-complete-node :resource-completions)
                   (g/connect func-script-node :completion-info code-complete-node :resource-completions)))
      (is (= {:vars #{"a" "b" "c" "d"} :functions {} :requires {} :namespace "variable_test"}
             (g/node-value var-script-node :completion-info)))
      (let [completions (g/node-value code-complete-node :completions)
            go-names (set (map :name (get-in completions [:script "go"])))
            var-names (set (map :name (get-in completions [:script "variable_test"])))
            func-names (set (map :name (get-in completions [:script "function_test"])))
            func-display-names (set (map :display-string (get-in completions [:script "function_test"])))]
        (is (contains? go-names "go.set_position"))
        (is (contains? var-names "a"))
        (is (contains? func-names "add"))
        (is (contains? func-display-names "add([\"num1\"],[\"num2\"])"))))))
