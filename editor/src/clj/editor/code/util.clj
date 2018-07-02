(ns editor.code.util
  (:require [clojure.string :as string])
  (:import (clojure.lang MapEntry)
           (java.util.regex Matcher Pattern)))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(def ^:private unanonymized-tokens
  #{"abstract"
    "and"
    "async"
    "await"
    "break"
    "case"
    "catch"
    "class"
    "const"
    "constexpr"
    "continue"
    "default"
    "do"
    "each"
    "elif"
    "else"
    "elseif"
    "elsif"
    "end"
    "endif"
    "enum"
    "export"
    "extends"
    "extern"
    "final"
    "finally"
    "for"
    "foreach"
    "function"
    "goto"
    "if"
    "implements"
    "import"
    "in"
    "interface"
    "let"
    "local"
    "mutable"
    "namespace"
    "native"
    "noexcept"
    "not"
    "operator"
    "or"
    "package"
    "private"
    "protected"
    "public"
    "register"
    "repeat"
    "require"
    "requires"
    "return"
    "static"
    "struct"
    "switch"
    "synchronized"
    "template"
    "then"
    "throw"
    "throws"
    "transient"
    "try"
    "typedef"
    "typename"
    "union"
    "until"
    "using"
    "var"
    "volatile"
    "while"
    "with"
    "yield"})

(defn- anonymize-token
  ^String [^String token]
  (if (contains? unanonymized-tokens token)
    token
    (let [length (count token)]
      (loop [index 0
             builder (StringBuilder. length)]
        (if (= length index)
          (.toString builder)
          (let [original-glyph (.codePointAt token index)
                scrambled-glyph (if (Character/isLetter original-glyph)
                                  (if (Character/isUpperCase original-glyph)
                                    (int \A)
                                    (int \a))
                                  (if (Character/isDigit original-glyph)
                                    (int \0)
                                    original-glyph))]
            (recur (inc index)
                   (.appendCodePoint builder scrambled-glyph))))))))

(defn anonymize-line
  "Given a line of code, replace all identifying information such as
  strings, numbers and variable names with zeroes or the letter 'A'."
  ^String [^String line]
  (let [length (count line)]
    (loop [index 0
           line-builder (StringBuilder. length)
           token-builder (StringBuilder. length)]
      (if (= length index)
        (.toString (.append line-builder (anonymize-token (.toString token-builder))))
        (let [glyph (.codePointAt line index)]
          (if (Character/isLetterOrDigit glyph)
            (recur (inc index)
                   line-builder
                   (.appendCodePoint token-builder glyph))
            (recur (inc index)
                   (doto line-builder
                     (.append (anonymize-token (.toString token-builder)))
                     (.appendCodePoint glyph))
                   (doto token-builder
                     (.setLength 0)))))))))

(defn- comparisons-syntax [exp & rest-exps]
  (if (empty? rest-exps)
    exp
    (let [comparison-sym (gensym "comparison")]
      `(let [~comparison-sym ~exp]
         (if (zero? ~comparison-sym)
           ~(apply comparisons-syntax rest-exps)
           ~comparison-sym)))))

(defmacro comparisons [& exps]
  (apply comparisons-syntax exps))

(defn pair
  "Returns a two-element collection that implements IPersistentVector."
  [a b]
  (MapEntry/create a b))

(defn re-matcher-from
  "Returns an instance of java.util.regex.Matcher that starts at an offset."
  ^Matcher [re s start]
  (let [whole-string-matcher (re-matcher re s)]
    (if (> ^long start 0)
      (.region whole-string-matcher start (count s))
      whole-string-matcher)))

(defn re-match-result-seq
  "Returns a lazy sequence of successive MatchResults of the pattern in the
  specified string, using java.util.regex.Matcher.find(), with an optional
  start offset into the string."
  ([^Pattern re s]
   (re-match-result-seq re s 0))
  ([^Pattern re s ^long start]
   (let [m (re-matcher-from re s start)]
     ((fn step []
        (when (. m (find))
          (cons (.toMatchResult m) (lazy-seq (step)))))))))

(defn split-lines
  "Splits s on \\n or \\r\\n. Contrary to string/split-lines, keeps trailing newlines."
  [text]
  (string/split text #"\r?\n" -1))
