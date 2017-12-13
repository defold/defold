(ns editor.lua-parser
  (:require [clj-antlr.core :as antlr]
            [clojure.java.io :as io]
            [clojure.set :as set]
            [clojure.string :as string]
            [editor.math :as math]))

(def real-lua-parser (antlr/parser (slurp (io/resource "Lua.g4")) {:throw? false}))

(defn lua-parser [code]
  (try
    (real-lua-parser code)
    (catch ClassCastException _
      ;; for some inputs, like "local max_speed = 60|= <!ed\n\tend\nend+-!\n" we get:
      ;; Unhandled java.lang.ClassCastException
      ;;   org.antlr.v4.runtime.atn.EpsilonTransition cannot be cast to
      ;;   org.antlr.v4.runtime.atn.RuleTransition
      ;; ... which we can't do much about for now.
      nil)))


(defn- node-type? [tag node] (and (seq? node) (= tag (first node))))
(defn- error-node? [node] (node-type? :clj-antlr/error node))
(defn- var-node? [node] (node-type? :var node))
(defn- namelist-node? [node] (node-type? :namelist node))
(defn- name-and-args-node? [node] (node-type? :nameAndArgs node))
(defn- comma-node? [node] (and (string? node) (= "," node)))

(defn- matching-pred [pred node] (when (pred node) node))
(defn- matching-type [tag node] (when (node-type? tag node) node))
(defn- unpack-exp [node] (some->> node (matching-type :exp) second))

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
  (when (and (node-type? :funcbody funcbody)
             (not-any? error-node? funcbody))
    (let [[_ _ rparen-or-parlist parlist-or-block maybe-block] funcbody
          parlist (when (not= ")" rparen-or-parlist) rparen-or-parlist)
          block (if (= ")" rparen-or-parlist) parlist-or-block maybe-block)]
      (when (and (node-type? :block block)
                 (or (nil? parlist) (node-type? :parlist parlist)))
        [parlist block]))))

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

(defn- parse-boolean [s]
  (case s
    "true" true
    "false" false
    nil))

(defn- parse-boolean-exp-node [node]
  (some->> node unpack-exp parse-boolean))

(defn- parse-number [s]
  (try
    (some-> s Double/parseDouble)
    (catch NumberFormatException _
      nil)))

(defn- parse-number-exp-node [node]
  ;; Only supports number literals and the unary negation operator.
  (when-some [exp-node (matching-type :exp node)]
    (let [[tag value] (second exp-node)]
      (if (= :number tag)
        (parse-number value)
        (when (and (= :operatorUnary) (= "-" value))
          (- (parse-number-exp-node (second (next exp-node)))))))))

(defn- parse-string [s]
  (or (parse-quoted-string s)
      (parse-long-string s)))

(defn- parse-string-node [node]
  (some->> node (matching-type :string) second parse-string))

(defn- parse-string-exp-node [node]
  (some->> node unpack-exp parse-string-node))

(defn- parse-arg-exps [node]
  (when-some [name-and-args-node (matching-type :nameAndArgs node)]
    (when-some [args-node (matching-type :args (second name-and-args-node))]
      (cond
        ;; func()
        (= '(:args "(" ")") args-node)
        []

        ;; func {}
        ;; require ""
        (= 2 (count args-node))
        [(list :exp (second args-node))] ;; Wrap in :exp so these can be treated the same as regular args.

        ;; func(1)
        ;; func(1, 2)
        :else
        (when-some [explist-node (matching-type :explist (second (next args-node)))]
          (into [] (keep (partial matching-type :exp)) explist-node))))))

(defn- parse-functioncall [node]
  (when-some [var-or-exp-node (matching-type :varOrExp (second node))]
    (when-some [var-node (matching-type :var (second var-or-exp-node))]
      (when-some [name-and-args-node (first (filter name-and-args-node? (nthrest node 2)))]
        (when-some [arg-exps (parse-arg-exps name-and-args-node)]
          (if (= 2 (count var-node))
            (when-some [function-name (matching-pred string? (second var-node))]
              [nil function-name arg-exps])
            (when-some [var-suffix-node (matching-type :varSuffix (nth var-node 2))]
              (when-some [module-name (matching-pred string? (second var-node))]
                (when-some [function-name (matching-pred string? (nth var-suffix-node 2))]
                  [module-name function-name arg-exps])))))))))

(defn- matching-functioncall
  ([function-name node]
   (matching-functioncall nil function-name node))
  ([module-name function-name node]
   (when-some [[parsed-module-name parsed-function-name parsed-arg-exps] (parse-functioncall node)]
     (when (and (= module-name parsed-module-name)
                (= function-name parsed-function-name))
       parsed-arg-exps))))

(defn- matching-args [parse-fns arg-exps]
  (when (= (count parse-fns) (count arg-exps))
    (let [parsed-args (mapv (fn [parse-fn arg-exp] (parse-fn arg-exp)) parse-fns arg-exps)]
      (when (every? some? parsed-args)
        parsed-args))))

(def ^:private one-string-arg [parse-string-exp-node])

(defmulti collect-node-info (fn [_result node] (if (seq? node) (first node) node)))

(defmethod collect-node-info :default [result _]
  result)

(defmethod collect-node-info :chunk [result node]
  (collect-node-info result (second node)))

(defmethod collect-node-info :block [result node]
  (reduce collect-node-info result (rest node)))

(defmulti collect-stat-node-info (fn [kw _result _node] kw))

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
      (if (error-node? fname)
        result
        (if-some [[parlist block] (parse-funcbody funcbody)]
          (collect-node-info (conj result {:local-functions {fname {:params (parse-parlist parlist)}}}) block)
          result)))
    ;; 'local' namelist ('=' explist)?
    (let [[_ _ namelist _ _explist] node]
      (if-let [parsed-names (parse-namelist namelist)]
        (conj result {:local-vars parsed-names})
        result))))

