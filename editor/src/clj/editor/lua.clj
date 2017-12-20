(ns editor.lua
  (:require [clojure.java.io :as io]
            [clojure.edn :as edn]
            [clojure.set :as set]
            [clojure.string :as string]
            [editor.code :as code]
            [editor.code.integration :as code-integration]
            [editor.protobuf :as protobuf]
            [internal.util :as util]
            [schema.core :as s])
  (:import [com.dynamo.scriptdoc.proto ScriptDoc$Type ScriptDoc$Document ScriptDoc$Element ScriptDoc$Parameter ScriptDoc$ReturnValue]
           [org.apache.commons.io FilenameUtils]))

(set! *warn-on-reflection* true)

(defn- load-sdoc [path]
  (try
    (with-open [in (io/input-stream (io/resource path))]
      (let [doc (-> (ScriptDoc$Document/newBuilder)
                  (.mergeFrom in)
                  (.build))
            elements (.getElementsList ^ScriptDoc$Document doc)]
        (reduce
         (fn [ns-elements ^ScriptDoc$Element element]
           (let [qname (.getName element)
                 dot (.indexOf qname ".")
                 ns (if (= dot -1) "" (subs qname 0 dot))]
             (update ns-elements ns conj element)))
         nil
         elements)))
    (catch Exception e
      (.printStackTrace e)
      {})))

