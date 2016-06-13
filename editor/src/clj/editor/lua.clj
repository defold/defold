(ns editor.lua
  (:require [clojure.string :as str]
            [clojure.java.io :as io]
            [editor.code :as code])

  (:import [com.dynamo.scriptdoc.proto ScriptDoc ScriptDoc$Type ScriptDoc$Document ScriptDoc$Document$Builder ScriptDoc$Element ScriptDoc$Parameter]))

(set! *warn-on-reflection* true)

(def ^:private pattern1 #"([a-zA-Z0-9_]+)([\\(]?)")
(def ^:private pattern2 #"([a-zA-Z0-9_]+)\.([a-zA-Z0-9_]*)([\\(]?)")

(defn- make-parse-result [namespace function in-function start end]
  {:namespace namespace
   :function function
   :in-function in-function
   :start start
   :end end})

(defn- default-parse-result []
  {:namespace ""
   :function ""
   :in-function false
   :start 0
   :end 0})

(defn- parse-unscoped-line [line]
  (let [matcher1 (re-matcher pattern1 line)]
    (loop [match (re-find matcher1)
           result nil]
      (if-not match
        result
        (let [in-function (> (count (nth match 2)) 0)
              start (.start matcher1)
              end (- (.start matcher1) (if in-function 1 0))]
          (recur (re-find matcher1)
                 (make-parse-result "" (nth match 1) in-function start end)))))))

(defn- parse-scoped-line [line]
  (let [matcher2 (re-matcher pattern2 line)]
        (loop [match (re-find matcher2)
               result nil]
          (if-not match
            result
            (let [in-function (> (count (nth match 3)) 0)
                  start (.start matcher2)
                  end (- (.end matcher2) (if in-function 1 0))]
              (recur (re-find matcher2)
                     (make-parse-result (nth match 1) (nth match 2) in-function start end)))))))

(defn parse-line [line]
  (or (parse-scoped-line line)
      (parse-unscoped-line line)))

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

(def ^:private docs (str/split "builtins camera collection_proxy collection_factory collision_object engine factory go gui http iap image json msg particlefx render sound sprite sys tilemap vmath spine zlib" #" "))

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
                   (.setReturn "")
                   (.build))]
         (if (not= (.getName e) "")
           sofar
           (update sofar "" conj e))))
     ns-elements
     namespaces)))

(def ^:private the-documentation (atom nil))

(defn- documentation []
  (when (nil? @the-documentation)
    (reset! the-documentation (load-documentation)))
  @the-documentation)

