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
