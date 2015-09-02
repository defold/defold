(ns editor.gl.volatile-cache-test
  (:require [clojure.test :refer :all]
            [clojure.core.cache :as cache]
            [editor.gl.volatile-cache :as volatile-cache]))

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