(defn filter-elements [elements prefix]
  (filter #(.startsWith (str/lower-case (.getName ^ScriptDoc$Element %)) prefix) elements))

(defn get-documentation [{:keys [namespace function]}]
  (let [namespace (str/lower-case namespace)
        function (str/lower-case function)
        qualified (if (str/blank? namespace) function (format "%s.%s" namespace function))
        elements (filter-elements (get (documentation) namespace) qualified)
        sorted-elements (sort #(compare (str/lower-case (.getName ^ScriptDoc$Element %1)) (str/lower-case (.getName ^ScriptDoc$Element %2))) elements)]
    (into-array ScriptDoc$Element sorted-elements)))

(defn- element-additional-info [^ScriptDoc$Element element]
  (when (= (.getType element) ScriptDoc$Type/FUNCTION)
    (str/join
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
      (when (> (count (.getReturn element)) 0)
        (concat
         ["<br>"]
         ["<b>Returns:</b><br>"]
         ["&#160;&#160;&#160;&#160;<b>"]
         [(.getReturn element)]))))))

(defn- element-display-string [^ScriptDoc$Element element]
  (let [base (.getName element)
        rest (when (= (.getType element) ScriptDoc$Type/FUNCTION)
               (str/join
                (concat
                 ["("
                  (str/join ", "
                            (for [^ScriptDoc$Parameter parameter (.getParametersList element)]
                              [(.getName parameter)]))
                  ")"])))]
    (str/join [base rest])))

(defn defold-documentation []
  (reduce
   (fn [result [ns elements]]
     (assoc result ns (mapv (fn [e] {:name (.getName ^ScriptDoc$Element e)
                                    :display-string (element-display-string e)
                                    :doc (element-additional-info e)}) elements)))
   {}
   (load-documentation)))


(defn filter-proposals [completions ^String text offset ^String line]
  (try
    (let
        [{:keys [namespace function] :as parse-result} (or (parse-line line) (default-parse-result))
         items (if namespace (get completions (str/lower-case namespace)) (get completions ""))
         pattern (re-pattern (str/lower-case function))]
      (filter (fn [i] (re-find pattern (:name i))) items))
    (catch Exception e
      (.printStackTrace e))))


(def helper-keywords #{"assert" "collectgarbage" "dofile" "error" "getfenv" "getmetatable" "ipairs" "loadfile" "loadstring" "module" "next" "pairs" "pcall"
                "print" "rawequal" "rawget" "rawset" "require" "select" "setfenv" "setmetatable" "tonumber" "tostring" "type" "unpack" "xpcall"})

(def logic-keywords #{"and" "or" "not"})

(def self-keyword #{"self"})

(def control-flow-keywords #{"break" "do" "else" "for" "if" "elseif" "return" "then" "repeat" "while" "until" "end" "function"
                             "local" "goto" "in"})

(def defold-keywords #{"final" "init" "on_input" "on_message" "on_reload" "update" "acquire_input_focus" "disable" "enable"
                       "release_input_focus" "request_transform" "set_parent" "transform_response"})

(def constant-pattern #"^(?<![^.]\.|:)\b(false|nil|true|_G|_VERSION|math\.(pi|huge))\b|(?<![.])\.{3}(?!\.)")
(def operator-pattern #"^(\+|\-|\%|\#|\*|\/|\^|\=|\=\=|\~\=|\<\=|\>\=)")

(defn match-constant [s]
  (code/match-regex constant-pattern s))

(defn match-operator [s]
  (code/match-regex operator-pattern s))

(defn- is-word-start [^Character c] (or (Character/isLetter c) (#{\_ \:} c)))
(defn- is-word-part [^Character c] (or (is-word-start c) (Character/isDigit c) (#{\-} c)))

(defn- match-multi-comment [s]
  (when-let [match-open (code/match-string s "--[")]
    (when-let [match-eqs (code/match-while-eq (:body match-open) \=)]
      (when-let [match-open2 (code/match-string (:body match-eqs) "[")]
        (let [closing-marker (str "]" (apply str (repeat (:length match-eqs) \=)) "]")]
          (when-let [match-body (code/match-until-string (:body match-open2) closing-marker)]
            (code/combine-matches match-open match-eqs match-open2 match-body)))))))

(defn- match-multi-open [s]
  (when-let [match-open (code/match-string s "[")]
    (when-let [match-eqs (code/match-while-eq (:body match-open) \=)]
      (when-let [match-open2 (code/match-string (:body match-eqs) "[")]
        (code/combine-matches match-open match-eqs match-open2)))))

(defn- match-single-comment [s]
  (when-let [match-open (code/match-string s "--")]
    (when-not (match-multi-open (:body match-open))
      (when-let [match-body (code/match-until-eol (:body match-open))]
        (code/combine-matches match-open match-body)))))

;; TODO: splitting into partitions using multiline (MultiLineRule) in combination with FastPartitioner does not work properly when
;; :eof is false. The initial "/*" does not start a comment partition, and when the ending "*/" is added (on another line) the document
;; is repartitioned only from the beginning of that final line - meaning the now complete multiline comment is not detected as a partition.
;; After inserting some text on the opening ("/*" ) line, the partition is detected however.
;; Workaround: the whole language is treated as one single default partition type.

(def lua {:language "lua"
          :syntax
          {:line-comment "-- "
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
              {:type :custom :scanner match-single-comment :class "comment"}
              {:type :custom :scanner match-multi-comment :class "comment-multi"}
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
