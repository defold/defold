(ns editor.code-view-ux-test
  (:require [clojure.test :refer :all]
            [clojure.string :as string]
            [dynamo.graph :as g]
            [editor.code-view :as cv :refer :all]
            [editor.code-view-ux :as cvx :refer :all]
            [editor.handler :as handler]
            [editor.lua :as lua]
            [editor.script :as script]
            [editor.code-view-test :as cvt :refer [setup-code-view-nodes]]
            [support.test-support :refer [with-clean-system tx-nodes]])
  (:import [com.sun.javafx.tk Toolkit]
           [javafx.scene.input KeyEvent KeyCode]))

(defrecord TestClipboard [content]
  TextContainer
  (text! [this s] (reset! content s))
  (text [this] @content)
  (replace! [this offset length s]))

(defmacro with-code [opts code & forms]
  `(with-clean-system
     (let [~'opts ~opts
           ~'code ~code
           ~'source-viewer (setup-source-viewer ~opts)
           [~'code-node ~'viewer-node] (setup-code-view-nodes ~'world ~'source-viewer ~code script/ScriptNode)]
       ~@forms)))

(defn- ->context [source-viewer clipboard]
  {:name :code-view
   :env {:source-viewer source-viewer :clipboard clipboard}})

(defn- get-char-at-caret [source-viewer]
  (get (text source-viewer) (caret source-viewer)))

(defn- go-to-line! [source-viewer line-number]
  ;; bypassing handler for the dialog handling
  (go-to-line source-viewer line-number))

(defn- key-typed! [source-viewer key-typed]
  (cvx/handler-run :key-typed [(->context source-viewer nil)] {:key-typed key-typed
                                                               :key-event (KeyEvent. KeyEvent/KEY_TYPED
                                                                                     key-typed
                                                                                     ""
                                                                                     (KeyCode/getKeyCode key-typed)
                                                                                     false
                                                                                     false
                                                                                     false
                                                                                     false)}))

(defn- tab! [source-viewer]
  (cvx/handler-run :tab [(->context source-viewer nil)]{}))

(defn- enter! [source-viewer]
  (cvx/handler-run :enter [(->context source-viewer nil)]{}))

(defn- right! [source-viewer]
  (cvx/handler-run :right [(->context source-viewer nil)] {}))

(defn- left! [source-viewer]
  (cvx/handler-run :left [(->context source-viewer nil)] {}))

(defn- up! [source-viewer]
  (cvx/handler-run :up [(->context source-viewer nil)] {}))

(defn- down! [source-viewer]
  (cvx/handler-run :down [(->context source-viewer nil)] {}))

(defn- next-word! [source-viewer]
  (cvx/handler-run :next-word [(->context source-viewer nil)] {}))

(defn- prev-word! [source-viewer]
  (cvx/handler-run :prev-word [(->context source-viewer nil)] {}))

(defn- beginning-of-line! [source-viewer]
  (cvx/handler-run :beginning-of-line [(->context source-viewer nil)]{}))

(defn- end-of-line! [source-viewer]
  (cvx/handler-run :end-of-line [(->context source-viewer nil)]{}))

(defn- beginning-of-file! [source-viewer]
  (cvx/handler-run :beginning-of-file [(->context source-viewer nil)]{}))

(defn- end-of-file! [source-viewer]
  (cvx/handler-run :end-of-file [(->context source-viewer nil)]{}))

(defn- select-right! [source-viewer]
  (cvx/handler-run :select-right [(->context source-viewer nil)] {}))

(defn- select-left! [source-viewer]
  (cvx/handler-run :select-left [(->context source-viewer nil)] {}))

(defn- select-up! [source-viewer]
  (cvx/handler-run :select-up [(->context source-viewer nil)] {}))

(defn- select-down! [source-viewer]
  (cvx/handler-run :select-down [(->context source-viewer nil)] {}))

(defn- select-word! [source-viewer]
  (cvx/handler-run :select-word [(->context source-viewer nil)]{}))

(defn- select-next-word! [source-viewer]
  (cvx/handler-run :select-next-word [(->context source-viewer nil)] {}))

(defn- select-prev-word! [source-viewer]
  (cvx/handler-run :select-prev-word [(->context source-viewer nil)] {}))

(defn- select-line! [source-viewer]
  (cvx/handler-run :select-line [(->context source-viewer nil)]{}))

(defn- select-beginning-of-line! [source-viewer]
  (cvx/handler-run :select-beginning-of-line [(->context source-viewer nil)]{}))

(defn- select-end-of-line! [source-viewer]
  (cvx/handler-run :select-end-of-line [(->context source-viewer nil)]{}))

(defn- select-beginning-of-file! [source-viewer]
  (cvx/handler-run :select-beginning-of-file [(->context source-viewer nil)]{}))

(defn- select-end-of-file! [source-viewer]
  (cvx/handler-run :select-end-of-file [(->context source-viewer nil)]{}))

(defn- select-all! [source-viewer]
  (cvx/handler-run :select-all [(->context source-viewer nil)]{}))

(defn- delete-backward! [source-viewer]
  (cvx/handler-run :delete-backward [(->context source-viewer nil)]{}))

(defn- delete! [source-viewer]
  (cvx/handler-run :delete [(->context source-viewer nil)]{}))

(defn- delete-prev-word! [source-viewer]
  (cvx/handler-run :delete-prev-word [(->context source-viewer nil)]{}))

(defn- delete-next-word! [source-viewer]
  (cvx/handler-run :delete-next-word [(->context source-viewer nil)]{}))

(defn- delete-to-end-of-line! [source-viewer]
  (cvx/handler-run :delete-to-end-of-line [(->context source-viewer nil)] {}))

(defn- delete-to-beginning-of-line! [source-viewer]
  (cvx/handler-run :delete-to-beginning-of-line [(->context source-viewer nil)] {}))

(defn- copy! [source-viewer clipboard]
  (cvx/handler-run :copy [(->context source-viewer clipboard)] {}))

(defn- paste! [source-viewer clipboard]
  (cvx/handler-run :paste [(->context source-viewer clipboard)]{}))

(defn- cut! [source-viewer clipboard]
  (cvx/handler-run :cut [(->context source-viewer clipboard)]{}))

(defn- cut-to-end-of-line! [source-viewer clipboard]
  (cvx/handler-run :cut-to-end-of-line [(->context source-viewer clipboard)]{}))

(defn- undo! [source-viewer view-node code-node]
  (g/undo! (g/node-id->graph-id code-node)))

(defn- redo! [source-viewer view-node code-node]
  (g/redo! (g/node-id->graph-id code-node)))

(defn- set-code-and-caret! [source-viewer code]
  (text! source-viewer code)
  (caret! source-viewer (count code) false)
  (typing-changes! source-viewer))

(defn- propose! [source-viewer]
  (cvx/handler-run :proposals [(->context source-viewer nil)]{}))

(defn- refresh-viewer [viewer-node]
  (g/node-value viewer-node :new-content))

(deftest key-typed-test
  (with-code lua/lua "hello"
    (testing "typing without text selected"
      (key-typed! source-viewer "a")
      (is (= "ahello" (text source-viewer))))
    (testing "typing without text selected replaces the selected text"
      (text-selection! source-viewer 0 1)
      (key-typed! source-viewer "b")
      (is (= "bhello" (text source-viewer))))
    (testing "making the text view non-editable prevents typing"
      (editable! source-viewer false)
      (key-typed! source-viewer "x")
      (is (= "bhello" (text source-viewer))))
    (testing "automatch works"
      (editable! source-viewer true)
      (text! source-viewer "")
      (key-typed! source-viewer "[")
      (is (= "[]" (text source-viewer)))
      (key-typed! source-viewer "1")
      (is (= "[1]" (text source-viewer))))))

(deftest copy-paste-test
  (let [clipboard (new TestClipboard (atom ""))]
    (with-code lua/lua "hello world"
      (text-selection! source-viewer 6 5)
      (copy! source-viewer clipboard)
      (testing "pasting without text selected"
        (caret! source-viewer 0 false)
        (paste! source-viewer clipboard)
        (is (= "world" (text clipboard)))
        (is (= "worldhello world" (text source-viewer))))
      (testing "pasting with text selected"
        (text-selection! source-viewer 0 10)
        (paste! source-viewer clipboard)
        (is (= "world world" (text source-viewer)))))))

(deftest move-right-left-test
  (with-code lua/lua "hello world"
    (caret! source-viewer 5 false)
    (testing "moving right"
      (right! source-viewer)
      (is (= 6 (caret source-viewer))))
    (testing "moving left"
      (left! source-viewer)
      (is (= 5 (caret source-viewer))))
    (testing "out of bounds right"
      (caret! source-viewer (count code) false)
      (right! source-viewer)
      (right! source-viewer)
      (is (= (count code) (caret source-viewer))))
    (testing "out of bounds left"
      (caret! source-viewer 0 false)
      (left! source-viewer)
      (left! source-viewer)
      (is (= 0 (caret source-viewer))))
    (testing "with chunk of selected text, and the cursor at the start, right takes cursor to end of chunk"
      (caret! source-viewer 0 false)
      (text-selection! source-viewer 0 5)
      (is (= 0 (caret source-viewer)))
      (is (= "hello" (text-selection source-viewer)))
      (right! source-viewer)
      (is (= "" (text-selection source-viewer)))
      (is (= 5 (caret source-viewer))))
    (testing "with chunk of selected text, and the cursor at the end, right takes cursor one forward"
      (caret! source-viewer 4 false)
      (text-selection! source-viewer 0 4)
      (is (= 4 (caret source-viewer)))
      (is (= "hell" (text-selection source-viewer)))
      (right! source-viewer)
      (is (= "" (text-selection source-viewer)))
      (is (= 5 (caret source-viewer))))
    (testing "with chunk of selected text, and the cursor at the end, left takes cursor to beginning of chunk"
      (caret! source-viewer 5 false)
      (text-selection! source-viewer 0 5)
      (is (= 5 (caret source-viewer)))
      (is (= "hello" (text-selection source-viewer)))
      (left! source-viewer)
      (is (= "" (text-selection source-viewer)))
      (is (= 0 (caret source-viewer))))
    (testing "with chunk of selected text, and the cursor at the start, left takes cursor one step back"
      (caret! source-viewer 1 false)
      (text-selection! source-viewer 1 4)
      (is (= 1 (caret source-viewer)))
      (is (= "ello" (text-selection source-viewer)))
      (left! source-viewer)
      (is (= "" (text-selection source-viewer)))
      (is (= 0 (caret source-viewer))))))

(deftest select-move-right-left-test
  (with-code lua/lua "hello world"
    (testing "selecting right"
      (select-right! source-viewer)
      (select-right! source-viewer)
      (select-right! source-viewer)
      (select-right! source-viewer)
      (select-right! source-viewer)
      (is (= 5 (caret source-viewer)))
      (is (= "hello" (text-selection source-viewer))))
    (testing "selecting left"
      (caret! source-viewer 11 false)
      (select-left! source-viewer)
      (select-left! source-viewer)
      (select-left! source-viewer)
      (select-left! source-viewer)
      (select-left! source-viewer)
      (is (= 6 (caret source-viewer)))
      (is (= "world" (text-selection source-viewer))))
    (testing "out of bounds right"
      (caret! source-viewer (count code) false)
      (select-right! source-viewer)
      (select-right! source-viewer)
      (is (= (count code) (caret source-viewer))))
    (testing "out of bounds left"
      (caret! source-viewer 0 false)
      (select-left! source-viewer)
      (select-left! source-viewer)
      (is (= 0 (caret source-viewer))))))

(deftest move-up-down-test
  (with-code lua/lua "line1\nline2\n\nline3and"
    (caret! source-viewer 4 false)
    (preferred-offset! source-viewer 4)
    (is (= \1 (get-char-at-caret source-viewer)))
    (testing "moving down"
      (down! source-viewer)
      (is (= \2 (get-char-at-caret source-viewer)))
      (down! source-viewer))
    (is (= \newline (get-char-at-caret source-viewer)))
    (down! source-viewer)
    (is (= \3 (get-char-at-caret source-viewer)))
    ;; can't move further down
    (down! source-viewer)
    (is (= \3 (get-char-at-caret source-viewer)))
    (testing "out of bounds down"
      (down! source-viewer)
      (down! source-viewer)
      (is (= \3 (get-char-at-caret source-viewer))))
    (testing "moving up"
      (up! source-viewer)
      (is (= \newline (get-char-at-caret source-viewer)))
      (up! source-viewer)
      (is (= \2 (get-char-at-caret source-viewer)))
      (up! source-viewer)
      (is (= \1 (get-char-at-caret source-viewer)))
      (up! source-viewer)
      ;; can't move further up
      (is (= \1 (get-char-at-caret source-viewer))))
    (testing "out of bounds up"
      (up! source-viewer)
      (is (= \1 (get-char-at-caret source-viewer))))
    (testing "respects tab spacing"
      (let [new-code "line1\n\t2345"]
        (text! source-viewer new-code)
        (caret! source-viewer 3 false)
        (preferred-offset! source-viewer 4)
        (is (= \e (get-char-at-caret source-viewer)))
        (down! source-viewer)
        (is (= \2 (get-char-at-caret source-viewer)))
        (up! source-viewer)
        (is (= \1 (get-char-at-caret source-viewer)))))
    (testing "with chunk of selected text, down takes cursor to end of chunk"
      (preferred-offset! source-viewer 0)
      (text! source-viewer "line1\nline2\nline3")
      (caret! source-viewer 0 false)
      (text-selection! source-viewer 0 15)
      (is (= 0 (caret source-viewer)))
      (is (= "line1\nline2\nlin" (text-selection source-viewer)))
      (down! source-viewer)
      (is (= "" (text-selection source-viewer)))
      (is (= 15 (caret source-viewer))))
    (testing "with chunk of selected text, up takes cursor to start of chunk"
      (preferred-offset! source-viewer 0)
      (text! source-viewer "line1\nline2\nline3")
      (caret! source-viewer 15 false)
      (text-selection! source-viewer 0 15)
      (is (= 15 (caret source-viewer)))
      (is (= "line1\nline2\nlin" (text-selection source-viewer)))
      (up! source-viewer)
      (is (= "" (text-selection source-viewer)))
      (is (= 0 (caret source-viewer))))))

(deftest select-move-up-down-test
  (with-code lua/lua "line1\nline2"
    (caret! source-viewer 4 false)
    (preferred-offset! source-viewer 4)
    (is (= \1 (get-char-at-caret source-viewer)))
    (testing "select moving down"
      (select-down! source-viewer)
      (is (= "1\nline" (text-selection source-viewer))))
    (testing "select moving up"
      (select-up! source-viewer)
      (is (= "" (text-selection source-viewer)))
      (select-up! source-viewer)
      (is (= "line" (text-selection source-viewer))))))

(deftest word-move-test
  (with-code lua/lua "the quick.brown\nfox_test"
    (is (= \t (get-char-at-caret source-viewer)))
    (testing "moving next word"
      (next-word! source-viewer)
      (is (= \space (get-char-at-caret source-viewer)))
      (next-word! source-viewer)
      (is (= \. (get-char-at-caret source-viewer)))
      (next-word! source-viewer)
      (is (= \b (get-char-at-caret source-viewer)))
      (next-word! source-viewer)
      (is (= \newline (get-char-at-caret source-viewer)))
      (next-word! source-viewer)
      (is (= \f (get-char-at-caret source-viewer)))
      (next-word! source-viewer)
      (is (= \_ (get-char-at-caret source-viewer)))
      (next-word! source-viewer)
      (is (= nil (get-char-at-caret source-viewer))))
    (testing "moving prev word"
      (prev-word! source-viewer)
      (is (= \t (get-char-at-caret source-viewer)))
      (prev-word! source-viewer)
      (is (= \f (get-char-at-caret source-viewer)))
      (prev-word! source-viewer)
      (is (= \newline (get-char-at-caret source-viewer)))
      (prev-word! source-viewer)
      (is (= \b (get-char-at-caret source-viewer)))
      (prev-word! source-viewer)
      (is (= \. (get-char-at-caret source-viewer)))
      (prev-word! source-viewer)
      (is (= \q (get-char-at-caret source-viewer)))
      (prev-word! source-viewer)
      (is (= \t (get-char-at-caret source-viewer))))
    (testing "moving by word remembers col position"
      (text! source-viewer "aed.111\nbcf 222")
      (caret! source-viewer 0 false)
      (next-word! source-viewer)
      (is (= \. (get-char-at-caret source-viewer)))
      (down! source-viewer)
      (is (= \space (get-char-at-caret source-viewer)))
      (prev-word! source-viewer)
      (is (= \b (get-char-at-caret source-viewer)))
      (up! source-viewer)
      (is (= \a (get-char-at-caret source-viewer))))
    (testing "treats opererators correctly"
      (text! source-viewer "local v = 1")
      (caret! source-viewer 0 false)
      (next-word! source-viewer)
      (next-word! source-viewer)
      (next-word! source-viewer)
      (is (= 9 (caret source-viewer))))))

(deftest select-word-move-test
  (with-code lua/lua "the quick"
    (is (= \t (get-char-at-caret source-viewer)))
    (testing "select moving next word"
      (select-next-word! source-viewer)
      (is (= "the" (text-selection source-viewer)))
      (select-next-word! source-viewer)
      (is (= "the quick" (text-selection source-viewer))))
    (testing "moving prev word"
      (select-prev-word! source-viewer)
      (is (= "the " (text-selection source-viewer)))
      (select-prev-word! source-viewer)
      (is (= "" (text-selection source-viewer))))))

(deftest beginning-of-line-to-end-test
  (with-code lua/lua "hello world"
    (testing "moving to beginning of the line"
      (caret! source-viewer 4 false)
      (is (= \o (get-char-at-caret source-viewer)))
      (beginning-of-line! source-viewer)
      (is (= \h (get-char-at-caret source-viewer)))
      (beginning-of-line! source-viewer)
      (is (= \h (get-char-at-caret source-viewer))))
    (testing "moving to the end of the line"
      (caret! source-viewer 4 false)
      (is (= \o (get-char-at-caret source-viewer)))
      (end-of-line! source-viewer)
      (is (= nil (get-char-at-caret source-viewer)))
      (is (= 11 (caret source-viewer)))
      (end-of-line! source-viewer)
      (is (= 11 (caret source-viewer))))
    (testing "moving remembers col position"
      (text! source-viewer "line1\nline2")
      (caret! source-viewer 0 false)
      (end-of-line! source-viewer)
      (down! source-viewer)
      (is (= 11 (caret source-viewer)))
      (beginning-of-line! source-viewer)
      (up! source-viewer)
      (is (= 0 (caret source-viewer))))))

(deftest select-beginning-of-line-to-end-test
  (with-code lua/lua "hello world"
    (testing "selecting to beginning of the line"
      (caret! source-viewer 4 false)
      (is (= \o (get-char-at-caret source-viewer)))
      (select-beginning-of-line! source-viewer)
      (is (= "hell" (text-selection source-viewer))))
    (testing "selecting to the end of the line"
      (caret! source-viewer 4 false)
      (is (= \o (get-char-at-caret source-viewer)))
      (select-end-of-line! source-viewer)
      (is (= "o world" (text-selection source-viewer))))))

(deftest beginning-of-file-to-end-test
  (with-code lua/lua "hello\nworld"
    (testing "moving to beginning of the file"
      (caret! source-viewer 4 false)
      (is (= \o (get-char-at-caret source-viewer)))
      (beginning-of-file! source-viewer)
      (is (= 0 (caret source-viewer))))
    (testing "moving to the end of the file"
      (caret! source-viewer 4 false)
      (is (= \o (get-char-at-caret source-viewer)))
      (end-of-file! source-viewer)
      (is (= 11 (caret source-viewer))))))

(deftest select-beginning-of-file-to-end-test
  (with-code lua/lua "hello\nworld"
    (testing "selecting to the end of the file"
      (caret! source-viewer 4 false)
      (is (= \o (get-char-at-caret source-viewer)))
      (select-end-of-file! source-viewer)
      (is (= "o\nworld" (text-selection source-viewer))))
    (testing "selecting to the beginning of the file"
      (caret! source-viewer 8 false)
      (is (= \r (get-char-at-caret source-viewer)))
      (select-beginning-of-file! source-viewer)
      (is (= "hello\nwo" (text-selection source-viewer))))))

(deftest goto-line-test
  (with-code lua/lua "a\nb\nc"
    (testing "goto line"
      (go-to-line! source-viewer 2)
      (is (= \b (get-char-at-caret source-viewer)))
      (go-to-line! source-viewer 3)
      (is (= \c (get-char-at-caret source-viewer)))
      (go-to-line! source-viewer 1)
      (is (= \a (get-char-at-caret source-viewer))))
    (testing "goto line bounds"
      (go-to-line! source-viewer 0)
      (is (= \a (get-char-at-caret source-viewer)))
      (go-to-line! source-viewer -2)
      (is (= \a (get-char-at-caret source-viewer)))
      (go-to-line! source-viewer 100)
      (is (= nil (get-char-at-caret source-viewer)))
      (is (= 5 (caret source-viewer))))))

(deftest select-word-test
  (with-code lua/lua "blue  cat"
    (testing "caret is in the middle of the word"
      (caret! source-viewer 2 false)
      (is (= \u (get-char-at-caret source-viewer)))
      (select-word! source-viewer)
      (is (= "blue" (text-selection source-viewer))))
    (testing "caret is at the end of the word"
      (caret! source-viewer 4 false)
      (is (= \space (get-char-at-caret source-viewer)))
      (select-word! source-viewer)
      (is (= "blue" (text-selection source-viewer))))
    (testing "caret is at the beginning of the word"
      (caret! source-viewer 6 false)
      (is (= \c (get-char-at-caret source-viewer)))
      (select-word! source-viewer)
      (is (= "cat" (text-selection source-viewer))))
    (testing "caret is not near words"
      (caret! source-viewer 5 false)
      (is (= \space (get-char-at-caret source-viewer)))
      (select-word! source-viewer)
      (is (= "" (text-selection source-viewer))))))

(deftest select-line-test
  (with-code lua/lua "line1\nline2"
    (testing "selects the line"
      (select-line! source-viewer)
      (is (= "line1" (text-selection source-viewer)))
      (caret! source-viewer 7 false)
      (select-line! source-viewer)
      (is (= "line2" (text-selection source-viewer))))))

(deftest select-all-test
  (with-code lua/lua "hello there"
    (testing "selects the entire doc"
      (select-all! source-viewer)
      (is (= "hello there" (text-selection source-viewer)))
      (is (= (caret source-viewer) (count (text source-viewer)))))))

(deftest delete-backward-test
  (with-code lua/lua "blue"
    (testing "deleting"
      (caret! source-viewer 3 false)
      (delete-backward! source-viewer)
      (is (= \e (get-char-at-caret source-viewer)))
      (is (= "ble" (text source-viewer))))
    (testing "deleting with highlighting selection"
      (caret! source-viewer 0 false)
      (text-selection! source-viewer 0 2)
      (delete-backward! source-viewer)
      (is (= "e" (text source-viewer))))
    (testing "delete works with automatch"
      (text! source-viewer "[]hello")
      (caret! source-viewer 1 false)
      (delete-backward! source-viewer)
      (is (= "hello" (text source-viewer))))
    (testing "automatch delete doesn't invoke when second char is deleted"
      (text! source-viewer "[]hello")
      (caret! source-viewer 2 false)
      (delete-backward! source-viewer)
      (is (= "[hello" (text source-viewer))))))

(deftest delete-test
  (with-code lua/lua "blue"
    (testing "deleting"
      (caret! source-viewer 2 false)
      (delete! source-viewer)
      (is (= \e (get-char-at-caret source-viewer)))
      (is (= "ble" (text source-viewer))))
    (testing "deleting with highlighting selection"
      (caret! source-viewer 0 false)
      (text-selection! source-viewer 0 2)
      (delete! source-viewer)
      (is (= "e" (text source-viewer))))
    (testing "delete works with automatch"
      (text! source-viewer "[]hello")
      (caret! source-viewer 0 false)
      (delete! source-viewer)
      (is (= "hello" (text source-viewer))))
    (testing "automatch delete doesn't invoke when second char is deleted"
      (text! source-viewer "[]hello")
      (caret! source-viewer 1 false)
      (delete! source-viewer)
      (is (= "[hello" (text source-viewer))))))

(deftest cut-test
  (let [clipboard (new TestClipboard (atom ""))]
    (with-code lua/lua "blue duck"
      (testing "cutting"
        (text-selection! source-viewer 0 4)
        (cut! source-viewer clipboard)
        (is (= " duck" (text source-viewer)))
        (caret! source-viewer 5 false)
        (paste! source-viewer clipboard)
        (is (= " duckblue" (text source-viewer))))
      (testing "cutting without selection cuts the line"
        (text! source-viewer "line1\nline2")
        (text-selection! source-viewer 0 0)
        (caret! source-viewer 9 false)
        (is (= \e (get-char-at-caret source-viewer)))
        (cut! source-viewer clipboard)
        (= "line1\n" (text source-viewer))
        (caret! source-viewer 0 false)
        (paste! source-viewer clipboard)
        (= "line1\n" (text source-viewer))))))

(deftest delete-by-word
  (with-code lua/lua "blue duck"
    (testing "deleting back"
      (caret! source-viewer 7 false)
      (is (= \c (get-char-at-caret source-viewer)))
      (delete-prev-word! source-viewer)
      (is (= "blue ck" (text source-viewer)))
      (is (= \c  (get-char-at-caret source-viewer)))
      (delete-prev-word! source-viewer)
      (is (= "ck" (text source-viewer)))
      (is (= \c  (get-char-at-caret source-viewer))))
    (testing "deleting next"
      (text! source-viewer code)
      (caret! source-viewer 2 false)
      (is (= \u (get-char-at-caret source-viewer)))
      (delete-next-word! source-viewer)
      (is (= "bl duck" (text source-viewer)))
      (is (= \space  (get-char-at-caret source-viewer)))
      (delete-next-word! source-viewer)
      (is (= "bl" (text source-viewer)))
      (is (= nil  (get-char-at-caret source-viewer))))))

(deftest delete-to-start-end-line-test
  (with-code lua/lua "blue duck"
    (testing "deleting to end of the line"
      (caret! source-viewer 7 false)
      (is (= \c (get-char-at-caret source-viewer)))
      (delete-to-end-of-line! source-viewer)
      (is (= "blue du" (text source-viewer)))
      (is (= nil (get-char-at-caret source-viewer))))
    (testing "deleting to start of the line"
      (text! source-viewer code)
      (caret! source-viewer 7 false)
      (is (= \c (get-char-at-caret source-viewer)))
      (delete-to-beginning-of-line! source-viewer)
      (is (= "ck" (text source-viewer)))
      (is (= \c (get-char-at-caret source-viewer))))))

(deftest cut-to-end-line-test
  (let [clipboard (new TestClipboard (atom ""))]
    (with-code lua/lua "line1\nline2\nline3"
      (testing "cutting to end of the line"
        (caret! source-viewer 2 false)
        (cut-to-end-of-line! source-viewer clipboard)
        (is (= "li\nline2\nline3" (text source-viewer)))
        (paste! source-viewer clipboard)
        (is (= "line1\nline2\nline3" (text source-viewer))))
      (testing "multiple kill lines work"
        (caret! source-viewer 0 false)
        (cut-to-end-of-line! source-viewer clipboard)
        (is (= "\nline2\nline3" (text source-viewer)))
        (cut-to-end-of-line! source-viewer clipboard)
        (is (= "line2\nline3" (text source-viewer)))
        (cut-to-end-of-line! source-viewer clipboard)
        (is (= "\nline3" (text source-viewer)))))))

(deftest copy-paste-selection-preserves-caret
  (let [clipboard (new TestClipboard (atom ""))]
    (with-code lua/lua "line1\nline2\nline3"
      (testing "copy-paste-preserves-caret"
        (text-selection! source-viewer 2 8)
        (copy! source-viewer clipboard)
        (paste! source-viewer clipboard)
        (is (= 10 (caret source-viewer)))))))


(defn set-find-params! [{:keys [term replacement whole-word case-sensitive wrap]}]
  (.setValue cvx/find-term (or term ""))
  (.setValue cvx/find-replacement (or replacement ""))
  (.setValue cvx/find-whole-word (or whole-word false))
  (.setValue cvx/find-case-sensitive (or case-sensitive false))
  (.setValue cvx/find-wrap (or wrap false)))

(defn- check-find-next [source-viewer expectations]
  (doseq [[text pos] expectations]
    (cvx/find-next source-viewer)
    (is (= text (text-selection source-viewer)))
    (is (= pos (caret source-viewer)))))

(deftest find-next-test
  (with-code lua/lua "cakes Cakes CAKES cake Cake CAKE"
    (testing "find empty string"
      (caret! source-viewer 0 false)
      (set-find-params! {:term "" :wrap true})
      (check-find-next source-viewer [["" 0]])
      (caret! source-viewer (count code) false)
      (check-find-next source-viewer [["" (count code)]]))
    (testing "find case sensitive"
      (caret! source-viewer 0 false)
      (set-find-params! {:term "cake" :case-sensitive true})
      (check-find-next source-viewer [["cake" 4] ["cake" 22] ["cake" 22]]))
    (testing "find case insensitive"
      (caret! source-viewer 22 false)
      (set-find-params! {:term "cake"})
      (check-find-next source-viewer [["Cake" 27] ["CAKE" 32] ["CAKE" 32]]))
    (testing "find whole word case sensitive"
      (caret! source-viewer 0 false)
      (set-find-params! {:term "cake" :whole-word true :case-sensitive true})
      (check-find-next source-viewer [["cake" 22] ["cake" 22]]))
    (testing "find whole word case insensitive"
      (caret! source-viewer 0 false)
      (set-find-params! {:term "cake" :whole-word true})
      (check-find-next source-viewer [["cake" 22] ["Cake" 27] ["CAKE" 32] ["CAKE" 32]]))
    (testing "wrap wraps"
      (caret! source-viewer 0 false)
      (set-find-params! {:term "cake" :whole-word true :wrap true})
      (check-find-next source-viewer [["cake" 22] ["Cake" 27] ["CAKE" 32] ["cake" 22] ["Cake" 27] ["CAKE" 32]]))))

(defn- check-find-prev [source-viewer expectations]
  (doseq [[text pos] expectations]
    (cvx/find-prev source-viewer)
    (is (= text (text-selection source-viewer)))
    (is (= pos (caret source-viewer)))))

(deftest find-prev-test
  (with-code lua/lua "cake Cake CAKE cakes Cakes CAKES"
    (testing "find empty string"
      (caret! source-viewer 0 false)
      (set-find-params! {:term "" :wrap true})
      (check-find-prev source-viewer [["" 0]])
      (caret! source-viewer (count code) false)
      (check-find-prev source-viewer [["" (count code)]]))
    (testing "find exact case sensitive"
      (caret! source-viewer (count code) false)
      (set-find-params! {:term "cake" :case-sensitive true})
      (check-find-prev source-viewer [["cake" 15] ["cake" 0] ["cake" 0]]))
    (testing "find case insensitive"
      (caret! source-viewer (count code) false)
      (set-find-params! {:term "cake"})
      (check-find-prev source-viewer [["CAKE" 27] ["Cake" 21] ["cake" 15] ["CAKE" 10] ["Cake" 5] ["cake" 0] ["cake" 0]]))
    (testing "find whole word case sensitive"
      (caret! source-viewer (count code) false)
      (set-find-params! {:term "cake" :whole-word true :case-sensitive true})
      (check-find-prev source-viewer [["cake" 0] ["cake" 0]]))
    (testing "find whole word case insensitive"
      (caret! source-viewer (count code) false)
      (set-find-params! {:term "cake" :whole-word true})
      (check-find-prev source-viewer [["CAKE" 10] ["Cake" 5] ["cake" 0] ["cake" 0]]))
    (testing "wrap wraps"
      (caret! source-viewer (count code) false)
      (set-find-params! {:term "cake" :whole-word true :wrap true})
      (check-find-prev source-viewer [["CAKE" 10] ["Cake" 5] ["cake" 0] ["CAKE" 10] ["Cake" 5] ["cake" 0]]))))

(defn- check-replacements [source-viewer expectations]
  (doseq [[new-code selection pos] expectations]
    (cvx/replace-next source-viewer)
    (is (= new-code (text source-viewer)))
    (is (= selection (text-selection source-viewer)))
    (is (= pos (caret source-viewer)))))

(deftest replace-next-test
  (with-code lua/lua "cakes Cakes CAKES cake Cake CAKE"
    (testing "replace empty string"
      (caret! source-viewer 0 false)
      (set-find-params! {:term "" :replacement "bogus"})
      (check-replacements source-viewer [[code "" 0] [code "" 0]])))
  (with-code lua/lua "cakes Cakes CAKES cake Cake CAKE"
    (testing "replace case-sensitive"
      (caret! source-viewer 0 false)
      (set-find-params! {:term "cake" :replacement "tank" :case-sensitive true})
      (check-replacements source-viewer [["cakes Cakes CAKES cake Cake CAKE" "cake" 4]
                                         ["tanks Cakes CAKES cake Cake CAKE" "cake" 22]
                                         ["tanks Cakes CAKES tank Cake CAKE" "" 22]
                                         ["tanks Cakes CAKES tank Cake CAKE" "" 22]])))
  (with-code lua/lua "cakes Cakes CAKES cake Cake CAKE"
    (testing "replace case insensitive"
      (caret! source-viewer 0 false)
      (set-find-params! {:term "cake" :replacement "tank"})
      (check-replacements source-viewer [["cakes Cakes CAKES cake Cake CAKE" "cake" 4]
                                         ["tanks Cakes CAKES cake Cake CAKE" "Cake" 10]
                                         ["tanks tanks CAKES cake Cake CAKE" "CAKE" 16]
                                         ["tanks tanks tankS cake Cake CAKE" "cake" 22]
                                         ["tanks tanks tankS tank Cake CAKE" "Cake" 27]
                                         ["tanks tanks tankS tank tank CAKE" "CAKE" 32]
                                         ["tanks tanks tankS tank tank tank" "" 32]
                                         ["tanks tanks tankS tank tank tank" "" 32]])))
  (with-code lua/lua "cakes Cakes CAKES cake Cake CAKE"
    (testing "replace whole word case sensitive"
      (caret! source-viewer 0 false)
      (set-find-params! {:term "cake" :replacement "tank" :whole-word true :case-sensitive true})
      (check-replacements source-viewer [["cakes Cakes CAKES cake Cake CAKE" "cake" 22]
                                         ["cakes Cakes CAKES tank Cake CAKE" "" 22]
                                         ["cakes Cakes CAKES tank Cake CAKE" "" 22]])))
  (with-code lua/lua "cakes Cakes CAKES cake Cake CAKE"
    (testing "replace whole-word case insensitive"
      (caret! source-viewer 0 false)
      (set-find-params! {:term "cake" :replacement "tank" :whole-word true})
      (check-replacements source-viewer [["cakes Cakes CAKES cake Cake CAKE" "cake" 22]
                                         ["cakes Cakes CAKES tank Cake CAKE" "Cake" 27]
                                         ["cakes Cakes CAKES tank tank CAKE" "CAKE" 32]
                                         ["cakes Cakes CAKES tank tank tank" "" 32]
                                         ["cakes Cakes CAKES tank tank tank" "" 32]])))
  (with-code lua/lua "cakes Cakes CAKES cake Cake CAKE"
    (testing "replace whole word case insensitive wrap"
      (caret! source-viewer 22 false)
      (set-find-params! {:term "cake" :replacement "tank" :whole-word true :wrap true})
      (check-replacements source-viewer [["cakes Cakes CAKES cake Cake CAKE" "Cake" 27]
                                         ["cakes Cakes CAKES cake tank CAKE" "CAKE" 32]
                                         ["cakes Cakes CAKES cake tank tank" "cake" 22]
                                         ["cakes Cakes CAKES tank tank tank" "" 22]
                                         ["cakes Cakes CAKES tank tank tank" "" 22]]))))

(deftest replace-all-test
  (with-code lua/lua "cakes Cakes CAKES cake Cake CAKE"
    (testing "replace empty string"
      (caret! source-viewer 0 false)
      (set-find-params! {:term "" :replacement "bogus"})
      (cvx/replace-all source-viewer)
      (cvx/replace-all source-viewer)
      (is (= "cakes Cakes CAKES cake Cake CAKE" (text source-viewer)))))
  (with-code lua/lua  "cakes Cakes CAKES cake Cake CAKE"
    (testing "replace case sensitive"
      (caret! source-viewer 0 false)
      (set-find-params! {:term "cake" :replacement "tank" :case-sensitive true})
      (cvx/replace-all source-viewer)
      (is (=  "tanks Cakes CAKES tank Cake CAKE" (text source-viewer)))))
  (with-code lua/lua  "cakes Cakes CAKES cake Cake CAKE"
    (testing "replace case insensitive"
      (caret! source-viewer 0 false)
      (set-find-params! {:term "cake" :replacement "tank"})
      (cvx/replace-all source-viewer)
      (is (= "tanks tanks tankS tank tank tank" (text source-viewer)))))
  (with-code lua/lua  "cakes Cakes CAKES cake Cake CAKE"
    (testing "replace whole word case sensitive"
      (caret! source-viewer 0 false)
      (set-find-params! {:term "cake" :replacement "tank" :whole-word true :case-sensitive true})
      (cvx/replace-all source-viewer)
      (is (= "cakes Cakes CAKES tank Cake CAKE" (text source-viewer)))))
  (with-code lua/lua  "cakes Cakes CAKES cake Cake CAKE"
    (testing "replace whole word case insensitive"
      (caret! source-viewer 0 false)
      (set-find-params! {:term "cake" :replacement "tank" :whole-word true})
      (cvx/replace-all source-viewer)
      (is (= "cakes Cakes CAKES tank tank tank" (text source-viewer)))))
  (with-code lua/lua  "cakes Cakes CAKES cake Cake CAKE"
    (testing "replace whole word case insensitive wrap (irrelevant)"
      (caret! source-viewer 0 false)
      (set-find-params! {:term "cake" :replacement "tank" :whole-word true})
      (cvx/replace-all source-viewer)
      (is (= "cakes Cakes CAKES tank tank tank" (text source-viewer))))))

(deftest tab-test
  (with-code lua/lua "hi"
    (testing "basic tab"
      (tab! source-viewer)
      (is (= "\thi" (text source-viewer))))))

(deftest enter-test
  (with-code lua/lua "hi"
    (testing "basic enter"
      (enter! source-viewer)
      (is (= "\nhi" (text source-viewer))))))

(deftest undo-redo-test
  ;; Too much special setup to use the with-code macro
  (with-clean-system
    (let [pgraph-id  (g/make-graph! :history true)
          code ""
          opts lua/lua
          source-viewer (setup-source-viewer opts)
          [code-node viewer-node] (setup-code-view-nodes pgraph-id source-viewer code script/ScriptNode)]
      (testing "text editing"
        (key-typed! source-viewer "h")
        (typing-changes! source-viewer)
        (key-typed! source-viewer "i")
        (typing-changes! source-viewer)
        (is (= "hi" (g/node-value code-node :code)))
        (undo! source-viewer viewer-node code-node)
        (is (= "h" (g/node-value code-node :code)))
        (redo! source-viewer viewer-node code-node)
        (is (= "hi" (g/node-value code-node :code))))
      (testing "preferred offset"
        (text! source-viewer "helllo world")
        (preferred-offset! source-viewer 4)
        (typing-changes! source-viewer)
        (preferred-offset! source-viewer 0)
        (typing-changes! source-viewer)
        (undo! source-viewer viewer-node code-node)
        (refresh-viewer viewer-node)
        (is (= 4 (preferred-offset source-viewer))))
      (testing "snippet-tab-triggers"
        (set-code-and-caret! source-viewer "string.sub")
        (propose! source-viewer)
        (is (= "string.sub(s, i)" (text source-viewer)))
        (is (= "s" (text-selection source-viewer)))
        (typing-changes! source-viewer)
        (tab! source-viewer)
        (typing-changes! source-viewer)
        (is (= "i" (text-selection source-viewer)))
        (undo! source-viewer viewer-node code-node)
        (refresh-viewer viewer-node)
        (is (= "s" (text-selection source-viewer)))))))

(deftest tab-indents
  (with-clean-system
    (with-code lua/lua "\tlocal a = 1"
      (select-line! source-viewer)
      (tab! source-viewer)
      (is (= "local a = 1" (text source-viewer))))))