(defmethod collect-stat-node-info :default [_ result _node] result)

(defmethod collect-stat-node-info :varlist [_ result node]
  (let [[_ varlist _ _explist] node
        parsed-names (parse-string-vars varlist)]
    (conj result {:vars parsed-names})))

(defmethod collect-stat-node-info :function [_ result node]
  (let [[_ _ funcname funcbody] node]
    (if-let [funcname (parse-funcname funcname)]
      (let [[parlist block] (parse-funcbody funcbody)]
        (collect-node-info (conj result {:functions {funcname {:params (parse-parlist parlist)}}}) block))
      result)))

(defn- parse-hash-functioncall [node]
  (when-some [arg-exps (matching-functioncall "hash" node)]
    (when-some [[string-value] (matching-args one-string-arg arg-exps)]
      string-value)))

(defn- parse-hash-or-string-exp-node [node]
  (when-some [value (unpack-exp node)]
    (or (parse-string-node value)
        (parse-hash-functioncall value))))

(def ^:private three-hash-or-string-args (vec (repeat 3 parse-hash-or-string-exp-node)))

(defn- parse-url-functioncall [node]
  (when-some [arg-exps (matching-functioncall "msg" "url" node)]
    (case (count arg-exps)
      0 ""
      1 (parse-string-exp-node (first arg-exps))
      3 (when-some [[socket path fragment] (matching-args three-hash-or-string-args arg-exps)]
          (str socket ":" path "#" fragment)))))

(def ^:private quat-default-value [0.0 0.0 0.0]) ;; Euler angles.
(def ^:private four-number-args (vec (repeat 4 parse-number-exp-node)))

(defn- parse-quat-functioncall [node]
  (when-some [arg-exps (matching-functioncall "vmath" "quat" node)]
    (if (empty? arg-exps)
      quat-default-value
      (when-some [[x y z w] (matching-args four-number-args arg-exps)]
        (math/quat-components->euler x y z w)))))

(defn- make-vector-functioncall-parse-fn [module-name function-name component-count]
  (let [default-value (vec (repeat component-count 0.0))
        arg-parse-fns (vec (repeat component-count parse-number-exp-node))]
    (fn parse-vector-functioncall [node]
      (when-some [arg-exps (matching-functioncall module-name function-name node)]
        (case (count arg-exps)
          0 default-value
          1 (when-some [number-value (parse-number-exp-node (first arg-exps))]
              (vec (repeat component-count number-value)))
          (matching-args arg-parse-fns arg-exps))))))

(def ^:private parse-vector3-functioncall (make-vector-functioncall-parse-fn "vmath" "vector3" 3))
(def ^:private parse-vector4-functioncall (make-vector-functioncall-parse-fn "vmath" "vector4" 4))

(defn- parse-boolean-script-property-value-info [value-arg-exp]
  (when-some [boolean-value (parse-boolean-exp-node value-arg-exp)]
    {:type :property-type-boolean
     :value boolean-value}))

(defn- parse-number-script-property-value-info [value-arg-exp]
  (when-some [number-value (parse-number-exp-node value-arg-exp)]
    {:type :property-type-number
     :value number-value}))

(defn- parse-hash-script-property-value-info [value-arg-exp]
  (when-some [string-value (some-> value-arg-exp unpack-exp parse-hash-functioncall)]
    {:type :property-type-hash
     :value string-value}))

