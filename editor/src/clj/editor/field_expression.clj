;; Copyright 2020-2024 The Defold Foundation
;; Copyright 2014-2020 King
;; Copyright 2009-2014 Ragnar Svensson, Christian Murray
;; Licensed under the Defold License version 1.0 (the "License"); you may not use
;; this file except in compliance with the License.
;; 
;; You may obtain a copy of the License, together with FAQs at
;; https://www.defold.com/license
;; 
;; Unless required by applicable law or agreed to in writing, software distributed
;; under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
;; CONDITIONS OF ANY KIND, either express or implied. See the License for the
;; specific language governing permissions and limitations under the License.

(ns editor.field-expression
  (:require [editor.math :as math])
  (:import [java.math MathContext]
           [java.text DecimalFormat DecimalFormatSymbols]
           [java.util Locale]
           [net.objecthunter.exp4j ExpressionBuilder]))

(set! *warn-on-reflection* true)

(defn- evaluate-expression [text]
  (try
    (-> (ExpressionBuilder. text) .build .evaluate)
    (catch Throwable _
      nil)))

(defn to-int [s]
  (try
    (Integer/parseInt s)
    (catch Throwable _
      (try
        (-> s evaluate-expression int)
        (catch Throwable _
          nil)))))

(defn format-int
  ^String [n]
  (if (nil? n) "" (str n)))

(defn to-double [s]
  (try
    (Double/parseDouble s)
    (catch Throwable _
      (try
        (math/round-with-precision (evaluate-expression s) math/precision-general)
        (catch Throwable _
          nil)))))

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
