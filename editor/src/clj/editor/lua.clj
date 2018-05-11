(ns editor.lua
  (:require [clojure.java.io :as io]
            [clojure.edn :as edn]
            [clojure.set :as set]
            [clojure.string :as string]
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

(defn create-code-hint
  ([type name]
   (create-code-hint type name name name nil nil))
  ([type name display-string insert-string doc tab-triggers]
   (cond-> {:type type
            :name name
            :display-string display-string
            :insert-string insert-string}

           (not-empty doc)
           (assoc :doc doc)

           (some? tab-triggers)
           (assoc :tab-triggers tab-triggers))))

(def ^:private documentation-schema
  {s/Str [{:type (s/enum :constant :function :message :namespace :property :snippet :variable :keyword)
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
                                                                   (create-code-hint (protobuf/pb-enum->val (.getType e))
                                                                                     (.getName e)
                                                                                     (element-display-string e true)
                                                                                     (element-display-string e false)
                                                                                     (element-additional-info e)
                                                                                     (element-tab-triggers e))))
                                                            (util/distinct-by :display-string))
                                                      elements))]
                (if (= "" ns) new-result (assoc new-result "" (conj global-results (create-code-hint :namespace ns))))))
            {}
            (load-documentation))))

(def helper-keywords #{"assert" "collectgarbage" "dofile" "error" "getfenv" "getmetatable" "ipairs" "loadfile" "loadstring" "module" "next" "pairs" "pcall"
                       "print" "rawequal" "rawget" "rawset" "require" "select" "setfenv" "setmetatable" "tonumber" "tostring" "type" "unpack" "xpcall"})

(def logic-keywords #{"and" "or" "not"})

(def self-keyword #{"self"})

(def control-flow-keywords #{"break" "do" "else" "for" "if" "elseif" "return" "then" "repeat" "while" "until" "end" "function"
                             "local" "goto" "in"})

(def defold-callbacks #{"final" "init" "on_input" "on_message" "on_reload" "update"})

(def defold-messages #{"acquire_input_focus" "disable" "enable" "release_input_focus" "request_transform" "set_parent" "transform_response"})

(def lua-constants #{"nil" "false" "true"})

(def all-keywords
  (set/union helper-keywords
             logic-keywords
             self-keyword
             control-flow-keywords
             defold-callbacks
             defold-messages))

(def defold-docs (atom (defold-documentation)))
(def preinstalled-modules (into #{} (remove #{""} (keys @defold-docs))))
(def defold-globals (into preinstalled-modules ["hash"]))

(defn lua-base-documentation []
  (s/validate documentation-schema
              {"" (into []
                        (util/distinct-by :display-string)
                        (concat (map #(assoc % :type :snippet)
                                     (-> (io/resource "lua-base-snippets.edn")
                                         slurp
                                         edn/read-string))
                                (map (fn [constant]
                                       {:type :constant
                                        :name constant
                                        :display-string constant
                                        :insert-string constant})
                                     lua-constants)
                                (map (fn [keyword]
                                       {:type :keyword
                                        :name keyword
                                        :display-string keyword
                                        :insert-string keyword})
                                     all-keywords)))}))

(def lua-std-libs-docs (atom (lua-base-documentation)))

(defn lua-module->path [module]
  (str "/" (string/replace module #"\." "/") ".lua"))

(defn path->lua-module [path]
  (-> (if (string/starts-with? path "/") (subs path 1) path)
      (string/replace #"/" ".")
      (FilenameUtils/removeExtension)))

(defn lua-module->build-path [module]
  (str (lua-module->path module) "c"))
