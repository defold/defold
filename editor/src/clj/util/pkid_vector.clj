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

(ns util.pkid-vector
  "A persistent vector with stable pkids that are independent of realized indexes.

  Ordinary vector operations use realized indexes, while assoc-pkids,
  dissoc-pkids, and find-pkids use stable pkids. Vector equality and hashing
  consider only realized values. Generic transient conversion is unsupported."
  (:refer-clojure :exclude [vals])
  (:require [clojure.data.int-map :as int-map])
  (:import [clojure.lang PkidVector PersistentVector]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(def ^:private empty-pkid-vector
  (PkidVector. PersistentVector/EMPTY (int-map/int-set) 0))

(defn pkid-vector
  "Returns an empty pkid vector."
  ^PkidVector []
  empty-pkid-vector)

(definline vals
  "Returns the realized values vector."
  [^PkidVector vector]
  (with-meta vector {:tag `PkidVector}))

(definline next-pkid
  "Returns the pkid that will be assigned by the next append."
  [^PkidVector vector]
  `(let [^PkidVector vector# ~vector]
     (.-nextPkid vector#)))

(definline append
  "Appends a value at the next pkid."
  ^PkidVector [^PkidVector vector val]
  `(let [^PkidVector vector# ~vector]
     (.append vector# ~val)))

(definline assoc-pkids
  "Associates a value with every supplied pkid."
  ^PkidVector [^PkidVector vector pkids val]
  `(let [^PkidVector vector# ~vector]
     (.assocPkids vector# ~pkids ~val)))

(definline dissoc-pkids
  "Dissociates the values at the supplied pkids."
  ^PkidVector [^PkidVector vector pkids]
  `(let [^PkidVector vector# ~vector]
     (.dissocPkids vector# ~pkids)))

(definline find-pkids
  "Returns the pkids whose values equal the supplied value."
  [^PkidVector vector val]
  `(let [^PkidVector vector# ~vector]
     (.findPkids vector# ~val)))
