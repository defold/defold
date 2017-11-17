(ns editor.text-file
  (:require
   [clojure.string :as str]
   [dynamo.graph :as g]
   [editor.code :as code]
   [editor.types :as t]
   [editor.graph-util :as gu]
   [editor.defold-project :as project]
   [editor.properties :as properties]
   [editor.workspace :as workspace]
   [editor.resource :as resource]
   [editor.resource-node :as resource-node]
   [util.text-util :as text-util])
  (:import
   (java.util.regex Pattern)))

(set! *warn-on-reflection* true)

(defn- re-quote [s] (Pattern/quote s))

(defn- set->pattern
  [s]
  (re-pattern (str "^(?:"
                   (clojure.string/join "|" (map re-quote (reverse (sort s))))
                   ")")))


;;; Rudimentary syntax highlightning support for C/C++

(def cpp-preprocessor-directives
  #{"#assert"
    "#define"
    "#elif"
    "#else"
    "#endif"
    "#error"
    "#ident"
    "#if"
    "#ifdef"
    "#ifndef"
    "#import"
    "#include"
    "#line"
    "#pragma"
    "#sccs"
    "#unassert"
    "#undef"
    "#warning"})

(def cpp-keywords
  #{"alignas"
    "alignof"
    "asm"
    "auto"
    "bool"
    "break"
    "case"
    "catch"
    "char"
    "char16_t"
    "char32_t"
    "class"
    "const"
    "const_cast"
    "constexpr"
    "continue"
    "decltype"
    "default"
    "delete"
    "do"
    "double"
    "dynamic_cast"
    "else"
    "enum"
    "explicit"
    "export"
    "extern"
    "false"
    "float"
    "for"
    "friend"
    "goto"
    "if"
    "inline"
    "int"
    "long"
    "mutable"
    "namespace"
    "new"
    "noexcept"
    "nullptr"
    "operator"
    "private"
    "protected"
    "public"
    "register"
    "reinterpret_cast"
    "return"
    "short"
    "signed"
    "sizeof"
    "static"
    "static_assert"
    "static_cast"
    "struct"
    "switch"
    "template"
    "this"
    "thread_local"
    "throw"
    "true"
    "try"
    "typedef"
    "typeid"
    "typename"
    "union"
    "unsigned"
    "using"
    "virtual"
    "void"
    "volatile"
    "wchar_t"
    "while"})

(def cpp-operators
  #{"!"
    "!="
    "#"
    "##"
    "%"
    "%:"
    "%:%:"
    "%="
    "%>"
    "&"
    "&&"
    "&="
    "("
    ")"
    "*"
    "*="
    "+"
    "++"
    "+="
    ","
    "-"
    "--"
    "-="
    "->"
    "->*"
    "."
    ".*"
    "..."
    "/"
    "/="
    ":"
    "::"
    ":>"
    ";"
    "<"
    "<%"
    "<:"
    "<<"
    "<<="
    "<="
    "="
    "=="
    ">"
    ">="
    ">>"
    ">>="
    "?"
    "["
    "]"
    "and"
    "and_eq"
    "bitand"
    "bitor"
    "compl"
    "delete"
    "new"
    "not"
    "not_eq"
    "or"
    "or_eq"
    "xor"
    "xor_eq"
    "{"
    "}"
    "|"
    "|="
    "||"
    "~"
    "^"
    "^="})

(defn- is-word-start [^Character c] (or (Character/isLetter c) (= c \_)))
(defn- is-word-part [^Character c] (or (is-word-start c) (Character/isDigit c)))

(def operator-pattern (set->pattern cpp-operators))

(defn match-operator [s]
  (code/match-regex s operator-pattern))

(defn match-single-line-comment [s]
  (code/match-regex s #"^\Q//\E.*\n"))

(defn match-multi-line-comment [s]
  (code/match-regex s #"(?s)^\/\*.*?\*\/"))

(def cpp-ish
  {:language "c"
   :syntax   {:line-comment "// "
              :scanner [{:partition :default
                         :type      :default
                         :rules     [{:type :whitespace :space? #{\space \tab \newline \return}}
                                     {:type :custom :scanner match-single-line-comment :class "comment"}
                                     {:type :custom :scanner match-multi-line-comment :class "comment"}
                                     {:type :keyword :start? is-word-start :part? is-word-part :keywords cpp-keywords :class "keyword"}
                                     {:type :keyword :start? is-word-start :part? is-word-part :keywords cpp-preprocessor-directives :class "directive"}
                                     {:type :word :start? is-word-start :part? is-word-part :class "default"}
                                     {:type :multiline :start "\"" :end "\"" :esc \\ :class "string"}
                                     {:type :multiline :start "'" :end "'" :esc \\ :class "string"}
                                     {:type :custom :scanner code/match-number :class "number"}
                                     {:type :custom :scanner match-operator :class "operator"}
                                     {:type :default :class "default"}]}]}})


(def script-defs
  [{:ext "cpp"
    :label "C++"
    :icon "icons/32/Icons_12-Script-type.png"
    :view-types [:code :default]
    :view-opts {:code cpp-ish}}
   {:ext "h"
    :label "C/C++ Header"
    :icon "icons/32/Icons_12-Script-type.png"
    :view-types [:code :default]
    :view-opts {:code cpp-ish}}
   {:ext "mm"
    :label "Objective-C"
    :icon "icons/32/Icons_12-Script-type.png"
    :view-types [:code :default]
    :view-opts {:code cpp-ish}}
   {:ext "c"
    :label "C"
    :icon "icons/32/Icons_12-Script-type.png"
    :view-types [:code :default]
    :view-opts {:code cpp-ish}}
   {:ext "manifest"
    :label "Manifest"
    :icon "icons/32/Icons_11-Script-general.png"
    :view-types [:code :default]}
   {:ext "appmanifest"
    :label "App Manifest"
    :icon "icons/32/Icons_11-Script-general.png"
    :view-types [:code :default]}])

(g/defnode TextNode
  (inherits resource-node/TextResourceNode)

  (property caret-position g/Int (dynamic visible (g/constantly false)) (default 0))
  (property prefer-offset g/Int (dynamic visible (g/constantly false)) (default 0))
  (property tab-triggers g/Any (dynamic visible (g/constantly false)) (default nil))
  (property selection-offset g/Int (dynamic visible (g/constantly false)) (default 0))
  (property selection-length g/Int (dynamic visible (g/constantly false)) (default 0)))

(defn register-resource-types [workspace]
  (for [def script-defs
        :let [args (assoc def :node-type TextNode)]]
    (apply resource-node/register-text-resource-type workspace (mapcat identity args))))

(comment
  ;; Useful for re-registering resource types
  (do (dynamo.graph/transact (register-resource-types 0)) nil)
  
  )


