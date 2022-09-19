;; Copyright 2020-2022 The Defold Foundation
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

(ns editor.util
  (:require [clojure.string :as string])
  (:import [com.dynamo.bob Platform]
           [java.util Locale Comparator]))

(set! *warn-on-reflection* true)

(defmacro spy
  [& body]
  `(let [ret# (try ~@body (catch Throwable t# (prn t#) (throw t#)))]
     (prn ret#)
     ret#))


;; See http://mattryall.net/blog/2009/02/the-infamous-turkish-locale-bug

(defn lower-case*
  "Like clojure.string/lower-case but using root locale."
  [^CharSequence s]
  (.. s toString (toLowerCase Locale/ROOT)))

(defn upper-case*
  "Like clojure.string/upper-case but using root locale."
  [^CharSequence s]
  (.. s toString (toUpperCase Locale/ROOT)))

(defn capitalize*
  "Like clojure.string/capitalize but using root locale."
  ^String [^CharSequence s]
  (let [s (.toString s)]
    (if (< (count s) 2)
      (.toUpperCase s Locale/ROOT)
      (str (.toUpperCase (subs s 0 1) Locale/ROOT)
           (.toLowerCase (subs s 1) Locale/ROOT)))))

(defn format*
  "Like clojure.core/format but using root locale."
  ^String [fmt & args]
  (String/format Locale/ROOT fmt (to-array args)))

(def ^Comparator natural-order
  (reify Comparator
    (compare [_ a b]
      (if (and a b)
        ;; compare code point by code point
        ;; if something starts a digit, read the whole number and then compare
        (let [^String a a
              ^String b b
              a-len (.length a)
              b-len (.length b)]
          (loop [a-i 0
                 b-i 0]
            (if (or (== a-i a-len) (== b-i b-len))
              ;; end of a or b, sort shorter first
              (unchecked-subtract
                (unchecked-subtract a-len a-i)
                (unchecked-subtract b-len b-i))
              ;; compare code points
              (let [a-cp (.codePointAt a a-i)
                    a-digit (Character/isDigit a-cp)
                    b-cp (.codePointAt b b-i)
                    b-digit (Character/isDigit b-cp)]
                (if (= a-digit b-digit)
                  (if a-digit
                    ;; compare numbers: read whole numbers, advance indices
                    (let [a-sb (doto (StringBuilder.) (.appendCodePoint a-cp))
                          next-a-i (loop [i (unchecked-inc a-i)]
                                     (if (== i a-len)
                                       i
                                       (let [cp (.codePointAt a i)]
                                         (if (Character/isDigit cp)
                                           (do (.appendCodePoint a-sb cp)
                                               (recur (unchecked-inc i)))
                                           i))))
                          a-num (BigInteger. (.toString a-sb))
                          b-sb (doto (StringBuilder.) (.appendCodePoint b-cp))
                          next-b-i (loop [i (unchecked-inc b-i)]
                                     (if (== i b-len)
                                       i
                                       (let [cp (.codePointAt b i)]
                                         (if (Character/isDigit cp)
                                           (do (.appendCodePoint b-sb cp)
                                               (recur (unchecked-inc i)))
                                           i))))
                          b-num (BigInteger. (.toString b-sb))
                          ret (compare a-num b-num)]
                      (if (zero? ret)
                        (recur (long next-a-i) (long next-b-i))
                        ret))
                    ;; compare as alphanumeric chars
                    (let [ret (compare (Character/toLowerCase a-cp) (Character/toLowerCase b-cp))]
                      (if (zero? ret)
                        (recur (unchecked-inc a-i) (unchecked-inc b-i))
                        ret)))
                  ;; both digits and non-digits, simple compare
                  (compare a-cp b-cp))))))
        (compare a b)))))

(defn comparator-chain
  ([^Comparator c1 ^Comparator c2]
   (reify Comparator
     (compare [_ a b]
       (let [ret (.compare c1 a b)]
         (if (zero? ret)
           (.compare c2 a b)
           ret)))))
  ([^Comparator c1 ^Comparator c2 ^Comparator c3]
   (reify Comparator
     (compare [_ a b]
       (let [ret (.compare c1 a b)]
         (if (zero? ret)
           (let [ret (.compare c2 a b)]
             (if (zero? ret)
               (.compare c3 a b)
               ret))
           ret))))))

(defn comparator-on
  ([f]
   (reify Comparator
     (compare [_ a b]
       (compare (f a) (f b)))))
  ([^Comparator c f]
   (reify Comparator
     (compare [_ a b]
       (.compare c (f a) (f b))))))

(defn os-raw
  "Returns :win32, :macos or :linux"
  []
  (keyword (.. Platform getHostPlatform getOs)))

(def os (memoize os-raw))

(defn is-mac-os? []
  (= (os) :macos))

(defn is-linux? []
  (= (os) :linux))

(defn positions [pred coll]
  (keep-indexed (fn [idx x]
                  (when (pred x)
                    idx))
                coll))

(defn join-words
  "Returns a string joining the elements in coll together in such a way that
  they form valid english syntax. Examples:
  (join-words ', ' ' or ' []) => ''
  (join-words ', ' ' or ' ['a']) => 'a'
  (join-words ', ' ' or ' ['a' 'b']) => 'a or b'
  (join-words ', ' ' or ' ['a' 'b' 'c']) => 'a, b or c'"
  ^String [separator final-separator coll]
  (string/join final-separator
               (concat
                 (some->> coll
                          butlast
                          not-empty
                          (string/join separator)
                          list)
                 (some->> coll
                          last
                          list))))

(defn dissoc-in
  "Dissociates an entry from a nested associative structure returning a new
  nested structure. k & ks is a sequence of keys. Any empty maps that result
  will not be present in the new structure."
  [m [k & ks]]
  (if ks
    (if-let [child (get m k)]
      (let [new-child (dissoc-in child ks)]
        (if (zero? (count new-child))
          (dissoc m k)
          (assoc m k new-child)))
      m)
    (dissoc m k)))
