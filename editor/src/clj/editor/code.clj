(ns editor.code
  (:require [clojure.string :as string]))

(defn not-ascii-or-delete [key-typed]
 ;; ascii control chars like Enter are all below 32
  ;; delete is an exception and is 127
  (let [n (.charAt ^String key-typed 0)]
    (and (> (int n) 31) (not= (int n) 127))))

(defn- remove-optional-params [s]
  (string/replace s #"\[.*\]" ""))

(defn create-hint
  ([name]
   (create-hint name name name ""))
  ([name display-string insert-string doc]
   {:name name
    :display-string display-string
    :insert-string insert-string
    :doc doc}))

(defn match-while [body p]
  (loop [body body
         length 0]
    (if-let [c (first body)]
      (if (p c)
        (recur (rest body) (inc length))
        {:body body :length length})
      {:body body :length length})))

(defn match-while-eq [body ch]
  (match-while body (fn [c] (= c ch))))

(defn match-until-string [body s]
  (if-let [s (seq s)]
    (let [slen (count s)]
      (when-let [index (ffirst (->> (partition slen 1 body)
                                    (map-indexed vector)
                                    (filter #(= (second %) s))))]
        (let [length (+ index slen)]
          {:body (nthrest body length) :length length})))
    {:body body :length 0}))

(defn match-until-eol [body]
  (match-while body (fn [ch] (and ch (not (#{\newline \return} ch))))))

(defn match-string [body s]
  (if-let [s (seq s)]
    (let [length (count s)]
      (when (= s (take length body))
        {:body (nthrest body length) :length length}))
    {:body body :length 0}))

(defn combine-matches [& matches]
  {:body (last matches) :length (apply + (map :length matches))})

(defn match-regex [pattern s]
(when-let [result (re-find pattern s)]
    (if (string? result)
      {:body result :length (count result)}
      (let [first-group-match (->> result (remove nil?) first)]
        {:body first-group-match :length (count first-group-match)}))))

(def number-pattern   #"^-?(?:0|[1-9]\d*)(?:\.\d*)?(?:[eE][+\-]?\d+)?" )
(def leading-decimal-number-pattern   #"^-?(?:\.\d*)(?:[eE][+\-]?\d+)?" )

(defn match-number [s]
  (or (match-regex number-pattern s)
      (match-regex leading-decimal-number-pattern s)))

(def ^:private pattern1 #"([a-zA-Z0-9_]+)([\\(]?)")
(def ^:private pattern2 #"([a-zA-Z0-9_]+)\.([a-zA-Z0-9_]*)([\\(]?)")

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
