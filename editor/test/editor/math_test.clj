(ns editor.math-test
 (:require [clojure.test :refer :all]
           [editor.math :as math])
 (:import [java.lang Math]
          [javax.vecmath Quat4d]))

(def epsilon 0.000001)

(defn- near? [v1 v2]
  (<= (Math/abs (- v1 v2)) epsilon))

(defn- eq? [q1 q2]
  (boolean (and (near? (.getX q1) (.getX q2)) (near? (.getY q1) (.getY q2)) (near? (.getZ q1) (.getZ q2)) (near? (.getW q1) (.getW q2)))))

(deftest quat->euler->quat []
  (let [test-vals [[1 0 0 0]
                   [0 1 0 0]
                   [0 0 -1 0]
                   [0 0 0 1]
                   [1 1 1 1]]
        quats (map #(Quat4d. (double-array %)) test-vals)]
    (doseq [result (map eq? quats (map (comp math/euler->quat math/quat->euler) quats))]
      (is result))))
