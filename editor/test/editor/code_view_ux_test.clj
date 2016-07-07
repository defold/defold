(ns editor.code-view-ux-test
  (:require [clojure.test :refer :all]
            [dynamo.graph :as g]
            [editor.code-view :as cv :refer :all]
            [editor.code-view-ux :as cvx :refer :all]
            [editor.handler :as handler]
            [editor.lua :as lua]
            [editor.script :as script]
            [editor.code-view-test :as cvt :refer [setup-code-view-nodes]]
            [support.test-support :refer [with-clean-system tx-nodes]]))

(defrecord TestClipboard [content]
  TextContainer
  (text! [this s] (reset! content s))
  (text [this] @content)
  (replace! [this offset length s]))

(defn- key-typed! [source-viewer key-typed]
  (handler/run :key-typed [{:name :code-view :env {:selection source-viewer :key-typed key-typed}}] {}))

(deftest key-typed-test
  (with-clean-system
    (let [code "hello"
          opts lua/lua
          source-viewer (setup-source-viewer opts false)
          [code-node viewer-node] (setup-code-view-nodes world source-viewer code script/ScriptNode)]
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
        (is (= "bhello" (text source-viewer)))))))

(defn- copy! [source-viewer clipboard]
  (handler/run :copy [{:name :code-view :env {:selection source-viewer :clipboard clipboard}}] {}))

(defn- paste! [source-viewer clipboard]
  (handler/run :paste [{:name :code-view :env {:selection source-viewer :clipboard clipboard}}]{}))

(deftest copy-paste-test
  (with-clean-system
    (let [clipboard (new TestClipboard (atom ""))
          code "hello world"
          opts lua/lua
          source-viewer (setup-source-viewer opts false)
          [code-node viewer-node] (setup-code-view-nodes world source-viewer code script/ScriptNode)]
      (text-selection! source-viewer 6 5)
      (copy! source-viewer clipboard)
      (testing "pasting without text selected"
        (text-selection! source-viewer 0 0)
        (paste! source-viewer clipboard)
        (is (= "world" (text clipboard)))
        (is (= "worldhello world" (text source-viewer))))
      (testing "pasting with text selected"
        (text-selection! source-viewer 0 10)
        (paste! source-viewer clipboard)
        (is (= "world world" (text source-viewer)))))))

(defn- right! [source-viewer]
  (handler/run :right [{:name :code-view :env {:selection source-viewer}}]{}))

(defn- left! [source-viewer]
  (handler/run :left [{:name :code-view :env {:selection source-viewer}}]{}))

(deftest move-right-left-test
  (with-clean-system
    (let [code "hello world"
          opts lua/lua
          source-viewer (setup-source-viewer opts false)
          [code-node viewer-node] (setup-code-view-nodes world source-viewer code script/ScriptNode)]
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
      (testing "with chunk of selected text, right takes cursor to end of chunk"
        (caret! source-viewer 0 false)
        (text-selection! source-viewer 0 5)
        (is (= 0 (caret source-viewer)))
        (is (= "hello" (text-selection source-viewer)))
        (right! source-viewer)
        (is (= "" (text-selection source-viewer)))
        (is (= 5 (caret source-viewer))))
      (testing "with chunk of selected text, left takes cursor to beginning of chunk"
        (caret! source-viewer 5 false)
        (text-selection! source-viewer 0 5)
        (is (= 5 (caret source-viewer)))
        (is (= "hello" (text-selection source-viewer)))
        (left! source-viewer)
        (is (= "" (text-selection source-viewer)))
        (is (= 0 (caret source-viewer)))))))

(defn- select-right! [source-viewer]
  (handler/run :select-right [{:name :code-view :env {:selection source-viewer}}]{}))

(defn- select-left! [source-viewer]
  (handler/run :select-left [{:name :code-view :env {:selection source-viewer}}]{}))

(deftest select-move-right-left-test
  (with-clean-system
   (let [code "hello world"
         opts lua/lua
         source-viewer (setup-source-viewer opts false)
         [code-node viewer-node] (setup-code-view-nodes world source-viewer code script/ScriptNode)]
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
       (is (= 0 (caret source-viewer)))))))

(defn- up! [source-viewer]
  (handler/run :up [{:name :code-view :env {:selection source-viewer}}]{}))

