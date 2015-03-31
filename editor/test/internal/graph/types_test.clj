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
                (let [id (gt/make-nref g n)]
                  (and
                   (= g (gt/nref->gid id))
                   (= n (gt/nref->nid id))))))

(defspec tempids-recognized
  100000
  (prop/for-all [g (gen/choose 0 (dec (Math/pow 2 gt/GID-BITS)))]
                (let [tid (gt/tempid g)]
                  (and (gt/tempid? tid)
                       (= g (gt/nref->gid tid))))))

(defspec regular-ids-not-confused
  100000
  (prop/for-all [g (gen/choose 0 (dec (Math/pow 2 gt/GID-BITS)))
                 n (gen/choose 0 (dec (Math/pow 2 gt/NID-BITS)))]
                (let [id (gt/make-nref g n)]
                  (not (gt/tempid? id)))))
