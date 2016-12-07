(ns editor.code-view-test
  (:require [clojure.test :refer :all]
            [clojure.java.io :as io]
            [dynamo.graph :as g]
            [editor.code-view :as cv :refer :all]
            [editor.code-view-ux :as cvx :refer :all]
            [editor.handler :as handler]
            [editor.lua :as lua]
            [editor.script :as script]
            [editor.gl.shader :as shader]
            [integration.test-util :as test-util :refer [MockAppView]]
            [support.test-support :refer [with-clean-system tx-nodes]]
            [clojure.string :as str]))

(defn setup-code-view-nodes [world source-viewer code code-node-type]
  (let [[app-view code-node viewer-node] (tx-nodes
                                          (g/make-node world MockAppView)
                                          (g/make-node world code-node-type :resource (test-util/make-fake-file-resource nil "tmp" (io/file "code") ""))
                                          (g/make-node world CodeView :source-viewer source-viewer))]
    (do (g/transact (g/set-property code-node :code code))
        (setup-code-view app-view viewer-node code-node 0)
        (g/node-value viewer-node :new-content)
        [code-node viewer-node])))


(defn- select-range [source-viewer start end]
  (caret! source-viewer start false)
  (caret! source-viewer end true))

(defn- find-and-remove [s p]
  [(str/index-of s p) (str/replace s p "")])

(defn load-buffer
  [world node-type opts txt]
  (let [[caret-pos txt]         (find-and-remove txt "|")
        [start-pos txt]         (find-and-remove txt ">")
        caret-pos               (if (and start-pos (< start-pos caret-pos)) (dec caret-pos) caret-pos)
        [end-pos   txt]         (find-and-remove txt "<")
        end-pos                 (if (and end-pos (>= end-pos caret-pos)) (inc end-pos) end-pos)
        source-viewer           (setup-source-viewer opts)
        [code-node viewer-node] (setup-code-view-nodes world source-viewer txt node-type)]
    (when end-pos   (assert (>= end-pos caret-pos)))
    (when start-pos (assert (<= start-pos caret-pos)))
    (assert (not (and start-pos end-pos)))
    (assert caret-pos)
    (cond
      (and start-pos (<= start-pos caret-pos)) (select-range source-viewer start-pos caret-pos)
      (and end-pos   (<= caret-pos end-pos))   (select-range source-viewer end-pos caret-pos)
      :else                                    (caret! source-viewer caret-pos false))
    [source-viewer code-node viewer-node]))

(defn insert-at [s [i c]]
  (if i
    (str (subs s 0 i) c (subs s i))
    s))

(defn buffer-picture [source-viewer]
  (let [caret-pos  (caret source-viewer)
        start-pos  (selection-offset source-viewer)
        end-pos    (+ (selection-offset source-viewer) (selection-length source-viewer))
        insertions [(when-not (= start-pos caret-pos) [start-pos ">"])
                    [caret-pos "|"]
                    (when-not (= end-pos caret-pos)   [end-pos "<"])]]
    (reduce insert-at (text source-viewer) (reverse (sort-by first insertions)))))

(defn expect-buffer
  [source-viewer expected]
  (is (= expected (buffer-picture source-viewer))))