(defn- down! [source-viewer]
  (handler/run :down [{:name :code-view :env {:selection source-viewer}}]{}))

(defn- get-char-at-caret [source-viewer]
  (get (text source-viewer) (caret source-viewer)))

(deftest move-up-down-test
  (with-clean-system
    (let [code "line1\nline2\n\nline3and"
          opts lua/lua
          source-viewer (setup-source-viewer opts false)
          [code-node viewer-node] (setup-code-view-nodes world source-viewer code script/ScriptNode)]
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
      (down! source-viewer)
      (is (nil? (get-char-at-caret source-viewer)))
      (testing "out of bounds down"
        (down! source-viewer)
        (down! source-viewer)
        (is (nil? (get-char-at-caret source-viewer))))
      (testing "moving up"
        (up! source-viewer)
        (is (= \newline (get-char-at-caret source-viewer)))
        (up! source-viewer)
        (is (= \2 (get-char-at-caret source-viewer)))
        (up! source-viewer)
        (is (= \1 (get-char-at-caret source-viewer)))
        (up! source-viewer)
        (is (= \l (get-char-at-caret source-viewer))))
      (testing "out of bounds up"
        (up! source-viewer)
        (is (= \l (get-char-at-caret source-viewer))))
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
        (is (= 0 (caret source-viewer)))))))

(defn- select-up! [source-viewer]
  (handler/run :select-up [{:name :code-view :env {:selection source-viewer}}]{}))

(defn- select-down! [source-viewer]
  (handler/run :select-down [{:name :code-view :env {:selection source-viewer}}]{}))

(deftest select-move-up-down-test
  (with-clean-system
    (let [code "line1\nline2"
          opts lua/lua
          source-viewer (setup-source-viewer opts false)
          [code-node viewer-node] (setup-code-view-nodes world source-viewer code script/ScriptNode)]
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
        (is (= "line" (text-selection source-viewer)))))))

(defn- next-word! [source-viewer]
  (handler/run :next-word [{:name :code-view :env {:selection source-viewer}}]{}))

(defn- prev-word! [source-viewer]
  (handler/run :prev-word [{:name :code-view :env {:selection source-viewer}}]{}))

(deftest word-move-test
  (with-clean-system
    (let [code "the quick.brown\nfox_test"
          opts lua/lua
          source-viewer (setup-source-viewer opts false)
          [code-node viewer-node] (setup-code-view-nodes world source-viewer code script/ScriptNode)]
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
        (is (= \a (get-char-at-caret source-viewer)))))))

(defn- select-next-word! [source-viewer]
  (handler/run :select-next-word [{:name :code-view :env {:selection source-viewer}}]{}))

(defn- select-prev-word! [source-viewer]
  (handler/run :select-prev-word [{:name :code-view :env {:selection source-viewer}}]{}))

(deftest select-word-move-test
  (with-clean-system
    (let [code "the quick"
          opts lua/lua
          source-viewer (setup-source-viewer opts false)
          [code-node viewer-node] (setup-code-view-nodes world source-viewer code script/ScriptNode)]
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
        (is (= "" (text-selection source-viewer)))))))

(defn- line-begin! [source-viewer]
  (handler/run :line-begin [{:name :code-view :env {:selection source-viewer}}]{}))

(defn- line-end! [source-viewer]
  (handler/run :line-end [{:name :code-view :env {:selection source-viewer}}]{}))

(deftest line-begin-end-test
  (with-clean-system
    (let [code "hello world"
          opts lua/lua
          source-viewer (setup-source-viewer opts false)
          [code-node viewer-node] (setup-code-view-nodes world source-viewer code script/ScriptNode)]
      (testing "moving to beginning of the line"
        (caret! source-viewer 4 false)
        (is (= \o (get-char-at-caret source-viewer)))
        (line-begin! source-viewer)
        (is (= \h (get-char-at-caret source-viewer)))
        (line-begin! source-viewer)
        (is (= \h (get-char-at-caret source-viewer))))
      (testing "moving to the end of the line"
        (caret! source-viewer 4 false)
        (is (= \o (get-char-at-caret source-viewer)))
        (line-end! source-viewer)
        (is (= nil (get-char-at-caret source-viewer)))
        (is (= 11 (caret source-viewer)))
        (line-end! source-viewer)
        (is (= 11 (caret source-viewer))))
      (testing "moving remembers col position"
        (text! source-viewer "line1\nline2")
        (caret! source-viewer 0 false)
        (line-end! source-viewer)
        (down! source-viewer)
        (is (= 11 (caret source-viewer)))
        (line-begin! source-viewer)
        (up! source-viewer)
        (is (= 0 (caret source-viewer)))))))

