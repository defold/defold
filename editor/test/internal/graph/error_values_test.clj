(ns internal.graph.error-values-test
  (:require [internal.graph.error-values :refer :all]
            [clojure.test :refer :all]))

(deftest error-construction
  (is (info?    (info    "this is a message")))
  (is (warning? (warning "this is a message")))
  (is (severe?  (severe  "this is a message")))
  (is (fatal?   (fatal   "this is a message"))))

(deftest severity-aggregating
  (is (info?    (error-aggregate [(info "info") (info "another") (info "third")])))
  (is (warning? (error-aggregate [(info "info") (info "another") (warning "third")])))
  (is (severe?  (error-aggregate [(info "info") (info "another") (severe "third")])))
  (is (fatal?   (error-aggregate [(info "info") (info "another") (fatal "third")]))))

(deftest severity-ordering
  (is (worse-than INFO    (warning "")))
  (is (worse-than INFO    (severe  "")))
  (is (worse-than INFO    (fatal   "")))
  (is (worse-than WARNING (severe  "")))
  (is (worse-than WARNING (fatal   "")))
  (is (worse-than SEVERE  (fatal   "")))

  (is (not (worse-than WARNING (info    ""))))
  (is (not (worse-than SEVERE  (info    ""))))
  (is (not (worse-than FATAL   (info    ""))))
  (is (not (worse-than SEVERE  (warning ""))))
  (is (not (worse-than FATAL   (warning ""))))
  (is (not (worse-than FATAL   (severe  "")))))

(deftest checking-errorness
  (is (error? (info    "")))
  (is (error? (warning "")))
  (is (error? (severe  "")))
  (is (error? (fatal   "")))
  (is (not (error? [])))
  (is (not (error? nil)))
  (is (not (error? 1)))
  (is (not (error? 'foo)))
  (is (not (error? :foo)))
  (is (error? [1 2 (fatal :boo) 4 5])))

(deftest checking-seriousness
  (is (info?    (most-serious [(info "info") (info "another") (info "third")])))
  (is (warning? (most-serious [(info "info") (warning "another") (warning "third")])))
  (is (severe?  (most-serious [(info "info") (warning "another") (severe "third")])))
  (is (fatal?   (most-serious [(info "info") (severe "another") (fatal "third")]))))
