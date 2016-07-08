(ns editor.lua
  (:require [clojure.string :as string]
            [clojure.java.io :as io]
            [clojure.edn :as edn]
            [editor.code :as code])
  (:import [com.dynamo.scriptdoc.proto ScriptDoc ScriptDoc$Type ScriptDoc$Document ScriptDoc$Document$Builder ScriptDoc$Element ScriptDoc$Parameter]))

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

(def ^:private docs (string/split "builtins camera collection_proxy collection_factory collision_object engine factory go gui http iap image json msg particlefx render sound sprite sys tilemap vmath spine zlib" #" "))

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
      (when (> (count (.getReturn element)) 0)
        (concat
         ["<br>"]
         ["<b>Returns:</b><br>"]
         ["&#160;&#160;&#160;&#160;<b>"]
         [(.getReturn element)]))))))

(defn- element-display-string [^ScriptDoc$Element element include-optional-params?]
  (let [base (.getName element)
        rest (when (= (.getType element) ScriptDoc$Type/FUNCTION)
               (let [params (for [^ScriptDoc$Parameter parameter (.getParametersList element)]
                              (.getName parameter))
                     display-params (if include-optional-params? params (remove #(= \[ (first %)) params))]
                 (str "(" (string/join "," display-params) ")")))]
    (str base rest)))

(defn- element-tab-triggers [^ScriptDoc$Element element]
  (when (= (.getType element) ScriptDoc$Type/FUNCTION)
    (let [params (for [^ScriptDoc$Parameter parameter (.getParametersList element)]
                   (.getName parameter))]
      {:select (remove #(= \[ (first %)) params)})))

(defn defold-documentation []
  (reduce
   (fn [result [ns elements]]
     (let [global-results (get result "" [])
           new-result (assoc result ns (set (map (fn [e] (code/create-hint (.getName ^ScriptDoc$Element e)
                                                                          (element-display-string e true)
                                                                          (element-display-string e false)
                                                                          (element-additional-info e)
                                                                          (element-tab-triggers e))) elements)))]
       (if (= "" ns) new-result (assoc new-result "" (conj global-results {:name ns :display-string ns :doc ""})))))
   {}
   (load-documentation)))

(def defold-docs (atom (defold-documentation)))

(defn lua-std-libs-documentation []
  (let [base-items (->  (io/resource "lua-standard-libs.edn")
                        slurp
                        edn/read-string)
        hints (for [item base-items]
                (let [insert-string (string/replace item #"\[.*\]" "")
                      params (re-find #"(\()(.*)(\))" insert-string)
                      tab-triggers (when (< 3 (count params)) (remove #(= "" %) (string/split (nth params 2) #",")))]
                  (code/create-hint
                   (first (string/split item #"\("))
                   item
                   insert-string
                   ""
                   {:select tab-triggers})))
        completions (group-by #(let [names (string/split (:name %) #"\.")]
                                         (if (= 2 (count names)) (first names) "")) hints)
        package-completions {"" (map code/create-hint (remove #(= "" %) (keys completions)))}]
    (merge-with into completions package-completions)))

(defn lua-base-documentation []
  {"" (-> (io/resource "lua-base-snippets.edn")
          slurp
          edn/read-string)})

(def lua-std-libs-docs (atom (merge-with into (lua-base-documentation) (lua-std-libs-documentation))))


(defn filter-proposals [completions ^String text offset ^String line]
  (try
    (let
        [{:keys [namespace function] :as parse-result} (or (code/parse-line line) (code/default-parse-result))
         items (if namespace (get completions (string/lower-case namespace)) (get completions ""))
         pattern (string/lower-case (code/proposal-filter-pattern namespace function))
         results (filter (fn [i] (string/starts-with? (:name i) pattern)) items)]
      (->> results (sort-by :display-string) (partition-by :display-string) (mapv first)))
    (catch Exception e
      (.printStackTrace e))))

(defn lua-module->path [module]
  (str "/" (string/replace module #"\." "/") ".lua"))

(defn lua-module->build-path [module]
  (str (lua-module->path module) "c"))


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

(defn increase-indent? [s]
  (re-find #"^\s*(else|elseif|for|(local\s+)?function|if|while)\b((?!end).)*$|\{\s*$" s))

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
           :indentation {:indent-chars "\t"
                         :increase? increase-indent?
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
