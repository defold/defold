(ns editor.field-expression
  (:require [clojure.string :as string]
            [editor.math :as math]))

(defn- parsable-number-format [text]
  (-> text
      (string/replace \, \.)))

(defn- evaluate-expression [parse-fn precision text]
  (if-let [matches (re-find #"(.+?)\s*([\+\-*/])\s*(.+)" text)]
    (let [a (parse-fn (parsable-number-format (matches 1)))
          b (parse-fn (parsable-number-format (matches 3)))
          op (resolve (symbol (matches 2)))]
      (math/round-with-precision (op a b) precision))
    (parse-fn (parsable-number-format text))))

(defn to-int [s]
  (try
    (int (evaluate-expression #(Integer/parseInt %) 1.0 s))
    (catch Throwable _
      nil)))

(defn to-double [s]
 (try
   (evaluate-expression #(Double/parseDouble %) 0.01 s)
   (catch Throwable _
     nil)))
