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
  (text [this] @content))

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
      (paste! source-viewer clipboard)
      (is (= "world" (text clipboard)))
      (is (= "worldhello world" (g/node-value code-node :code))))))

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
        (g/node-value viewer-node :new-content)
        (is (= 6 (caret source-viewer)))
        (is (= 6 (g/node-value code-node :caret-position))))
      (testing "moving left"
        (left! source-viewer)
        (g/node-value viewer-node :new-content)
        (is (= 5 (caret source-viewer)))
        (is (= 5 (g/node-value code-node :caret-position))))
      (testing "out of bounds right"
        (caret! source-viewer (count code) false)
        (right! source-viewer)
        (right! source-viewer)
        (is (= (count code) (caret source-viewer))))
      (testing "out of bounds left"
        (caret! source-viewer 0 false)
        (left! source-viewer)
        (left! source-viewer)
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
       (g/node-value viewer-node :new-content)
       (is (= 2 (caret source-viewer)))
       (is (= 2 (g/node-value code-node :caret-position)))
       (is (= "he" (text-selection source-viewer))))
     (testing "selecting left"
       (select-left! source-viewer)
       (is (= 1 (caret source-viewer)))
       (is (= 1 (g/node-value code-node :caret-position)))
       (is (= "h" (text-selection source-viewer))))
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
          (g/transact (g/set-property code-node :code new-code))
          (g/node-value viewer-node :new-content)
          (caret! source-viewer 3 false)
          (preferred-offset! source-viewer 4)
          (is (= \e (get-char-at-caret source-viewer)))
          (down! source-viewer)
          (is (= \2 (get-char-at-caret source-viewer)))
          (up! source-viewer)
          (is (= \1 (get-char-at-caret source-viewer))))))))

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
    (let [code "the quick.brown\nfox"
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
        (is (= nil (get-char-at-caret source-viewer))))
      (testing "moving prev word"
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
        (is (= \t (get-char-at-caret source-viewer)))))))

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
        (is (= 11 (caret source-viewer)))))))

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
        (g/transact (g/set-property code-node :code "line1\nline2"))
        (g/node-value viewer-node :new-content)
        (text-selection! source-viewer 0 0)
        (caret! source-viewer 9 false)
        (is (= \e (get-char-at-caret source-viewer)))
        (cut! source-viewer clipboard)
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
        (g/transact (g/set-property code-node :code code))
        (g/node-value viewer-node :new-content)
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

(deftest delete-to-start-end-line
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
        (g/transact (g/set-property code-node :code code))
        (g/node-value viewer-node :new-content)
        (caret! source-viewer 7 false)
        (is (= \c (get-char-at-caret source-viewer)))
        (delete-to-start-of-line! source-viewer)
        (is (= "ck" (text source-viewer)))
        (is (= \c (get-char-at-caret source-viewer)))))))
