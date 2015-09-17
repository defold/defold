(ns internal.graph.error-values-test
  (:require [internal.graph.error-values :refer :all]
            [clojure.test :refer :all]))

(deftest error-construction
  (is (error-info?    (error-info    "this is a message")))
  (is (error-warning? (error-warning "this is a message")))
  (is (error-severe?  (error-severe  "this is a message")))
  (is (error-fatal?   (error-fatal   "this is a message"))))

(deftest severity-aggregating
  (is (error-info?    (error-aggregate [(error-info "info") (error-info "another") (error-info "third")])))
  (is (error-warning? (error-aggregate [(error-info "info") (error-info "another") (error-warning "third")])))
  (is (error-severe?  (error-aggregate [(error-info "info") (error-info "another") (error-severe "third")])))
  (is (error-fatal?   (error-aggregate [(error-info "info") (error-info "another") (error-fatal "third")]))))

(deftest severity-ordering
  (is (worse-than INFO    (error-warning "")))
  (is (worse-than INFO    (error-severe  "")))
  (is (worse-than INFO    (error-fatal   "")))
  (is (worse-than WARNING (error-severe  "")))
  (is (worse-than WARNING (error-fatal   "")))
  (is (worse-than SEVERE  (error-fatal   "")))

  (is (not (worse-than WARNING (error-info    ""))))
  (is (not (worse-than SEVERE  (error-info    ""))))
  (is (not (worse-than FATAL   (error-info    ""))))
  (is (not (worse-than SEVERE  (error-warning ""))))
  (is (not (worse-than FATAL   (error-warning ""))))
  (is (not (worse-than FATAL   (error-severe  "")))))

(deftest checking-errorness
  (is (error? (error-info    "")))
  (is (error? (error-warning "")))
  (is (error? (error-severe  "")))
  (is (error? (error-fatal   "")))
  (is (not (error? [])))
  (is (not (error? nil)))
  (is (not (error? 1)))
  (is (not (error? 'foo)))
  (is (not (error? :foo)))
  (is (error? [1 2 (error-fatal :boo) 4 5])))

(deftest checking-seriousness
  (is (error-info?    (most-serious [(error-info "info") (error-info "another")    (error-info "third")])))
  (is (error-warning? (most-serious [(error-info "info") (error-warning "another") (error-warning "third")])))
  (is (error-severe?  (most-serious [(error-info "info") (error-warning "another") (error-severe "third")])))
  (is (error-fatal?   (most-serious [(error-info "info") (error-severe "another")  (error-fatal "third")]))))
