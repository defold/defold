(ns editor.lua-parser
  (:require [clj-antlr.core :as antlr]
            [clojure.string :as string]
            [clojure.java.io :as io]
            [clojure.set :as set]))

(def real-lua-parser (antlr/parser (slurp (io/resource "Lua.g4")) {:throw? false}))

(defn lua-parser [code]
  (try
    (real-lua-parser code)
    (catch ClassCastException e
      ;; for some inputs, like "local max_speed = 60|= <!ed\n\tend\nend+-!\n" we get:
      ;; Unhandled java.lang.ClassCastException
      ;;   org.antlr.v4.runtime.atn.EpsilonTransition cannot be cast to
      ;;   org.antlr.v4.runtime.atn.RuleTransition
      ;; ... which we can't do much about for now.
      nil)))


(defn- node-type? [tag node] (and (seq? node) (= tag (first node))))
(defn- error-node? [node] (node-type? :clj-antlr/error node))
(defn- exp-node? [node] (node-type? :exp node))
(defn- prefix-exp-node? [node] (node-type? :prefixexp node))
(defn- var-node? [node] (node-type? :var node))
(defn- string-node? [node] (node-type? :string node))
(defn- namelist-node? [node] (node-type? :namelist node))
(defn- explist-node? [node] (node-type? :explist node))
(defn- name-and-args-node? [node] (node-type? :nameAndArgs node))
(defn- comma-node? [node] (and (string? node) (= "," node)))

(defn- parse-namelist [namelist]
  (vec (remove (some-fn comma-node? error-node?) (rest namelist))))

(defn- parse-parlist [[_ parlist-content]]
  (vec (cond
         (and (string? parlist-content)
              (= "..." parlist-content))
         [parlist-content]

         (namelist-node? parlist-content)
         (parse-namelist parlist-content)

         :else nil)))

(defn- parse-funcname [funcname]
  (when-not (or (error-node? funcname)
                (some error-node? funcname))
    (apply str (rest funcname))))

(defn- parse-funcbody [funcbody]
  (when-not (or (error-node? funcbody)
                (some error-node? funcbody))
    (let [[_ _ parlist _ block] funcbody]
      [parlist block])))

(defn parse-string-vars [varlist]
  (filterv string? (map second (filter var-node? (rest varlist)))))

(def ^:private escape-sequence
  ;; From PIL https://www.lua.org/pil/2.4.html
  {\a (char 7)
   \b \backspace
   \f \formfeed
   \n \newline
   \r \return
   \t \tab
   \v (char 11)
   \\ \\
   \" \"
   \' \'
   \[ \[
   \] \]
   })

