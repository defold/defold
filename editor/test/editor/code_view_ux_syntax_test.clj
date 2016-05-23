(ns editor.code-view-ux-syntax-test
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
