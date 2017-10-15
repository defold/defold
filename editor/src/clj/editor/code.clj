(ns editor.code
  (:require [clojure.string :as string])
  (:import [com.defold.editor.eclipse DefoldRuleBasedScanner DefoldRuleBasedScanner$DocumentCharSequence]))

(set! *warn-on-reflection* true)

(defn control-char-or-delete [key-typed]
 ;; ascii control chars like Enter are all below 32
  ;; delete is an exception and is 127
  (let [n (.charAt ^String key-typed 0)]
    (or (< (int n) 32)
        (= (int n) 127))))


(defn lf-normalize-line-endings [string]
  (loop [normalized string]
    (let [next (string/replace normalized "\r\n" "\n")]
      (if (= next normalized)
        (string/replace normalized "\r" "\n")
        (recur next)))))

(defn- remove-optional-params [s]
  (string/replace s #"\[.*\]" ""))

(defn create-hint
  ([type name]
   (create-hint type name name name nil nil))
  ([type name display-string insert-string doc tab-triggers]
   (cond-> {:type type
            :name name
            :display-string display-string
            :insert-string insert-string}

           (not-empty doc)
           (assoc :doc doc)

           (some? tab-triggers)
           (assoc :tab-triggers tab-triggers))))

(defn- sub-sequence [^CharSequence code begin]
  (.subSequence code begin (.length code)))

(defn match-while [code p]
  (loop [body (seq code)
         length 0]
    (if-let [c (first body)]
      (if (p c)
        (recur (rest body) (inc length))
        {:body (sub-sequence code length) :length length})
      {:body (sub-sequence code length) :length length})))

(defn match-while-eq [code ch]
  (match-while code (fn [c] (= c ch))))

(defn match-until-string [code s]
  (if-let [s (seq s)]
    (let [slen (count s)]
      (when-let [index (ffirst (->> (partition slen 1 code)
                                    (map-indexed vector)
                                    (filter #(= (second %) s))))]
        (let [length (+ index slen)]
          {:body (sub-sequence code length) :length length})))
    {:body code :length 0}))

(defn match-until-eol [code]
  (match-while code (fn [ch] (and ch (not (#{\newline \return} ch))))))

(defn match-string [^DefoldRuleBasedScanner$DocumentCharSequence code ^String s]
  (let [slen (.length s)]
    (if (> slen 0)
      (when (.matchString code s)
        {:body (sub-sequence code slen) :length slen})
      {:body code :length 0})))

(defn combine-matches [& matches]
  {:body (last matches) :length (apply + (map :length matches))})

(defn match-regex [^CharSequence code pattern]
  (when-let [result (re-find pattern code)]
    (if (string? result)
      {:body (sub-sequence code (count result)) :length (count result)}
      (let [first-group-match (->> result (remove nil?) first)]
        {:body (sub-sequence code (count first-group-match)) :length (count first-group-match)}))))

(def number-pattern   #"^-?(?:0|[1-9]\d*)(?:\.\d*)?(?:[eE][+\-]?\d+)?" )
(def leading-decimal-number-pattern   #"^-?(?:\.\d*)(?:[eE][+\-]?\d+)?" )

(defn match-number [code]
  (or (match-regex code number-pattern)
      (match-regex code leading-decimal-number-pattern)))

(def ^:private pattern1 #"([a-zA-Z0-9_]+)([\\(]?)$")
(def ^:private pattern2 #"([a-zA-Z0-9_]+)\.([a-zA-Z0-9_]*)([\\(]?)$")

(defn- make-parse-result [namespace function in-function start end]
  {:namespace namespace
   :function function
   :in-function in-function
   :start start
   :end end})

(defn default-parse-result []
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
      (parse-unscoped-line line)
      (default-parse-result)))

(defn proposal-filter-pattern [namespace function]
  (if (= "" (or namespace ""))
    function
    (str namespace "." function)))
