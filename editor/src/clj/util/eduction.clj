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

(ns util.eduction
  (:refer-clojure :exclude [dedupe distinct drop drop-while filter interpose keep keep-indexed map map-indexed mapcat partition-all partition-by random-sample remove replace take take-nth take-while]))

(definline dedupe [coll]
  `(eduction
     (clojure.core/dedupe)
     ~coll))

(definline distinct [coll]
  `(eduction
     (clojure.core/distinct)
     ~coll))

(definline drop [n coll]
  `(eduction
     (clojure.core/drop ~n)
     ~coll))

(definline drop-while [pred coll]
  `(eduction
     (clojure.core/drop-while ~pred)
     ~coll))

(definline filter [pred coll]
  `(eduction
     (clojure.core/filter ~pred)
     ~coll))

(definline interpose [sep coll]
  `(eduction
     (clojure.core/interpose ~sep)
     ~coll))

(definline keep [f coll]
  `(eduction
     (clojure.core/keep ~f)
     ~coll))

(definline keep-indexed [f coll]
  `(eduction
     (clojure.core/keep-indexed ~f)
     ~coll))

(definline map [f coll]
  `(eduction
     (clojure.core/map ~f)
     ~coll))

(definline map-indexed [f coll]
  `(eduction
     (clojure.core/map-indexed ~f)
     ~coll))

(definline mapcat [f coll]
  `(eduction
     (clojure.core/mapcat ~f)
     ~coll))

(definline partition-all [n coll]
  `(eduction
     (clojure.core/partition-all ~n)
     ~coll))

(definline partition-by [f coll]
  `(eduction
     (clojure.core/partition-by ~f)
     ~coll))

(definline random-sample [prob coll]
  `(eduction
     (clojure.core/random-sample ~prob)
     ~coll))

(definline remove [pred coll]
  `(eduction
     (clojure.core/remove ~pred)
     ~coll))

(definline replace [smap coll]
  `(eduction
     (clojure.core/replace ~smap)
     ~coll))

(definline take [n coll]
  `(eduction
     (clojure.core/take ~n)
     ~coll))

(definline take-nth [n coll]
  `(eduction
     (clojure.core/take-nth ~n)
     ~coll))

(definline take-while [pred coll]
  `(eduction
     (clojure.core/take-while ~pred)
     ~coll))