(defmacro should-be [& s]
  `(str/join \newline ~(vec s)))

(def ^:dynamic *code-node* nil)
(def ^:dynamic *viewer-node* nil)

(defmacro buffer-commands [init-form init-contents & cmd-expectation-forms]
  (let [viewer (gensym)]
    `(let [[~viewer cn# vn#] (~@init-form ~init-contents)]
       (binding [*code-node* cn#
                 *viewer-node* vn#]
         ~@(mapcat
            (fn [[cmd expect]]
              `[(~(first cmd) ~viewer ~@(next cmd))
                (expect-buffer ~viewer ~expect)])
            (partition 2 cmd-expectation-forms))))))

(deftest simplest-picture-test
  (with-clean-system
    (let [no-selection (str/join \newline
                                 ["line1"
                                  "line2"
                                  "line3"
                                  "li|ne4"])
          source-viewer (first (load-buffer world script/ScriptNode lua/lua no-selection))]
      (is (= no-selection (buffer-picture source-viewer))))
    (let [select-down   (str/join \newline
                                  ["line1"
                                   "li>ne2"
                                   "line|3"
                                   "line4"])
          source-viewer (first (load-buffer world script/ScriptNode lua/lua select-down))]
      (is (= select-down (buffer-picture source-viewer))))
    (let [select-up     (str/join \newline
                                  ["l|ine1"
                                   "line2"
                                   "line3"
                                   "line4<"])
          source-viewer (first (load-buffer world script/ScriptNode lua/lua select-up))]
      (is (= select-up (buffer-picture source-viewer))))))

(deftest lua-syntax
  (with-clean-system
   (let [code ""
         opts lua/lua
         source-viewer (setup-source-viewer opts)
         [code-node viewer-node] (setup-code-view-nodes world source-viewer code script/ScriptNode)]
     (testing "default style"
       (let [new-code "foo"]
         (g/transact (g/set-property code-node :code new-code))
         (g/node-value viewer-node :new-content)
         (is (= [{:start 0 :length 3 :stylename "default"}] (styles source-viewer)))))
     (testing "number style"
       (let [new-code "22"]
         (g/transact (g/set-property code-node :code new-code))
         (g/node-value viewer-node :new-content)
         (is (= [{:start 0 :length 2 :stylename "number"}] (styles source-viewer)))))
     (testing "control-flow-keyword style"
       (let [new-code "break"]
         (g/transact (g/set-property code-node :code new-code))
         (g/node-value viewer-node :new-content)
         (is (= [{:start 0 :length 5 :stylename "control-flow-keyword"}] (styles source-viewer)))))
     (testing "defold-keyword style"
       (let [new-code "update"]
         (g/transact (g/set-property code-node :code new-code))
         (g/node-value viewer-node :new-content)
         (is (= [{:start 0 :length 6 :stylename "defold-keyword"}] (styles source-viewer)))))
     (testing "operator style"
       (let [new-code "=="]
         (g/transact (g/set-property code-node :code new-code))
         (g/node-value viewer-node :new-content)
         (is (=  [{:start 0, :length 2, :stylename "operator"}] (styles source-viewer)))))
     (testing "string style"
       (let [new-code "\"foo\""]
         (g/transact (g/set-property code-node :code new-code))
         (g/node-value viewer-node :new-content)
         (is (= [{:start 0 :length 5 :stylename "string"}] (styles source-viewer)))))
     (testing "single line comment style"
       (let [new-code "--foo"]
         (g/transact (g/set-property code-node :code new-code))
         (g/node-value viewer-node :new-content)
         (is (= [{:start 0 :length 5 :stylename "comment"}] (styles source-viewer)))))
     (testing "single line comment style stays to one line"
       (let [new-code "--foo\n bar"]
         (g/transact (g/set-property code-node :code new-code))
         (g/node-value viewer-node :new-content)
         (is (= {:start 0 :length 5 :stylename "comment"} (first (styles source-viewer))))))
     (testing "multi line comment style"
       (let [new-code "--[[\nmultilinecomment\n]]"]
         (g/transact (g/set-property code-node :code new-code))
         (g/node-value viewer-node :new-content)
         (is (= [{:start 0 :length 24 :stylename "comment-multi"}] (styles source-viewer)))))
     (testing "multi line comment style terminates"
       (let [new-code "--[[\nmultilinecomment\n]] morecode"]
         (g/transact (g/set-property code-node :code new-code))
         (g/node-value viewer-node :new-content)
         (is (= {:start 0 :length 24 :stylename "comment-multi"} (first (styles source-viewer)))))))))

(deftest glsl-syntax
  (with-clean-system
   (let [code ""
         opts (:code shader/glsl-opts)
         source-viewer (setup-source-viewer opts)
         [code-node viewer-node] (setup-code-view-nodes world source-viewer code shader/ShaderNode)]
     (testing "default style"
       (let [new-code "foo"]
         (g/transact (g/set-property code-node :code new-code))
         (g/node-value viewer-node :new-content)
         (is (= [{:start 0 :length 3 :stylename "default"}] (styles source-viewer)))))
    (testing "number style"
       (let [new-code "22"]
         (g/transact (g/set-property code-node :code new-code))
         (g/node-value viewer-node :new-content)
         (is (= [{:start 0 :length 2 :stylename "number"}] (styles source-viewer)))))
    (testing "storage modifier keyword style"
      (let [new-code "uniform"]
        (g/transact (g/set-property code-node :code new-code))
        (g/node-value viewer-node :new-content)
        (is (= [{:start 0 :length 7 :stylename "storage-modifier-keyword"}] (styles source-viewer)))))
    (testing "storage type style"
      (let [new-code "vec4"]
        (g/transact (g/set-property code-node :code new-code))
        (g/node-value viewer-node :new-content)
        (is (= [{:start 0 :length 4 :stylename "storage-type"}] (styles source-viewer)))))
    (testing "operator style"
      (let [new-code ">>"]
        (g/transact (g/set-property code-node :code new-code))
        (g/node-value viewer-node :new-content)
        (is (= [{:start 0 :length 2 :stylename "operator"}] (styles source-viewer)))))
    (testing "string style"
      (let [new-code "\"foo\""]
        (g/transact (g/set-property code-node :code new-code))
        (g/node-value viewer-node :new-content)
        (is (= [{:start 0 :length 5 :stylename "string"}] (styles source-viewer)))))
     (testing "single line comment style"
       (let [new-code "//foo"]
         (g/transact (g/set-property code-node :code new-code))
         (g/node-value viewer-node :new-content)
         (is (= [{:start 0 :length 5 :stylename "comment"}] (styles source-viewer)))))
     (testing "single line comment style stays to one line"
       (let [new-code "//foo\n bar"]
         (g/transact (g/set-property code-node :code new-code))
         (g/node-value viewer-node :new-content)
         (is (= {:start 0 :length 5 :stylename "comment"} (first (styles source-viewer))))))
     (testing "multi line comment style"
       (let [new-code "/**multilinecomment\n**/"]
         (g/transact (g/set-property code-node :code new-code))
         (g/node-value viewer-node :new-content)
         (is (= [{:start 0 :length 23 :stylename "comment-multi"}] (styles source-viewer)))))
     (testing "multi line comment style terminates"
       (let [new-code "/**multilinecomment\n**/ more stuff"]
         (g/transact (g/set-property code-node :code new-code))
         (g/node-value viewer-node :new-content)
         (is (= {:start 0 :length 23 :stylename "comment-multi"} (first (styles source-viewer)))))))))
