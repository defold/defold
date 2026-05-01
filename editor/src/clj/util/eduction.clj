;; Copyright 2020-2026 The Defold Foundation
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

(ns util.eduction
  (:refer-clojure :exclude [cat concat conj cons dedupe distinct drop drop-while filter interpose keep keep-indexed map map-indexed mapcat partition-all partition-by random-sample remove replace take take-nth take-while])
  (:require [util.array :as array])
  (:import [clojure.core Eduction]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(defonce empty-eduction (eduction))

(defn eduction? [value]
  (instance? Eduction value))

(definline cat [coll]
  `(->Eduction
     clojure.core/cat
     ~coll))

(defn concat
  ([] empty-eduction)
  ([a] a)
  ([a b]
   (->Eduction
     clojure.core/cat
     (array/of a b)))
  ([a b c]
   (->Eduction
     clojure.core/cat
     (array/of a b c)))
  ([a b c d]
   (->Eduction
     clojure.core/cat
     (array/of a b c d)))
  ([a b c d & more]
   (->Eduction
     clojure.core/cat
     (apply array/of a b c d more))))

(defn conj
  ([] empty-eduction)
  ([coll] coll)
  ([coll item]
   (->Eduction
     clojure.core/cat
     (array/of
       coll
       (array/of item))))
  ([coll item & more]
   (->Eduction
     clojure.core/cat
     (array/of
       coll
       (array/of item)
       (object-array more)))))

(defn cons [item coll]
  (->Eduction
    clojure.core/cat
    (array/of (array/of item)
              coll)))

(definline dedupe [coll]
  `(->Eduction
     (clojure.core/dedupe)
     ~coll))

(definline distinct [coll]
  `(->Eduction
     (clojure.core/distinct)
     ~coll))

(definline drop [n coll]
  `(->Eduction
     (clojure.core/drop ~n)
     ~coll))

(definline drop-while [pred coll]
  `(->Eduction
     (clojure.core/drop-while ~pred)
     ~coll))

(definline filter [pred coll]
  `(->Eduction
     (clojure.core/filter ~pred)
     ~coll))

(definline interpose [sep coll]
  `(->Eduction
     (clojure.core/interpose ~sep)
     ~coll))

(definline keep [f coll]
  `(->Eduction
     (clojure.core/keep ~f)
     ~coll))

(definline keep-indexed [f coll]
  `(->Eduction
     (clojure.core/keep-indexed ~f)
     ~coll))

(definline map [f coll]
  `(->Eduction
     (clojure.core/map ~f)
     ~coll))

(definline map-indexed [f coll]
  `(->Eduction
     (clojure.core/map-indexed ~f)
     ~coll))

(definline mapcat [f coll]
  `(->Eduction
     (clojure.core/mapcat ~f)
     ~coll))

(definline mapcat-indexed [f coll]
  `(->Eduction
     (comp (clojure.core/map-indexed ~f) clojure.core/cat)
     ~coll))

(definline partition-all [n coll]
  `(->Eduction
     (clojure.core/partition-all ~n)
     ~coll))

(definline partition-by [f coll]
  `(->Eduction
     (clojure.core/partition-by ~f)
     ~coll))

(definline random-sample [prob coll]
  `(->Eduction
     (clojure.core/random-sample ~prob)
     ~coll))

(definline remove [pred coll]
  `(->Eduction
     (clojure.core/remove ~pred)
     ~coll))

(definline replace [smap coll]
  `(->Eduction
     (clojure.core/replace ~smap)
     ~coll))

(definline take [n coll]
  `(->Eduction
     (clojure.core/take ~n)
     ~coll))

(definline take-nth [n coll]
  `(->Eduction
     (clojure.core/take-nth ~n)
     ~coll))

(definline take-while [pred coll]
  `(->Eduction
     (clojure.core/take-while ~pred)
     ~coll))
