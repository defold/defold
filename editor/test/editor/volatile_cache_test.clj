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

(ns editor.volatile-cache-test
  (:require [clojure.test :refer :all]
            [clojure.core.cache :as cache]
            [editor.volatile-cache :as volatile-cache]))

(defn- encache [c k v]
  (cache/miss c k v))

(deftest pruning []
  (let [c (volatile-cache/volatile-cache-factory {})]
    (let [c' (encache c :a 1)
          _ (is (cache/has? c' :a))
          c' (volatile-cache/prune c')
          _ (is (cache/has? c' :a))
          c' (volatile-cache/prune c')]
      (is (not (cache/has? c' :a))))))
