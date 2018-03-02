(ns editor.field-expression
  (:require [clojure.string :as string]
            [editor.math :as math])
  (:import (java.math MathContext)
           (java.text DecimalFormat DecimalFormatSymbols)
           (java.util Locale)))

(set! *warn-on-reflection* true)

(defn- parsable-number-format [text]
  (-> text
      (string/replace \, \.)))

(defn- evaluate-expression [parse-fn precision text]
  (if-let [matches (re-find #"(.+?)\s*((?<![Ee])[\+\-*/])\s*(.+)" text)]
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

(defn format-int
  ^String [n]
  (if (nil? n) "" (str n)))

(defn to-double [s]
 (try
   (evaluate-expression #(Double/parseDouble %) 0.01 s)
   (catch Throwable _
     nil)))

(def ^:private ^DecimalFormat double-decimal-format
  (doto (DecimalFormat. "0"
                        (doto (DecimalFormatSymbols. Locale/ROOT)
                          (.setInfinity (Double/toString Double/POSITIVE_INFINITY))
                          (.setNaN (Double/toString Double/NaN))))
    ;; Upper limit on fraction digits for a Java double.
    ;; Taken from private constant DecimalFormat/DOUBLE_FRACTION_DIGITS.
    (.setMaximumFractionDigits 340)))

(defn- format-double
  ^String [n]
  (if (nil? n) "" (.format double-decimal-format n)))

(defonce ^:private nan-string (Float/toString Float/NaN))
(defonce ^:private positive-infinity-string (Float/toString Float/POSITIVE_INFINITY))
(defonce ^:private negative-infinity-string (Float/toString Float/NEGATIVE_INFINITY))

(defn- format-float
  ^String [n]
  ;; NOTE: The NaN and Infinity tests are also true for Double NaN and Infinity values.
  (cond
    (nil? n) ""
    (Float/isNaN n) nan-string
    (= Float/POSITIVE_INFINITY n) positive-infinity-string
    (= Float/NEGATIVE_INFINITY n) negative-infinity-string
    :else (format-double (.doubleValue (.round (BigDecimal. (double n)) MathContext/DECIMAL32)))))

(defn format-number
  ^String [n]
  (cond
    (nil? n) ""
    (integer? n) (format-int n)
    (instance? Float n) (format-float n)
    :else (format-double n)))
