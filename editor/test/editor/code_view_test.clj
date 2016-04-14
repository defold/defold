(ns editor.code-view-test
  (:require [clojure.test :refer :all]
            [dynamo.graph :as g]
            [editor.code-view :as cv]
            [editor.lua :as lua]
            [editor.script :as script]
            [editor.gl.shader :as shader]
            [support.test-support :refer [with-clean-system tx-nodes]])
  (:import [org.eclipse.jface.text IDocument DocumentEvent TextUtilities]
           [org.eclipse.fx.text.ui TextPresentation]))


(defn ->style-range-map [sr]
  {:start (.-start sr)
   :length (.-length sr)
   :stylename (.-stylename sr)})

(defn viewer-node-style-maps [vnode]
  (let [document (->  (g/node-value vnode :source-viewer)
                      (.getDocument))
        document-len (.getLength document)
        text-widget (.getTextWidget  (g/node-value vnode :source-viewer))
        len (dec (.getCharCount text-widget))
        style-ranges (.getStyleRanges text-widget  (int 0) len false)
        style-maps (map ->style-range-map style-ranges)]
    style-maps))

(deftest lua-syntax
  (with-clean-system
   (let [code ""
         opts lua/lua
         source-viewer (cv/setup-source-viewer opts)
         [code-node viewer-node] (tx-nodes (g/make-node world script/ScriptNode)
                                           (g/make-node world cv/CodeView :source-viewer source-viewer))]
     (g/transact (g/set-property code-node :code code))
     (cv/setup-code-view viewer-node code-node 0)
     (testing "default style"
       (let [new-code "x="]
         (g/transact (g/set-property code-node :code new-code))
         (g/node-value viewer-node :new-content)
         (is (= [{:start 0 :length 2 :stylename "default"}] (viewer-node-style-maps viewer-node)))))
     (testing "number style"
       (let [new-code "22"]
         (g/transact (g/set-property code-node :code new-code))
         (g/node-value viewer-node :new-content)
         (is (= [{:start 0 :length 2 :stylename "number"}] (viewer-node-style-maps viewer-node)))))
     (testing "keyword style"
       (let [new-code "break"]
         (g/transact (g/set-property code-node :code new-code))
         (g/node-value viewer-node :new-content)
         (is (= [{:start 0 :length 5 :stylename "keyword"}] (viewer-node-style-maps viewer-node)))))
     (testing "string style"
       (let [new-code "\"foo\""]
         (g/transact (g/set-property code-node :code new-code))
         (g/node-value viewer-node :new-content)
         (is (= [{:start 0 :length 5 :stylename "string"}] (viewer-node-style-maps viewer-node)))))
     (testing "single line comment style"
       (let [new-code "--foo"]
         (g/transact (g/set-property code-node :code new-code))
         (g/node-value viewer-node :new-content)
         (is (= [{:start 0 :length 5 :stylename "comment"}] (viewer-node-style-maps viewer-node)))))
     (testing "single line comment style stays to one line"
       (let [new-code "--foo\n bar"]
         (g/transact (g/set-property code-node :code new-code))
         (g/node-value viewer-node :new-content)
         (is (= {:start 0 :length 5 :stylename "comment"} (first (viewer-node-style-maps viewer-node))))))
     (testing "multi line comment style"
       (let [new-code "--[[\nmultilinecomment\n]]"]
         (g/transact (g/set-property code-node :code new-code))
         (g/node-value viewer-node :new-content)
         (is (= [{:start 0 :length 24 :stylename "comment2"}] (viewer-node-style-maps viewer-node)))))
     (testing "multi line comment style terminates"
       (let [new-code "--[[\nmultilinecomment\n]] morecode"]
         (g/transact (g/set-property code-node :code new-code))
         (g/node-value viewer-node :new-content)
         (is (= {:start 0 :length 24 :stylename "comment2"} (first (viewer-node-style-maps viewer-node)))))))))

(deftest shader-glsl-vertex-program
  (with-clean-system
   (let [code "nothings"
         opts (:code shader/glsl-opts)
         source-viewer (cv/setup-source-viewer opts)
         [code-node viewer-node] (tx-nodes (g/make-node world shader/ShaderNode)
                                           (g/make-node world cv/CodeView :source-viewer source-viewer))]
     (g/transact (g/set-property code-node :code code))
     (cv/setup-code-view viewer-node code-node 0)
     (testing "default style"
       (let [new-code "x="]
         (g/transact (g/set-property code-node :code new-code))
         (g/node-value viewer-node :new-content)
         (viewer-node-style-maps viewer-node)
         (is (= [{:start 0 :length 2 :stylename "default"}] (viewer-node-style-maps viewer-node)))))
    (testing "number style"
       (let [new-code "22"]
         (g/transact (g/set-property code-node :code new-code))
         (g/node-value viewer-node :new-content)
         (is (= [{:start 0 :length 2 :stylename "number"}] (viewer-node-style-maps viewer-node)))))
    (testing "keyword style"
      (let [new-code "template"]
        (g/transact (g/set-property code-node :code new-code))
        (g/node-value viewer-node :new-content)
        (is (= [{:start 0 :length 8 :stylename "keyword"}] (viewer-node-style-maps viewer-node)))))
    (testing "string style"
      (let [new-code "\"foo\""]
        (g/transact (g/set-property code-node :code new-code))
        (g/node-value viewer-node :new-content)
        (is (= [{:start 0 :length 5 :stylename "string"}] (viewer-node-style-maps viewer-node)))))
     (testing "single line comment style"
       (let [new-code "//foo"]
         (g/transact (g/set-property code-node :code new-code))
         (g/node-value viewer-node :new-content)
         (is (= [{:start 0 :length 5 :stylename "comment"}] (viewer-node-style-maps viewer-node)))))
     (testing "single line comment style stays to one line"
       (let [new-code "//foo\n bar"]
         (g/transact (g/set-property code-node :code new-code))
         (g/node-value viewer-node :new-content)
         (is (= {:start 0 :length 5 :stylename "comment"} (first (viewer-node-style-maps viewer-node))))))
     (testing "multi line comment style"
       (let [new-code "/**multilinecomment\n**/"]
         (g/transact (g/set-property code-node :code new-code))
         (g/node-value viewer-node :new-content)
         (is (= [{:start 0 :length 23 :stylename "comment2"}] (viewer-node-style-maps viewer-node)))))
     (testing "multi line comment style terminates"
       (let [new-code "/**multilinecomment\n**/ more stuff"]
         (g/transact (g/set-property code-node :code new-code))
         (g/node-value viewer-node :new-content)
         (is (= {:start 0 :length 23 :stylename "comment2"} (first (viewer-node-style-maps viewer-node)))))))))
