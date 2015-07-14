(ns internal.type-test
  (:require [clojure.test :refer :all]
            [internal.graph.types :refer [error error?]]))

(deftest error-is-identifiable
  (is (error? (error {})))
  (is (not (error? 0)))
  (is (not (error? nil)))
  (is (not (error? false)))
  (is (not (error? (Exception.)))))