(defn- parse-url-script-property-value-info [value-arg-exp]
  (when-some [string-value (some-> value-arg-exp unpack-exp parse-url-functioncall)]
    {:type :property-type-url
     :value string-value}))

(defn- parse-vector3-script-property-value-info [value-arg-exp]
  (when-some [vector-components (some-> value-arg-exp unpack-exp parse-vector3-functioncall)]
    {:type :property-type-vector3
     :value vector-components}))

(defn- parse-vector4-script-property-value-info [value-arg-exp]
  (when-some [vector-components (some-> value-arg-exp unpack-exp parse-vector4-functioncall)]
    {:type :property-type-vector4
     :value vector-components}))

(defn- parse-quat-script-property-value-info [value-arg-exp]
  (when-some [euler-angles (some-> value-arg-exp unpack-exp parse-quat-functioncall)]
    {:type :property-type-quat
     :value euler-angles}))

(defn- parse-script-property-value-info [value-arg-exp]
  (when (some? value-arg-exp)
    (or (parse-boolean-script-property-value-info value-arg-exp)
        (parse-number-script-property-value-info value-arg-exp)
        (parse-hash-script-property-value-info value-arg-exp)
        (parse-url-script-property-value-info value-arg-exp)
        (parse-vector3-script-property-value-info value-arg-exp)
        (parse-vector4-script-property-value-info value-arg-exp)
        (parse-quat-script-property-value-info value-arg-exp))))

(defn- parse-script-property-name-info [name-arg-exp]
  (when-some [name (parse-string-exp-node name-arg-exp)]
    {:name name}))

(defn- parse-script-property-declaration [arg-exps]
  (let [name-info (parse-script-property-name-info (first arg-exps))
        value-info (parse-script-property-value-info (second arg-exps))
        status-info (cond
                      (or (nil? name-info) (< (count arg-exps) 2))
                      {:status :invalid-args}

                      (nil? value-info)
                      {:status :invalid-value}

                      :else
                      {:status :ok})]
    (merge name-info value-info status-info)))

(defmethod collect-stat-node-info :functioncall [_ result node]
  (let [functioncall (second node)]
    (let [[module-name function-name arg-exps] (parse-functioncall functioncall)]
      (cond
        (and (= "go" module-name) (= "property" function-name))
        (if-some [script-property (parse-script-property-declaration arg-exps)]
          (conj result {:script-properties script-property})
          result)

        :else
        result))))

(defn collect-info [node]
  (collect-node-info [] node))

(defn- parse-binding-forms [stat-node]
  (when (= :stat (first stat-node))
    (if (= "local" (second stat-node))
      (when-some [namelist-node (matching-type :namelist (nth stat-node 2))]
        (when (= "=" (nth stat-node 3))
          (into []
                (remove comma-node?)
                (next namelist-node))))
      (when-some [varlist-node (matching-type :varlist (second stat-node))]
        (when (= "=" (nth stat-node 2))
          (into []
                (comp (keep (partial matching-type :var))
                      (map second))
                (next varlist-node)))))))

(defn- matching-node-path [tags node-path]
  (when (and (= (count tags) (count node-path))
             (every? some? (map matching-type tags node-path)))
    node-path))

(def ^:private exp-binding-path-type [:exp :explist :stat :block :chunk])

(defn- parse-exp-binding [exp-node-path]
  (when-some [[exp explist stat] (matching-node-path exp-binding-path-type exp-node-path)]
    (some identity
          (map (fn [binding-form binding-exp]
                 (when (identical? exp binding-exp)
                   binding-form))
               (parse-binding-forms stat)
               (remove comma-node? (next explist))))))

(defn- parse-requires [node-path node]
  (lazy-seq
    (if-some [[module-name] (matching-args one-string-arg (matching-functioncall "require" node))]
      (list [(parse-exp-binding node-path) module-name])
      (when (seq? node)
        (mapcat (partial parse-requires (cons node node-path))
                (next node))))))

(defn lua-info [code]
  (let [tree (lua-parser code)
        info (collect-info tree)
        local-vars-info (into #{} (apply concat (map :local-vars info)))
        vars-info (set/difference (into #{} (apply concat (map :vars info))) local-vars-info)
        functions-info (or (apply merge (map :functions info)) {})
        local-functions-info (or (apply merge (map :local-functions info)) {})
        requires-info (vec (distinct (parse-requires (list) tree)))
        script-properties-info (into [] (keep :script-properties) info)]
    {:vars vars-info
     :local-vars local-vars-info
     :functions functions-info
     :local-functions local-functions-info
     :requires requires-info
     :script-properties script-properties-info}))
