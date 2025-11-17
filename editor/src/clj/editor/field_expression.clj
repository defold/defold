;; Copyright 2020-2025 The Defold Foundation
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
  (:import [net.objecthunter.exp4j ExpressionBuilder]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

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

(defn to-long [s]
  (try
    (Long/parseLong s)
    (catch Throwable _
      (try
        (-> s evaluate-expression long)
        (catch Throwable _
          nil)))))

(defn to-float [s]
  (try
    (Float/parseFloat s)
    (catch Throwable _
      (try
        (float (math/round-with-precision (evaluate-expression s) math/precision-general))
        (catch Throwable _
          nil)))))

(defn to-double [s]
  (try
    (Double/parseDouble s)
    (catch Throwable _
      (try
        (math/round-with-precision (evaluate-expression s) math/precision-general)
        (catch Throwable _
          nil)))))

(def ^:private ^:const ^int decimal-point-int (int \.))

(def ^:private ^:const ^int minus-sign-int (int \-))

(def ^:private ^:const ^int upper-case-e-int (int \E))

(def ^:private ^:const ^int zero-char-int (int \0))

(defn- last-index-of-significant-digit
  ^long [^String num-str ^long start-index]
  (loop [index start-index]
    (let [code-point (.codePointAt num-str index)]
      (if (= zero-char-int code-point)
        (recur (dec index))
        index))))

(defn- input-digit-code-point
  ^long [^String num-str ^long digit-index ^long index-of-decimal-point ^long index-of-last-decimal-digit]
  (if (or (neg? digit-index)
          (zero? (.length num-str)))
    zero-char-int
    (let [is-negative (= \- (.charAt num-str 0))
          index (if is-negative (inc digit-index) digit-index)]
      (cond
        (< index index-of-decimal-point)
        (.codePointAt num-str index)

        (< index index-of-last-decimal-digit)
        (.codePointAt num-str (inc index))

        :else
        zero-char-int))))

(defn- scientific->decimal
  "Convert a string representation of a floating point number expressed in
  scientific notation to plain decimal notation. E.g. 1.0E-5 => 0.00001.

  Yes, this is a weird way to go about things, but previous attempts using
  String.format, NumberFormat, DecimalFormat, or BigDecimal with MathContext
  all fail to produce satisfactory results for all inputs. Please do not use
  this for anything apart from presenting values in the UI."
  ^String [^String num-str]
  (let [index-of-e (.lastIndexOf num-str upper-case-e-int)]
    (if (= -1 index-of-e)
      num-str
      (let [is-negative (= minus-sign-int (.codePointAt num-str 0))
            decimal-point-offset (Integer/parseInt num-str (inc index-of-e) (.length num-str) 10)
            index-of-decimal-point (.indexOf num-str decimal-point-int)
            index-of-adjusted-decimal-point (cond-> (+ index-of-decimal-point decimal-point-offset)
                                                    is-negative dec)
            index-of-last-significant-decimal-digit (last-index-of-significant-digit num-str (dec index-of-e))
            end-index (cond-> (max index-of-last-significant-decimal-digit
                                   (+ 1 index-of-decimal-point decimal-point-offset))
                              is-negative dec)
            read-offset (min 0 (cond-> (+ -1 index-of-decimal-point decimal-point-offset)
                                       is-negative dec))
            string-builder (StringBuilder. 32)]
        (when is-negative
          (.appendCodePoint string-builder minus-sign-int))
        (loop [index read-offset]
          (when (< index end-index)
            (when (= index-of-adjusted-decimal-point index)
              (.appendCodePoint string-builder decimal-point-int))
            (let [code-point (input-digit-code-point num-str index index-of-decimal-point index-of-last-significant-decimal-digit)]
              (.appendCodePoint string-builder code-point)
              (recur (inc index)))))
        (.toString string-builder)))))

(def format-int str)

(defn format-real
  ^String [n]
  (if n
    (scientific->decimal (str n))
    ""))

(defn format-number
  ^String [n]
  (cond
    (nil? n) ""
    (integer? n) (format-int n)
    :else (format-real n)))