(defn- select-line-begin! [source-viewer]
  (handler/run :select-line-begin [{:name :code-view :env {:selection source-viewer}}]{}))

(defn- select-line-end! [source-viewer]
  (handler/run :select-line-end [{:name :code-view :env {:selection source-viewer}}]{}))

(deftest select-line-begin-end-test
  (with-clean-system
    (let [code "hello world"
          opts lua/lua
          source-viewer (setup-source-viewer opts false)
          [code-node viewer-node] (setup-code-view-nodes world source-viewer code script/ScriptNode)]
      (testing "selecting to beginning of the line"
        (caret! source-viewer 4 false)
        (is (= \o (get-char-at-caret source-viewer)))
        (select-line-begin! source-viewer)
        (is (= "hell" (text-selection source-viewer))))
      (testing "selecting to the end of the line"
        (caret! source-viewer 4 false)
        (is (= \o (get-char-at-caret source-viewer)))
        (select-line-end! source-viewer)
        (is (= "o world" (text-selection source-viewer)))))))

(defn- file-begin! [source-viewer]
  (handler/run :file-begin [{:name :code-view :env {:selection source-viewer}}]{}))

(defn- file-end! [source-viewer]
  (handler/run :file-end [{:name :code-view :env {:selection source-viewer}}]{}))

(deftest file-begin-end-test
  (with-clean-system
    (let [code "hello\nworld"
          opts lua/lua
          source-viewer (setup-source-viewer opts false)
          [code-node viewer-node] (setup-code-view-nodes world source-viewer code script/ScriptNode)]
      (testing "moving to beginning of the file"
        (caret! source-viewer 4 false)
        (is (= \o (get-char-at-caret source-viewer)))
        (file-begin! source-viewer)
        (is (= 0 (caret source-viewer))))
      (testing "moving to the end of the file"
        (caret! source-viewer 4 false)
        (is (= \o (get-char-at-caret source-viewer)))
        (file-end! source-viewer)
        (is (= 11 (caret source-viewer)))))))

(defn- select-file-begin! [source-viewer]
  (handler/run :select-file-begin [{:name :code-view :env {:selection source-viewer}}]{}))

(defn- select-file-end! [source-viewer]
  (handler/run :select-file-end [{:name :code-view :env {:selection source-viewer}}]{}))

(deftest select-file-begin-end-test
  (with-clean-system
    (let [code "hello\nworld"
          opts lua/lua
          source-viewer (setup-source-viewer opts false)
          [code-node viewer-node] (setup-code-view-nodes world source-viewer code script/ScriptNode)]
      (testing "selecting to the end of the file"
        (caret! source-viewer 4 false)
        (is (= \o (get-char-at-caret source-viewer)))
        (select-file-end! source-viewer)
        (is (= "o\nworld" (text-selection source-viewer))))
      (testing "selecting to the beginning of the file"
        (caret! source-viewer 8 false)
        (is (= \r (get-char-at-caret source-viewer)))
        (select-file-begin! source-viewer)
        (is (= "hello\nwo" (text-selection source-viewer)))))))

(defn- go-to-line! [source-viewer line-number]
  ;; bypassing handler for the dialog handling
  (go-to-line source-viewer line-number))

(deftest goto-line-test
  (with-clean-system
    (let [code "a\nb\nc"
          opts lua/lua
          source-viewer (setup-source-viewer opts false)
          [code-node viewer-node] (setup-code-view-nodes world source-viewer code script/ScriptNode)]
      (testing "goto line"
        (go-to-line! source-viewer "2")
        (is (= \b (get-char-at-caret source-viewer)))
        (go-to-line! source-viewer "3")
        (is (= \c (get-char-at-caret source-viewer)))
        (go-to-line! source-viewer "1")
        (is (= \a (get-char-at-caret source-viewer))))
      (testing "goto line bounds"
        (go-to-line! source-viewer "0")
        (is (= \a (get-char-at-caret source-viewer)))
        (go-to-line! source-viewer "-2")
        (is (= \a (get-char-at-caret source-viewer)))
        (go-to-line! source-viewer "100")
        (is (= nil (get-char-at-caret source-viewer)))
        (is (= 5 (caret source-viewer)))))))