(defn- parse-quoted-string [s]
  (when (and (>= (count s) 2)
             (contains? #{\" \'} (first s)))
    (let [s (subs s 1 (- (count s) 1))]
      (loop [new-chars []
             chars (seq s)
             escape false]
        (if-let [ch (first chars)]
          (if escape
            (if-let [actual (escape-sequence ch)]
              (recur (conj new-chars actual) (next chars) false)
              (recur new-chars (next chars) false))
            (if (= ch \\)
              (recur new-chars (next chars) true)
              (recur (conj new-chars ch) (next chars) false)))
          (apply str new-chars))))))

(def ^:private long-open-pattern #"^\[=*\[")

(defn- parse-long-string [s]
  ;; We assume the string is well formed, i.e. ends with a matching closing pattern, this is "safe" since it's part of the grammar
  (when-let [match (re-find long-open-pattern s)]
    (let [s (subs s (count match) (- (count s) (count match)))]
      ;; Strip one initial new line/carriage return. 
      ;; PIL says: "... this form ignores the first character of the string when this character is a newline"
      ;; but testing with lua interpreter it seems a leading \r\n, \n\r, \n or \r are stripped.
      (cond
        (string/starts-with? s "\r\n") (subs s 2)
        (string/starts-with? s "\n\r") (subs s 2)
        (string/starts-with? s "\n") (subs s 1)
        (string/starts-with? s "\r") (subs s 1)
        :else s))))

(defn- parse-string [s]
  (or (parse-quoted-string s)
      (parse-long-string s)))

(defn- parse-string-node [node]
  (when (string-node? node)
    (parse-string (second node))))

(defn- parse-exp-string [exp]
  (parse-string-node (second exp)))

(defn- parse-explist-strings [explist]
  (let [explist (remove comma-node? (rest explist))]
    (keep parse-exp-string explist)))

(defn- parse-args-strings [args]
  (cond
    (string-node? (second args))
    [(parse-string-node (second args))]

    (and (> (count args) 2)
         (explist-node? (nth args 2)))
    (parse-explist-strings (nth args 2))

    :else
    nil))

(defn- parse-name-and-args [name-and-args]
  (when-not (error-node? name-and-args)
    (if (and (= ":" (nth name-and-args 1))
             (string? (nth name-and-args 2)))
      [(str ":" (nth name-and-args 2)) (parse-args-strings (nth name-and-args 3))]
      ["" (parse-args-strings (nth name-and-args 1))])))

(defn- parse-first-functioncall [node]
  ;; should work on :functioncall and :prefixexp's
  ;; possible to chain calls, like foo("bar")("baz") or even a:foo("abc"):cake("topping")
  ;; we only parse the first call in the chain for now
  (let [var-or-exp (second node)
        name-and-args (first (filter name-and-args-node? (nthrest node 2)))]
    (when (and (not (error-node? var-or-exp))
               (var-node? (second var-or-exp))
               (string? (second (second var-or-exp))))
      (let [base-name (second (second var-or-exp))
            [meth-call args] (parse-name-and-args name-and-args)]
        [(str base-name meth-call) args]))))

(defn parse-explist-requires [parsed-names explist]
  (let [prefix-exps (keep #(when (and (exp-node? %) (prefix-exp-node? (second %))) (second %)) (rest explist))
        funcalls (map parse-first-functioncall prefix-exps)
        required-files (map #(when (= "require" (first %)) (first (second %))) funcalls)]
    (remove (comp nil? second) (map vector parsed-names required-files))))

(defmulti collect-node-info (fn [result node] (if (seq? node) (first node) node)))

(defmethod collect-node-info :default [result _]
  result)

(defmethod collect-node-info :chunk [result node]
  (collect-node-info result (second node)))

(defmethod collect-node-info :block [result node]
  (reduce collect-node-info result (rest node)))

(defmulti collect-stat-node-info (fn [kw result node] kw))

(defmethod collect-node-info :stat [result node]
  (let [k (second node)]
    (if (seq? k)
      (collect-stat-node-info (first k) result node)
      (collect-stat-node-info (keyword k) result node))))
        
(defmethod collect-stat-node-info :local [_ result node]
  (if (and (> (count node) 2)
           (string? (nth node 2)) 
           (= "function" (nth node 2)))
    ;; 'local' 'function' NAME funcbody
    (let [[_ _ _ fname funcbody] node]
      (if-not (error-node? fname)
        (let [[parlist block] (parse-funcbody funcbody)]
          (collect-node-info (conj result {:local-functions {fname {:params (parse-parlist parlist)}}}) block))
        result))
    ;; 'local' namelist ('=' explist)?
    (let [[_ _ namelist _ explist] node]
      (if-let [parsed-names (parse-namelist namelist)]
        (conj result {:local-vars parsed-names :requires (parse-explist-requires parsed-names explist)})
        result))))

(defmethod collect-stat-node-info :default [_ result node] result)

(defmethod collect-stat-node-info :varlist [_ result node]
  (let [[_ varlist _ explist] node
        parsed-names (parse-string-vars varlist)
        require-info (parse-explist-requires parsed-names explist)]
    (conj result {:vars parsed-names :requires require-info})))

(defmethod collect-stat-node-info :function [_ result node]
  (let [[_ _ funcname funcbody] node]
    (if-let [funcname (parse-funcname funcname)]
      (let [[parlist block] (parse-funcbody funcbody)]
        (collect-node-info (conj result {:functions {funcname {:params (parse-parlist parlist)}}}) block))
      result)))

(defmethod collect-stat-node-info :functioncall [_ result node]
  (let [functioncall (second node)]
    (let [[name args] (parse-first-functioncall functioncall)]
      (if (and (= name "require") (seq args))
        (let [require-name (first args)]
          (conj result {:requires [[nil require-name]]}))
        result))))

(defn collect-info [node]
  (collect-node-info [] node))
         
(defn lua-info [code]
  (let [info (-> code lua-parser collect-info)
        local-vars-info (into #{} (apply concat (map :local-vars info)))
        vars-info (set/difference (into #{} (apply concat (map :vars info))) local-vars-info)
        functions-info (or (apply merge (map :functions info)) {})
        local-functions-info (or (apply merge (map :local-functions info)) {})
        requires-info (vec (distinct (filter seq (apply concat (map :requires info)))))]
    {:vars vars-info
     :local-vars local-vars-info
     :functions functions-info
     :local-functions local-functions-info
     :requires requires-info}))
