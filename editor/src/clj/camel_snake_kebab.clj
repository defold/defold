(ns camel-snake-kebab
  "Original code (C) and thanks to  Christoffer Sawicki & ToBeReplaced.

  See https://github.com/qerub/camel-snake-kebab"
  (:require [clojure.string :refer [split join]]
            [editor.util :refer [lower-case* upper-case* capitalize*] :rename {lower-case* lower-case
                                                                               upper-case* upper-case
                                                                               capitalize* capitalize}])
  (:import  (clojure.lang Keyword Symbol)))

(def ^:private upper-case-http-headers
  #{"CSP" "ATT" "WAP" "IP" "HTTP" "CPU" "DNT" "SSL" "UA" "TE" "WWW" "XSS" "MD5"})

(defn ^:private capitalize-http-header [s]
  (or (upper-case-http-headers (upper-case s))
      (capitalize s)))

(def ^:private word-separator-pattern
  "A pattern that matches all known word separators."
  (->> ["\\s+" "_" "-"
        "(?<=[A-Z])(?=[A-Z][a-z])"
        "(?<=[^A-Z_-])(?=[A-Z])"
        "(?<=[A-Za-z])(?=[^A-Za-z])"]
       (join "|")
       re-pattern))

(defn convert-case [first-fn rest-fn sep s]
  "Converts the case of a string according to the rule for the first
  word, remaining words, and the separator."
  (let [[first & rest] (split s word-separator-pattern)]
    (join sep (cons (first-fn first) (map rest-fn rest)))))

(def ^:private case-conversion-rules
  "The formatting rules for each case."
  {"CamelCase"        [capitalize capitalize "" ]
   "Camel_Snake_Case" [capitalize capitalize "_"]
   "camelCase"        [lower-case capitalize "" ]
   "Snake_case"       [capitalize lower-case "_"]
   "SNAKE_CASE"       [upper-case upper-case "_"]
   "snake_case"       [lower-case lower-case "_"]
   "kebab-case"       [lower-case lower-case "-"]
   "HTTP-Header-Case" [capitalize-http-header capitalize-http-header "-"]})

(defprotocol AlterName
  (alter-name [this f] "Alters the name of this with f."))

(extend-protocol AlterName
  String  (alter-name [this f]
            (-> this f))
  Keyword (alter-name [this f]
            (if (namespace this)
              (throw (ex-info "Namespaced keywords are not supported" {:input this}))
              (-> this name f keyword)))
  Symbol  (alter-name [this f]
            (if (namespace this)
              (throw (ex-info "Namespaced symbols are not supported" {:input this}))
              (-> this name f symbol))))

(doseq [[case-label [first-fn rest-fn sep]] case-conversion-rules]
  (letfn [(convert-case* [s] (convert-case first-fn rest-fn sep s))
          (make-name [type-label] (->> [case-label type-label]
                                       (join \space)
                                       (convert-case*)
                                       (format "->%s")
                                       (symbol)))]
    ;; The type-preserving function
    (intern *ns*
            (->> case-label (format "->%s") symbol)
            #(alter-name % convert-case*))
    ;; The type-converting functions
    (doseq [[type-label type-converter] {"string" identity "symbol" symbol "keyword" keyword}]
      (intern *ns*
              (make-name type-label)
              (comp type-converter convert-case* name)))))
