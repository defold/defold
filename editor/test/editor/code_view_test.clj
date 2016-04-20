(ns editor.code-view-test
  (:require [clojure.test :refer :all]
            [dynamo.graph :as g]
            [editor.code-view :as cv]
            [editor.handler :as handler]
            [editor.lua :as lua]
            [editor.script :as script]
            [editor.gl.shader :as shader]
            [support.test-support :refer [with-clean-system tx-nodes]])
  (:import [javafx.scene.input Clipboard]
           [org.eclipse.jface.text IDocument DocumentEvent TextUtilities]
           [org.eclipse.fx.text.ui TextPresentation]))

(defn ->style-range-map [sr]
  {:start (.-start sr)
   :length (.-length sr)
   :stylename (.-stylename sr)})

(defn viewer-node-style-maps [source-viewer]
  (let [document (->  source-viewer (.getDocument))
        document-len (.getLength document)
        text-widget (.getTextWidget source-viewer)
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
         (is (= [{:start 0 :length 2 :stylename "default"}] (viewer-node-style-maps source-viewer)))))
     (testing "number style"
       (let [new-code "22"]
         (g/transact (g/set-property code-node :code new-code))
         (g/node-value viewer-node :new-content)
         (is (= [{:start 0 :length 2 :stylename "number"}] (viewer-node-style-maps source-viewer)))))
     (testing "keyword style"
       (let [new-code "break"]
         (g/transact (g/set-property code-node :code new-code))
         (g/node-value viewer-node :new-content)
         (is (= [{:start 0 :length 5 :stylename "keyword"}] (viewer-node-style-maps source-viewer)))))
     (testing "string style"
       (let [new-code "\"foo\""]
         (g/transact (g/set-property code-node :code new-code))
         (g/node-value viewer-node :new-content)
         (is (= [{:start 0 :length 5 :stylename "string"}] (viewer-node-style-maps source-viewer)))))
     (testing "single line comment style"
       (let [new-code "--foo"]
         (g/transact (g/set-property code-node :code new-code))
         (g/node-value viewer-node :new-content)
         (is (= [{:start 0 :length 5 :stylename "comment"}] (viewer-node-style-maps source-viewer)))))
     (testing "single line comment style stays to one line"
       (let [new-code "--foo\n bar"]
         (g/transact (g/set-property code-node :code new-code))
         (g/node-value viewer-node :new-content)
         (is (= {:start 0 :length 5 :stylename "comment"} (first (viewer-node-style-maps source-viewer))))))
     (testing "multi line comment style"
       (let [new-code "--[[\nmultilinecomment\n]]"]
         (g/transact (g/set-property code-node :code new-code))
         (g/node-value viewer-node :new-content)
         (is (= [{:start 0 :length 24 :stylename "comment-multi"}] (viewer-node-style-maps source-viewer)))))
     (testing "multi line comment style terminates"
       (let [new-code "--[[\nmultilinecomment\n]] morecode"]
         (g/transact (g/set-property code-node :code new-code))
         (g/node-value viewer-node :new-content)
         (is (= {:start 0 :length 24 :stylename "comment-multi"} (first (viewer-node-style-maps source-viewer)))))))))

(deftest glsl-syntax
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
         (is (= [{:start 0 :length 2 :stylename "default"}] (viewer-node-style-maps source-viewer)))))
    (testing "number style"
       (let [new-code "22"]
         (g/transact (g/set-property code-node :code new-code))
         (g/node-value viewer-node :new-content)
         (is (= [{:start 0 :length 2 :stylename "number"}] (viewer-node-style-maps source-viewer)))))
    (testing "keyword style"
      (let [new-code "template"]
        (g/transact (g/set-property code-node :code new-code))
        (g/node-value viewer-node :new-content)
        (is (= [{:start 0 :length 8 :stylename "keyword"}] (viewer-node-style-maps source-viewer)))))
    (testing "string style"
      (let [new-code "\"foo\""]
        (g/transact (g/set-property code-node :code new-code))
        (g/node-value viewer-node :new-content)
        (is (= [{:start 0 :length 5 :stylename "string"}] (viewer-node-style-maps source-viewer)))))
     (testing "single line comment style"
       (let [new-code "//foo"]
         (g/transact (g/set-property code-node :code new-code))
         (g/node-value viewer-node :new-content)
         (is (= [{:start 0 :length 5 :stylename "comment"}] (viewer-node-style-maps source-viewer)))))
     (testing "single line comment style stays to one line"
       (let [new-code "//foo\n bar"]
         (g/transact (g/set-property code-node :code new-code))
         (g/node-value viewer-node :new-content)
         (is (= {:start 0 :length 5 :stylename "comment"} (first (viewer-node-style-maps source-viewer))))))
     (testing "multi line comment style"
       (let [new-code "/**multilinecomment\n**/"]
         (g/transact (g/set-property code-node :code new-code))
         (g/node-value viewer-node :new-content)
         (is (= [{:start 0 :length 23 :stylename "comment-multi"}] (viewer-node-style-maps source-viewer)))))
     (testing "multi line comment style terminates"
       (let [new-code "/**multilinecomment\n**/ more stuff"]
         (g/transact (g/set-property code-node :code new-code))
         (g/node-value viewer-node :new-content)
         (is (= {:start 0 :length 23 :stylename "comment-multi"} (first (viewer-node-style-maps source-viewer)))))))))

(defrecord TestClipboard [content]
  cv/Copyable
  (put-copy [this s]
    (assoc this :content s))
  cv/Pastable
  (has-paste? [this]
    (some? (:content this)))
  (get-paste [this]
    (:content this)))

(deftest copy-test
  (with-clean-system
    (let [clipboard (new TestClipboard "")
          code "the quick brown fox"
          opts lua/lua
          source-viewer (cv/setup-source-viewer opts)
          [code-node viewer-node] (tx-nodes (g/make-node world script/ScriptNode)
                                            (g/make-node world cv/CodeView :source-viewer source-viewer))]
      (g/transact (g/set-property code-node :code code))
      (cv/setup-code-view viewer-node code-node 0)
      (g/node-value viewer-node :new-content)
      (.setSelectionRange (.getTextWidget source-viewer) 4 5)
      (let [result (handler/run :copy
                     [{:name :code-view :env {:selection source-viewer :code-node code-node :clipboard clipboard}}]
                     {})]
        (is (= "quick" (:content result)))))))

(deftest paste-test
  (with-clean-system
    (let [clipboard (new TestClipboard "hello")
          code " world"
          opts lua/lua
          source-viewer (cv/setup-source-viewer opts)
          [code-node viewer-node] (tx-nodes (g/make-node world script/ScriptNode)
                                            (g/make-node world cv/CodeView :source-viewer source-viewer))]
      (g/transact (g/set-property code-node :code code))
      (cv/setup-code-view viewer-node code-node 0)
      (g/node-value viewer-node :new-content)
      (g/transact (g/set-property code-node :code code))
      (let [result (handler/run :paste
                     [{:name :code-view :env {:selection source-viewer :code-node code-node :clipboard clipboard}}]
                     {})]
       )
      (g/node-value viewer-node :new-content)
      (is (= "hello world" (g/node-value code-node :code))))))