(defn- select-word! [source-viewer]
  (handler/run :select-word [{:name :code-view :env {:selection source-viewer}}]{}))

(deftest select-word-test
  (with-clean-system
    (let [code "blue  cat"
          opts lua/lua
          source-viewer (setup-source-viewer opts false)
          [code-node viewer-node] (setup-code-view-nodes world source-viewer code script/ScriptNode)]
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
        (is (= "" (text-selection source-viewer)))))))

(defn- select-line! [source-viewer]
  (handler/run :select-line [{:name :code-view :env {:selection source-viewer}}]{}))

(deftest select-line-test
  (with-clean-system
    (let [code "line1\nline2"
          opts lua/lua
          source-viewer (setup-source-viewer opts false)
          [code-node viewer-node] (setup-code-view-nodes world source-viewer code script/ScriptNode)]
      (testing "selects the line"
        (select-line! source-viewer)
        (is (= "line1" (text-selection source-viewer)))
        (caret! source-viewer 7 false)
        (select-line! source-viewer)
        (is (= "line2" (text-selection source-viewer)))))))

(defn- select-all! [source-viewer]
  (handler/run :select-all [{:name :code-view :env {:selection source-viewer}}]{}))

(deftest select-all-test
  (with-clean-system
    (let [code "hello there"
          opts lua/lua
          source-viewer (setup-source-viewer opts false)
          [code-node viewer-node] (setup-code-view-nodes world source-viewer code script/ScriptNode)]
      (testing "selects the entire doc"
        (select-all! source-viewer)
        (is (= "hello there" (text-selection source-viewer)))))))

(defn- delete! [source-viewer]
  (handler/run :delete [{:name :code-view :env {:selection source-viewer}}]{}))

(deftest delete-test
  (with-clean-system
    (let [code "blue"
          opts lua/lua
          source-viewer (setup-source-viewer opts false)
          [code-node viewer-node] (setup-code-view-nodes world source-viewer code script/ScriptNode)]
      (testing "deleting"
        (caret! source-viewer 3 false)
        (delete! source-viewer)
        (is (= \e (get-char-at-caret source-viewer)))
        (is (= "ble" (text source-viewer))))
      (testing "deleting with highlighting selection"
        (caret! source-viewer 0 false)
        (text-selection! source-viewer 0 2)
        (delete! source-viewer)
        (is (= "e" (text source-viewer)))))))

(defn- cut! [source-viewer clipboard]
  (handler/run :cut [{:name :code-view :env {:selection source-viewer :clipboard clipboard}}]{}))

