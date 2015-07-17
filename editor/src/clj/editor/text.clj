(ns editor.text
  (:require [clojure.java.io :as io]
            [clojure.string :as str]
            [dynamo.graph :as g]
            [editor.core :as core]
            [editor.resource :as resource]
            [editor.ui :as ui]
            [editor.workspace :as workspace]
            [instaparse.core :as insta])
  (:import [javafx.scene Parent]
           [javafx.scene.control TextArea]
           [javafx.scene.layout Pane]
           [java.util Collections]
           [java.util.regex Pattern]
           [org.fxmisc.richtext CodeArea LineNumberFactory StyleSpansBuilder]))

(defn span [cl start len] [cl start len])
(defn default-span [start len] [:default start len])

(def empty-spanlist {:last 0 :spans []})

(defn add-span
  ([spanlist]
   spanlist)
  ([spanlist [cl start len]]
   (cond
     (= start (:last spanlist))
     (-> spanlist
         (update :spans conj (span cl start len))
         (assoc  :last  (+ start len)))

     (and (< start (:last spanlist)) (< (:last spanlist) (+ len start)))
     (-> spanlist
         (update :spans conj (span cl (:last spanlist) (- len (:last spanlist))))
         (update :last max (+ start len)))

     (> start (:last spanlist))
     (-> spanlist
         (update :spans conj (default-span (:last spanlist) (- start (:last spanlist))))
         (update :spans conj (span cl start len))
         (assoc  :last (+ start len))))))

(defn pad-to-length [spanlist len]
  (if (<= len (:last spanlist))
    spanlist
    (-> spanlist
        (update :spans conj (default-span (:last spanlist) (- len (:last spanlist))))
        (assoc :last len))))

;; ----------------------------------------
;; Built in syntax parsers
;; ----------------------------------------

(def java-keywords ["abstract" "assert" "boolean" "break" "byte"
                    "case" "catch" "char" "class" "const"
                    "continue" "default" "do" "double" "else"
                    "enum" "extends" "final" "finally" "float"
                    "for" "goto" "if" "implements" "import"
                    "instanceof" "int" "interface" "long" "native"
                    "new" "package" "private" "protected" "public"
                    "return" "short" "static" "strictfp" "super"
                    "switch" "synchronized" "this" "throw" "throws"
                    "transient" "try" "void" "volatile" "while"])

(def java-syntax {"keyword"   (str "\\b(" (str/join "|" java-keywords) ")\\b")
                  "string"    "\"([^\"\\\\]|\\\\.)*\""
                  "comment"   "//[^\n]*|/\\*(.|\\R)*?\\*/"})

(def lua-p (insta/parser (io/resource "lua.ebnf") :auto-whitespace :standard))

;; ----------------------------------------
;; EBNF style parsing
;; ----------------------------------------
(def known-nodes {:Name          "variable"
                  :Numeral       "constant"
                  :Comment       "comment"
                  :LiteralString "string"
                  :Keyword       "keyword"})

(defn ast-seq
  [ast]
  (tree-seq vector? (partial drop 1) ast))

(defn ast-node->span
  [[class & _ :as node]]
  (let [[start end] (insta/span node)]
    [(known-nodes (first node)) start (- end start)]))

(defn literal->keyword
  [s]
  (if (string? s)
    [:Keyword s]
    s))

(def ast-seq->spans
  (comp
   (remove insta/failure?)
   (filter meta)
   (map literal->keyword)
   (filter #(known-nodes (first %)))
   (map ast-node->span)))

(defn instaparse
  "Uses the given parser to parse text. Every node in the resulting
  AST gets converted to a span, where the rule name becomes the
  style. This may create a lot of spans. If so, check out Instaparse's
  syntax for hiding nodes and mark up your grammar accordingly."
  [parser new-text]
  (let [ast   (parser new-text :total true)
        ast-s (ast-seq ast)]
    (transduce ast-seq->spans add-span empty-spanlist ast-s)))

;; ----------------------------------------
;; Regex style parsing
;; ----------------------------------------
(defn compile-pattern [pattern-map]
  (let [grouper (fn [[kind pattern]] (format "(?<%s>%s)" (name kind) pattern))
        all-groups (str/join "|" (map grouper pattern-map))]
    (Pattern/compile all-groups)))

(defn- class-for
  [patterns matcher]
  (loop [classes (keys patterns)]
    (when (seq classes)
      (let [class (first classes)]
        (if (not (nil? (.group matcher (name class))))
          class
          (recur (next classes)))))))

(defn- covers-any? [start last] (not= 0 (- last start)))

(defn regex-parse
  "Returns [class start end] for each span in the string. The
  class :default indicates no particular style for that region. Always
  returns at least one span. In the degenerate case of no matches,
  will return [[:default 0 (count text)]]."
  [patterns text]
  (let [matcher (re-matcher (compile-pattern patterns) text)]
    (loop [spanlist    empty-spanlist
           next-match  (.find matcher)]
      (if-not next-match
        (pad-to-length spanlist (count text))
        (let [start (.start matcher)
              end   (.end matcher)]
          (recur (add-span spanlist [(class-for patterns matcher) start (- end start)]) (.find matcher)))))))

;; ----------------------------------------
;; Common highlighting tasks
;; ----------------------------------------
(def ^{:doc "Associate file extensions to syntax functions."}
  ext->syntax
  {"java"   (partial regex-parse java-syntax)
   "script" (partial instaparse lua-p)})

(defn node->syntax
  [resource-node]
  (-> resource-node :resource resource/resource-type :ext ext->syntax))

(defn highlighting-for
  "Turns span data structures into the StyleSpans object needed by the
  CodeArea control"
  [{spans :spans}]
  (let [builder (StyleSpansBuilder.)]
    (doseq [[style start len] spans]
      (.add builder
            (if (= :default style)
              (Collections/emptyList)
              (Collections/singleton (name style)))
            len))
    (.create builder)))

(defn do-highlighting
  [text-area syntax observable old-text new-text]
  (try
    (let [spanlist (syntax new-text)]
      (if (< 0 (count (:spans spanlist)))
        (.setStyleSpans text-area 0 (highlighting-for spanlist))
        (.clearStyle text-area 0 (count new-text))))
    (catch Exception e
      (println 'editor.text/do-highlighting "Exception during syntax highlighting parse" e)
      (.clearStyle text-area 0 (count new-text)))))

;; ----------------------------------------
;; Node definition
;; ----------------------------------------
(g/defnode TextView
  (inherits core/ResourceNode)

  (property text-area CodeArea))

(defn make-view [graph ^Parent parent resource-node opts]
  (let [text-area (CodeArea.)]
    (.setParagraphGraphicFactory text-area (LineNumberFactory/get text-area))
    (when-let [syntax (node->syntax resource-node)]
      (ui/observe (.textProperty text-area) (partial do-highlighting text-area syntax)))
    (.replaceText text-area 0 0 (slurp (:resource resource-node)))
    (.add (.getChildren ^Pane parent) text-area)
    (ui/fill-control text-area)
    (g/make-node! graph TextView :text-area text-area)))

(defn register-view-types [workspace]
  (workspace/register-view-type workspace
                                :id :text
                                :make-view-fn make-view))
