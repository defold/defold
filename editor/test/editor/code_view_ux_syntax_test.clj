(ns editor.code-view-syntax-test
  (:require [clojure.test :refer :all]
            [dynamo.graph :as g]
            [editor.code-view :as cv :refer :all]
            [editor.code-view-ux :as cvx :refer :all]
            [editor.handler :as handler]
            [editor.lua :as lua]
            [editor.script :as script]
            [editor.gl.shader :as shader]
            [editor.code-view-test :as cvt :refer [setup-code-view-nodes]]
            [support.test-support :refer [with-clean-system tx-nodes]]))

(defn- toggle-comment! [source-viewer]
  (handler/run :toggle-comment [{:name :code-view :env {:selection source-viewer}}]{}))

(deftest toggle-comment-test
  (testing "lua syntax"
   (with-clean-system
     (let [code "hello world"
           opts lua/lua
           source-viewer (setup-source-viewer opts false)
           [code-node viewer-node] (setup-code-view-nodes world source-viewer code script/ScriptNode)]
       (testing "toggle line comment"
         (toggle-comment! source-viewer)
         (is (= "-- hello world" (text source-viewer)))
         (toggle-comment! source-viewer)
         (is (= "hello world" (text source-viewer)))))))
  (testing "glsl syntax"
   (with-clean-system
     (let [code "hello world"
           opts (:code shader/glsl-opts)
           source-viewer (setup-source-viewer opts false)
           [code-node viewer-node] (setup-code-view-nodes world source-viewer code script/ScriptNode)]
       (testing "toggle line comment"
         (toggle-comment! source-viewer)
         (is (= "// hello world" (text source-viewer)))
         (toggle-comment! source-viewer)
         (is (= "hello world" (text source-viewer))))))))