(deftest cut-test
  (with-clean-system
    (let [clipboard (new TestClipboard (atom ""))
          code "blue duck"
          opts lua/lua
          source-viewer (setup-source-viewer opts false)
          [code-node viewer-node] (setup-code-view-nodes world source-viewer code script/ScriptNode)]
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

(defn- delete-prev-word! [source-viewer]
  (handler/run :delete-prev-word [{:name :code-view :env {:selection source-viewer}}]{}))

(defn- delete-next-word! [source-viewer]
  (handler/run :delete-next-word [{:name :code-view :env {:selection source-viewer}}]{}))

(deftest delete-by-word
  (with-clean-system
    (let [code "blue duck"
          opts lua/lua
          source-viewer (setup-source-viewer opts false)
          [code-node viewer-node] (setup-code-view-nodes world source-viewer code script/ScriptNode)]
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
        (is (= nil  (get-char-at-caret source-viewer)))))))

(defn- delete-to-end-of-line! [source-viewer]
  (handler/run :delete-to-end-of-line [{:name :code-view :env {:selection source-viewer}}]{}))

(defn- delete-to-start-of-line! [source-viewer]
  (handler/run :delete-to-start-of-line [{:name :code-view :env {:selection source-viewer}}]{}))

(deftest delete-to-start-end-line-test
  (with-clean-system
    (let [code "blue duck"
          opts lua/lua
          source-viewer (setup-source-viewer opts false)
          [code-node viewer-node] (setup-code-view-nodes world source-viewer code script/ScriptNode)]
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
        (delete-to-start-of-line! source-viewer)
        (is (= "ck" (text source-viewer)))
        (is (= \c (get-char-at-caret source-viewer)))))))

(defn- cut-to-end-of-line! [source-viewer clipboard]
  (handler/run :cut-to-end-of-line [{:name :code-view :env {:selection source-viewer :clipboard clipboard}}]{}))

(deftest cut-to-end-line-test
  (with-clean-system
    (let [code "line1\nline2\nline3"
          clipboard (new TestClipboard (atom ""))
          opts lua/lua
          source-viewer (setup-source-viewer opts false)
          [code-node viewer-node] (setup-code-view-nodes world source-viewer code script/ScriptNode)]
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

(defn- find-text! [source-viewer text]
  ;; bypassing handler for the dialog handling
  (find-text source-viewer text))

(deftest find-text-test
  (with-clean-system
    (let [code "the blue ducks"
          opts lua/lua
          source-viewer (setup-source-viewer opts false)
          [code-node viewer-node] (setup-code-view-nodes world source-viewer code script/ScriptNode)]
      (testing "find match"
        (find-text! source-viewer "the")
        (is (= "the" (text-selection source-viewer)))
        (is (= \space (get-char-at-caret source-viewer)))
        (find-text! source-viewer "duck")
        (is (= "duck" (text-selection source-viewer)))
        (is (= \s (get-char-at-caret source-viewer)))
        (testing "no match"
          (find-text! source-viewer "dog")
          (is (= "duck" (text-selection source-viewer)))
          (is (= \s (get-char-at-caret source-viewer))))))))

(defn- find-next! [source-viewer]
  (handler/run :find-next [{:name :code-view :env {:selection source-viewer}}]{}))

(defn- find-prev! [source-viewer]
  (handler/run :find-prev [{:name :code-view :env {:selection source-viewer}}]{}))

(deftest find-next-prev-test
  (with-clean-system
    (let [code "duck1 duck2 duck3"
          opts lua/lua
          source-viewer (setup-source-viewer opts false)
          [code-node viewer-node] (setup-code-view-nodes world source-viewer code script/ScriptNode)]
      (testing "find-next"
        (find-text! source-viewer "duck")
        (is (= "duck" (text-selection source-viewer)))
        (is (= \1 (get-char-at-caret source-viewer)))
        (find-next! source-viewer)
        (is (= "duck" (text-selection source-viewer)))
        (is (= \2 (get-char-at-caret source-viewer)))
        (find-next! source-viewer)
        (is (= "duck" (text-selection source-viewer)))
        (is (= \3 (get-char-at-caret source-viewer)))
        (testing "bounds"
          (find-next! source-viewer)
          (is (= "duck" (text-selection source-viewer)))
          (is (= \3 (get-char-at-caret source-viewer)))))
      (testing "find-prev"
        (find-prev! source-viewer)
        (is (= "duck" (text-selection source-viewer)))
        (is (= \2 (get-char-at-caret source-viewer)))
        (find-prev! source-viewer)
        (is (= "duck" (text-selection source-viewer)))
        (is (= \1 (get-char-at-caret source-viewer)))
        (testing "bounds"
          (find-prev! source-viewer)
          (is (= "duck" (text-selection source-viewer)))
          (is (= \1 (get-char-at-caret source-viewer))))))))

(defn- replace-text! [source-viewer text]
  ;; bypassing handler for the dialog handling
  (replace-text source-viewer text))

(deftest replace-text-test
  (with-clean-system
    (let [code "the blue ducks"
          opts lua/lua
          source-viewer (setup-source-viewer opts false)
          [code-node viewer-node] (setup-code-view-nodes world source-viewer code script/ScriptNode)]
      (testing "replacing match"
        (replace-text! source-viewer {:find-text "blue" :replace-text "red"})
        (is (= "the red ducks" (text source-viewer)))
        (is (= 7 (caret source-viewer))))
      (testing "replacing no match"
        (replace-text! source-viewer {:find-text "cake" :replace-text "foo"})
        (is (= "the red ducks" (text source-viewer)))
        (is (= 7 (caret source-viewer)))))))

(defn- replace-next! [source-viewer]
  (handler/run :replace-next [{:name :code-view :env {:selection source-viewer}}]{}))

(deftest replace-next-test
  (with-clean-system
    (let [code "the blue blue ducks"
          opts lua/lua
          source-viewer (setup-source-viewer opts false)
          [code-node viewer-node] (setup-code-view-nodes world source-viewer code script/ScriptNode)]
      (testing "replacing match"
        (replace-text! source-viewer {:find-text "blue" :replace-text "red"})
        (is (= "the red blue ducks" (text source-viewer)))
        (is (= 7 (caret source-viewer)))
        (replace-next! source-viewer)
        (is (= "the red red ducks" (text source-viewer)))
        (is (= 11 (caret source-viewer))))
      (testing "when caret is beyond match, will not replace"
        (text! source-viewer code)
        (caret! source-viewer 7 false)
        (replace-next! source-viewer)
        (is (= "the blue red ducks" (text source-viewer)))
        (is (= 12 (caret source-viewer)))))))

(defn- tab! [source-viewer]
  (handler/run :tab [{:name :code-view :env {:selection source-viewer}}]{}))

(deftest tab-test
  (with-clean-system
    (let [code "hi"
          opts lua/lua
          source-viewer (setup-source-viewer opts false)
          [code-node viewer-node] (setup-code-view-nodes world source-viewer code script/ScriptNode)]
      (testing "basic tab"
        (tab! source-viewer)
        (is (= "\thi" (text source-viewer)))))))

(defn- enter! [source-viewer]
  (handler/run :enter [{:name :code-view :env {:selection source-viewer}}]{}))

(deftest enter-test
  (with-clean-system
    (let [code "hi"
          opts lua/lua
          source-viewer (setup-source-viewer opts false)
          [code-node viewer-node] (setup-code-view-nodes world source-viewer code script/ScriptNode)]
      (testing "basic enter"
        (enter! source-viewer)
        (is (= "\nhi" (text source-viewer)))))))

(defn- undo! [source-viewer view-node code-node]
  (handler/run :undo [{:name :code-view :env {:selection source-viewer :view-node view-node :code-node code-node}}]{}))

(defn- redo! [source-viewer view-node code-node]
  (handler/run :redo [{:name :code-view :env {:selection source-viewer :view-node view-node :code-node code-node}}]{}))

(defn- set-code-and-caret! [source-viewer code]
  (text! source-viewer code)
  (caret! source-viewer (count code) false)
  (changes! source-viewer))

(defn- propose! [source-viewer]
  (handler/run :proposals [{:name :code-view :env {:selection source-viewer}}]{}))

(deftest undo-redo-test
  (with-clean-system
    (let [pgraph-id  (g/make-graph! :history true)
          code ""
          opts lua/lua
          source-viewer (setup-source-viewer opts false)
          [code-node viewer-node] (setup-code-view-nodes pgraph-id source-viewer code script/ScriptNode)]
      (testing "text editing"
        (key-typed! source-viewer "h")
        (changes! source-viewer)
        (key-typed! source-viewer "i")
        (changes! source-viewer)
        (is (= "hi" (g/node-value code-node :code)))
        (undo! source-viewer viewer-node code-node)
        (is (= "h" (g/node-value code-node :code)))
        (redo! source-viewer viewer-node code-node)
        (is (= "hi" (g/node-value code-node :code))))
      (testing "preferred offset"
        (text! source-viewer "helllo world")
        (preferred-offset! source-viewer 4)
        (changes! source-viewer)
        (preferred-offset! source-viewer 0)
        (changes! source-viewer)
        (undo! source-viewer viewer-node code-node)
        (is (= 4 (preferred-offset source-viewer))))
      (testing "snippet-tab-triggers"
        (set-code-and-caret! source-viewer "string.sub")
        (propose! source-viewer)
        (is (= "string.sub(s,i)" (text source-viewer)))
        (is (= "s" (text-selection source-viewer)))
        (changes! source-viewer)
        (tab! source-viewer)
        (changes! source-viewer)
        (is (= "i" (text-selection source-viewer)))
        (undo! source-viewer viewer-node code-node)
        (is (= "s" (text-selection source-viewer)))))))
