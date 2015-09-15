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
  (is (worse-than (info    "") (warning "")))
  (is (worse-than (info    "") (severe  "")))
  (is (worse-than (info    "") (fatal   "")))
  (is (worse-than (warning "") (severe  "")))
  (is (worse-than (warning "") (fatal   "")))
  (is (worse-than (severe  "") (fatal   "")))

  (is (not (worse-than (warning "") (info    ""))))
  (is (not (worse-than (severe  "") (info    ""))))
  (is (not (worse-than (fatal   "") (info    ""))))
  (is (not (worse-than (severe  "") (warning ""))))
  (is (not (worse-than (fatal   "") (warning ""))))
  (is (not (worse-than (fatal   "") (severe  "")))))

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
