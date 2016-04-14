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

(defn- load-documentation []
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

(defn doc-element-to-hint [{:keys [in-function namespace function start end]} offset element]
  (let [display-string (element-display-string element)
        additional-info (element-additional-info element)
        image "icons/32/Icons_29-AT-Unkown.png"
        complete-length (+ (count namespace) (count function) 1)
        cursor-position (- (count display-string) complete-length)]
    (if in-function
      {:replacement ""
       :offset offset
       :length 0
       :position 0
       :image image
       :display-string display-string
       :additional-info additional-info}
      ;; logic for replacement offsets etc as lifted from old editor was borked so below is also broken and just for testing
      ;; probably need prefix length info etc from saner parsing
      (if (str/blank? namespace)
        (let [match-length (- end start) 
              replacement-offset offset
              replacement-length 0
              replacement-string (subs display-string match-length)
              new-cursor-position (+ cursor-position 1)]
          {:replacement replacement-string
           :offset replacement-offset
           :length replacement-length
           :position new-cursor-position
           :image image
           :display-string display-string
           :additional-info additional-info})
        (let [match-length (- end start)
              replacement-offset offset
              replacement-length 0
              replacement-string (subs display-string (+ (count namespace) 1))
              new-cursor-position (+ cursor-position (count function))]
          {:replacement replacement-string
           :offset replacement-offset
           :length replacement-length
           :position new-cursor-position
           :image image
           :display-string display-string
           :additional-info additional-info})))))

(defn- doc-elements-to-hints [elements parse-result offset]
  (map (partial doc-element-to-hint parse-result offset) elements))

(defn compute-proposals [^String text offset ^String line]
  (try 
    (when-let [parse-result (or (parse-line line) (default-parse-result))]
      (doc-elements-to-hints (get-documentation parse-result) parse-result offset))
    (catch Exception e
      (.printStackTrace e))))

(def ^:private keywords (str/split "and break do else elseif end false for function if in local nil not or repeat return then true until while self require" #" "))

(defn- is-word-start [^Character c] (or (Character/isLetter c) (#{\_ \:} c)))
(defn- is-word-part [^Character c] (or (is-word-start c) (Character/isDigit c) (#{\-} c)))

(defn- match-multi-comment [charseq]
  (when-let [match-open (code/match-string charseq "--[")]
    (when-let [match-eqs (code/match-while-eq (:body match-open) \=)]
      (when-let [match-open2 (code/match-string (:body match-eqs) "[")]
        (let [closing-marker (str "]" (apply str (repeat (:length match-eqs) \=)) "]")] 
          (when-let [match-body (code/match-until-string (:body match-open2) closing-marker)]
            (code/combine-matches match-open match-eqs match-open2 match-body)))))))

(defn- match-multi-open [charseq]
  (when-let [match-open (code/match-string charseq "[")]
    (when-let [match-eqs (code/match-while-eq (:body match-open) \=)]
      (when-let [match-open2 (code/match-string (:body match-eqs) "[")]
        (code/combine-matches match-open match-eqs match-open2)))))

(defn- match-single-comment [charseq]
  (when-let [match-open (code/match-string charseq "--")]
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
              [{:type :default :class "comment2"}]}
           #_{:partition "__singlecomment"
            :type :custom
            :scanner match-single-comment
            :rules
            [{:type :default :class "comment2"}]}
           {:partition :default
            :type :default
            :rules
            [{:type :whitespace :space? #{\space \tab \newline \return}}
             {:type :keyword :start? is-word-start :part? is-word-part :keywords keywords :class "keyword"}
             {:type :word :start? is-word-start :part? is-word-part :class "default"}
             {:type :singleline :start "\"" :end "\"" :esc \\ :class "string"}
             {:type :singleline :start "'" :end "'" :esc \\ :class "string"}
             {:type :custom :scanner match-single-comment :class "comment"}
             {:type :custom :scanner match-multi-comment :class "comment2"}
             {:type :number :class "number"}
             {:type :default :class "default"}
             ]}
           ]
          :assist compute-proposals
          })