(def ^:private docs (string/split "base bit buffer builtins camera collectionfactory collectionproxy coroutine crash debug facebook factory go gui html5 http iac iap image io json label math model msg os package particlefx physics profiler push render resource socket sound spine sprite string sys table tilemap vmath webview window zlib" #" "))

(defn- sdoc-path [doc]
  (format "doc/%s_doc.sdoc" doc))

(defn load-documentation []
  (let [ns-elements (reduce
                     (fn [sofar doc]
                       (merge-with concat sofar (load-sdoc (sdoc-path doc))))
                     nil
                     docs)
        namespaces (keys ns-elements)]
    (reduce
     (fn [sofar ns]
       (let [e (-> (ScriptDoc$Element/newBuilder)
                   (.setType ScriptDoc$Type/NAMESPACE)
                   (.setName ns)
                   (.setBrief "")
                   (.setDescription "")
                   (.build))]
         (if (not= (.getName e) "")
           sofar
           (update sofar "" conj e))))
     ns-elements
     namespaces)))

(defn- element-additional-info [^ScriptDoc$Element element]
  (when (= (.getType element) ScriptDoc$Type/FUNCTION)
    (string/join
     (concat
      [(.getDescription element)]
      ["<br><br>"]
      ["<b>Parameters:</b>"]
      ["<br>"]
      (for [^ScriptDoc$Parameter parameter (.getParametersList element)]
        (concat
         ["&#160;&#160;&#160;&#160;<b>"]
         [(.getName parameter)]
         ["</b> "]
         [(.getDoc parameter)]
         ["<br>"]))
      (when (< 0 (.getReturnvaluesCount element))
        (concat
         ["<br>"]
         ["<b>Returns:</b><br>"]
         ["&#160;&#160;&#160;&#160;<b>"]
         (for [^ScriptDoc$ReturnValue retval (.getReturnvaluesList element)]
           [(format "%s: %s" (.getName retval) (.getDoc retval))])))))))

(defn- element-display-string [^ScriptDoc$Element element include-optional-params?]
  (let [base (.getName element)
        rest (when (= (.getType element) ScriptDoc$Type/FUNCTION)
               (let [params (for [^ScriptDoc$Parameter parameter (.getParametersList element)]
                              (.getName parameter))
                     display-params (if include-optional-params? params (remove #(= \[ (first %)) params))]
                 (str "(" (string/join ", " display-params) ")")))]
    (str base rest)))

(defn- element-tab-triggers [^ScriptDoc$Element element]
  (when (= (.getType element) ScriptDoc$Type/FUNCTION)
    (let [params (for [^ScriptDoc$Parameter parameter (.getParametersList element)]
                   (.getName parameter))]
      {:select (filterv #(not= \[ (first %)) params) :exit (when params ")")})))

(def ^:private documentation-schema
  {s/Str [{:type (s/enum :function :message :namespace :property :snippet :variable :keyword)
           :name s/Str
           :display-string s/Str
           :insert-string s/Str
           (s/optional-key :doc) s/Str
           (s/optional-key :tab-triggers) (s/both {:select [s/Str]
                                                   (s/optional-key :types) [(s/enum :arglist :expr :name)]
                                                   (s/optional-key :start) s/Str
                                                   (s/optional-key :exit) s/Str}
                                                  (s/pred (fn [tab-trigger]
                                                            (or (nil? (:types tab-trigger))
                                                                (= (count (:select tab-trigger))
                                                                   (count (:types tab-trigger)))))
                                                          "specified :types count equals :select count"))}]})

(defn defold-documentation []
  (s/validate documentation-schema
    (reduce (fn [result [ns elements]]
              (let [global-results (get result "" [])
                    new-result (assoc result ns (into []
                                                      (comp (map (fn [^ScriptDoc$Element e]
                                                                   (code/create-hint (protobuf/pb-enum->val (.getType e))
                                                                                     (.getName e)
                                                                                     (element-display-string e true)
                                                                                     (element-display-string e false)
                                                                                     (element-additional-info e)
                                                                                     (element-tab-triggers e))))
                                                            (util/distinct-by :display-string))
                                                      elements))]
                (if (= "" ns) new-result (assoc new-result "" (conj global-results (code/create-hint :namespace ns))))))
            {}
            (load-documentation))))

(def helper-keywords #{"assert" "collectgarbage" "dofile" "error" "getfenv" "getmetatable" "ipairs" "loadfile" "loadstring" "module" "next" "pairs" "pcall"
                       "print" "rawequal" "rawget" "rawset" "require" "select" "setfenv" "setmetatable" "tonumber" "tostring" "type" "unpack" "xpcall"})

(def logic-keywords #{"and" "or" "not"})

(def self-keyword #{"self"})

(def control-flow-keywords #{"break" "do" "else" "for" "if" "elseif" "return" "then" "repeat" "while" "until" "end" "function"
                             "local" "goto" "in"})

(def defold-keywords #{"final" "init" "on_input" "on_message" "on_reload" "update" "acquire_input_focus" "disable" "enable"
                       "release_input_focus" "request_transform" "set_parent" "transform_response"})

(def constant-pattern #"^(?:(?<![^.]\.|:)\b(?:false|nil|true|_G|_VERSION|math\.(?:pi|huge))\b|(?<![.])\.{3}(?!\.))")

(def operator-pattern #"^(?:\+|\-|\%|\#|\*|\/|\^|\=|\=\=|\~\=|\<\=|\>\=)")

(def all-keywords
  (set/union helper-keywords
             logic-keywords
             self-keyword
             control-flow-keywords
             defold-keywords))

(def defold-docs (atom (defold-documentation)))
(def preinstalled-modules (into #{} (remove #{""} (keys @defold-docs))))

(defn lua-base-documentation []
  (s/validate documentation-schema
              {"" (into []
                        (util/distinct-by :display-string)
                        (concat (map #(assoc % :type :snippet)
                                     (-> (io/resource "lua-base-snippets.edn")
                                         slurp
                                         edn/read-string))
                                (when code-integration/use-new-code-editor?
                                  (map (fn [keyword]
                                         {:type :keyword
                                          :name keyword
                                          :display-string keyword
                                          :insert-string keyword})
                                       all-keywords))))}))

(def lua-std-libs-docs (atom (lua-base-documentation)))

(defn filter-proposals [completions ^String text offset ^String line]
  (try
    (let
        [{:keys [namespace function] :as parse-result} (or (code/parse-line line) (code/default-parse-result))
         items (if namespace (get completions namespace) (get completions ""))
         pattern (code/proposal-filter-pattern namespace function)
         results (filter (fn [i] (string/starts-with? (:name i) pattern)) items)]
      (->> results (sort-by :display-string) (partition-by :display-string) (mapv first)))
    (catch Exception e
      (.printStackTrace e))))

(defn lua-module->path [module]
  (str "/" (string/replace module #"\." "/") ".lua"))

(defn path->lua-module [path]
  (-> (if (string/starts-with? path "/") (subs path 1) path)
      (string/replace #"/" ".")
      (FilenameUtils/removeExtension)))

(defn lua-module->build-path [module]
  (str (lua-module->path module) "c"))

(defn match-constant [s]
  (code/match-regex s constant-pattern))

(defn match-operator [s]
  (code/match-regex s operator-pattern))

(defn- is-word-start [^Character c] (or (Character/isLetter c) (= c \_) (= c \:)))
(defn- is-word-part [^Character c] (or (is-word-start c) (Character/isDigit c) (= c \-)))

(defn- match-long-bracket-section [s]
  (when-let [match-open (code/match-string s "[")]
    (when-let [match-eqs (code/match-while-eq (:body match-open) \=)]
      (when-let [match-open2 (code/match-string (:body match-eqs) "[")]
        (let [closing-bracket (str "]" (apply str (repeat (:length match-eqs) \=)) "]")]
          (when-let [match-body (code/match-until-string (:body match-open2) closing-bracket)]
            (code/combine-matches match-open match-eqs match-open2 match-body)))))))
                        

(defn- match-multi-comment [s]
  (when-let [match-open (code/match-string s "--")]
    (when-let [match-bs (match-long-bracket-section (:body match-open))]
      (code/combine-matches match-open match-bs))))

(defn- match-single-comment [s]
  (when-let [match-open (code/match-string s "--")]
    (when-let [match-body (code/match-until-eol (:body match-open))]
      (code/combine-matches match-open match-body))))

(defn increase-indent? [s]
  (re-find #"^\s*(else|elseif|for|(local\s+)?function|if|while)\b((?!end\b).)*$|\{\s*$" s))

(defn decrease-indent? [s]
  (re-find #"^\s*(elseif|else|end|\}).*$" s))

;; TODO: splitting into partitions using multiline (MultiLineRule) in combination with FastPartitioner does not work properly when
;; :eof is false. The initial "/*" does not start a comment partition, and when the ending "*/" is added (on another line) the document
;; is repartitioned only from the beginning of that final line - meaning the now complete multiline comment is not detected as a partition.
;; After inserting some text on the opening ("/*" ) line, the partition is detected however.
;; Workaround: the whole language is treated as one single default
;; partition type.

(def lua {:language "lua"
          :syntax
          {:line-comment "-- "
           :indentation {:increase? increase-indent?
                         :decrease? decrease-indent?}
           :scanner
           [#_{:partition "__multicomment"
               :type :multiline
               :start "--[[" :end "]]"
               :eof false
               :rules
               [{:type :default :class "comment"}]
               }
            #_{:partition "__multicomment"
               :type :custom
               :scanner match-multi-comment
               :rules
               [{:type :default :class "comment"}]
               }
            #_{:partition "__singlecomment"
               :type :singleline
               :start "--"
               :rules
               [{:type :default :class "comment-multi"}]}
            #_{:partition "__singlecomment"
               :type :custom
               :scanner match-single-comment
               :rules
               [{:type :default :class "comment-multi"}]}
            {:partition :default
             :type :default
             :rules
             [{:type :whitespace :space? #{\space \tab \newline \return}}
              {:type :custom :scanner match-multi-comment :class "comment-multi"}
              {:type :custom :scanner match-single-comment :class "comment"}
              {:type :keyword :start? is-word-start :part? is-word-part :keywords defold-keywords :class "defold-keyword"}
              {:type :keyword :start? is-word-start :part? is-word-part :keywords helper-keywords :class "helper-keyword"}
              {:type :keyword :start? is-word-start :part? is-word-part :keywords self-keyword :class "self-keyword"}
              {:type :keyword :start? is-word-start :part? is-word-part :keywords logic-keywords :class "logic-keywords"}
              {:type :keyword :start? is-word-start :part? is-word-part :keywords control-flow-keywords :class "control-flow-keyword"}
              {:type :word :start? is-word-start :part? is-word-part :class "default"}
              {:type :singleline :start "\"" :end "\"" :esc \\ :class "string"}
              {:type :singleline :start "'" :end "'" :esc \\ :class "string"}
              {:type :custom :scanner match-constant :class "constant"}
              {:type :custom :scanner code/match-number :class "number"}
              {:type :custom :scanner match-operator :class "operator"}
              {:type :number :class "number"}
              {:type :default :class "default"}
              ]}
            ]}
          :assist filter-proposals
          })
