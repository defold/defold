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
  "A persistent vector with stable pkids that are independent of realized
  indexes.

  Ordinary vector operations use realized indexes, while assoc-pkids,
  dissoc-pkids, find-pkids, and find-pkids-by-value use stable pkids. Vector
  equality and hashing consider only realized values. Generic transient
  conversion is unsupported."
  (:import [clojure.lang IPersistentSet PkidVector]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(definline pkid-vector
  "Returns an empty pkid-vector."
  ^PkidVector []
  `PkidVector/EMPTY)

(definline next-pkid
  "Returns the pkid that will be assigned to the next conjoined value."
  [^PkidVector pkid-vector]
  `(let [^PkidVector pkid-vector# ~pkid-vector]
     (.-nextPkid pkid-vector#)))

(definline assoc-pkids
  "Associates a value with every supplied pkid."
  ^PkidVector [^PkidVector pkid-vector pkids value]
  `(let [^PkidVector vector# ~pkid-vector]
     (.assocPkids vector# ~pkids ~value)))

(definline dissoc-pkids
  "Dissociates the values at the supplied pkids."
  ^PkidVector [^PkidVector pkid-vector pkids]
  `(let [^PkidVector pkid-vector# ~pkid-vector]
     (.dissocPkids pkid-vector# ~pkids)))

(definline find-pkids
  "Returns a sorted regular vector of the pkids whose values equal the supplied
  value."
  [^PkidVector pkid-vector value]
  `(let [^PkidVector pkid-vector# ~pkid-vector]
     (.findPkids pkid-vector# ~value)))

(definline find-pkids-by-value
  "Returns a map from each requested value found in the pkid-vector to a sorted
  regular vector of its pkids."
  [^PkidVector pkid-vector ^IPersistentSet values]
  `(let [^PkidVector pkid-vector# ~pkid-vector
         ^IPersistentSet values# ~values]
     (.findPkidsByValue pkid-vector# values#)))
