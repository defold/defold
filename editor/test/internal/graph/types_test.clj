;; Copyright 2020 The Defold Foundation
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

(ns internal.graph.types-test
  (:require [clojure.test.check :as tc]
            [clojure.test.check.clojure-test :refer [defspec]]
            [clojure.test.check.generators :as gen]
            [clojure.test.check.properties :as prop]
            [clojure.test :refer :all]
            [internal.graph.types :as gt]))

(defspec unpacking
  100000
  (prop/for-all [g (gen/choose 0 (dec (Math/pow 2 gt/GID-BITS)))
                 n (gen/choose 0 (dec (Math/pow 2 gt/NID-BITS)))]
                (let [id (gt/make-node-id g n)]
                  (and
                   (= g (gt/node-id->graph-id id))
                   (= n (gt/node-id->nid id))))))
