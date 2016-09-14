(ns internal.graph.error-values-test
  (:require [internal.graph.error-values :refer :all]
            [clojure.test :refer :all]))

(deftest error-construction
  (is (error-info?    (error-info    "this is a message")))
  (is (error-warning? (error-warning "this is a message")))
  (is (error-fatal?   (error-fatal   "this is a message"))))

(deftest severity-aggregating
  (is (error-info?    (error-aggregate [(error-info "info") (error-info "another") (error-info "third")])))
  (is (error-warning? (error-aggregate [(error-info "info") (error-info "another") (error-warning "third")])))
  (is (error-fatal?   (error-aggregate [(error-info "info") (error-info "another") (error-fatal "third")]))))

(deftest severity-ordering
  (is (worse-than :info    (error-warning "")))
  (is (worse-than :info    (error-fatal   "")))
  (is (worse-than :warning (error-fatal   "")))

  (is (not (worse-than :warning (error-info    ""))))
  (is (not (worse-than :fatal   (error-info    ""))))
  (is (not (worse-than :fatal   (error-warning "")))))

(deftest checking-errorness
  (is (error? (error-info    "")))
  (is (error? (error-warning "")))
  (is (error? (error-fatal   "")))
  (is (not (error? [])))
  (is (not (error? nil)))
  (is (not (error? 1)))
  (is (not (error? 'foo)))
  (is (not (error? :foo)))
  (is (error? [1 2 (error-fatal :boo) 4 5])))

