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

(deftest packaged-errors-test
  (testing "Packaged errors are not errors"
    (is (not (error? (package-errors 12345 (error-fatal ""))))))

  (testing "Packaged errors support severity testing"
    (is (error-info? (package-errors 12345)))
    (is (error-info? (package-errors 12345 (error-info ""))))
    (is (error-warning? (package-errors 12345 (error-info "") (error-warning ""))))
    (is (error-fatal? (package-errors 12345 (error-info "") (error-warning "") (error-fatal "")))))

  (testing "Unpacking an error"
    (let [node-id 12345
          error (error-fatal "wrapped")
          package (package-errors node-id error)
          unpacked (unpack-errors package)]
      (is (error? unpacked))
      (is (= node-id (:_node-id unpacked)))
      (is (= [error] (:causes unpacked)))))

  (testing "Packaging flattens errors"
    (is (= [(error-fatal "0")
            (error-fatal "1a")
            (error-fatal "1b")
            (error-fatal "2a")
            (error-fatal "2ba")
            (error-fatal "2bb")]
           (:causes (unpack-errors (package-errors 12345
                                                   (error-fatal "0")
                                                   [(error-fatal "1a") (error-fatal "1b")]
                                                   [(error-fatal "2a") [(error-fatal "2ba")
                                                                        (error-fatal "2bb")]]))))))

  (testing "Packaging returns nil if no errors"
    (is (nil? (package-errors 12345 nil)))
    (is (nil? (package-errors 12345
                              nil
                              [nil nil]
                              [nil [nil
                                    nil]]))))

  (testing "Packaging lifts packaged errors with matching node id"
    (is (= (map->ErrorValue {:_node-id 12345
                             :severity :fatal
                             :causes [(error-fatal "0")
                                      (error-fatal "1a")
                                      (error-fatal "1b")
                                      (error-fatal "2a")
                                      (error-fatal "2ba")
                                      (error-fatal "2bb")]})
           (unpack-errors (package-errors 12345
                                          (error-fatal "0")
                                          (package-errors 12345
                                                          [(error-fatal "1a") (error-fatal "1b")])
                                          (package-errors 12345
                                                          [(error-fatal "2a") (package-errors 12345 [(error-fatal "2ba")
                                                                                                     (error-fatal "2bb")])]))))))

  (testing "The flatten-errors function returns nil if no errors"
    (is (nil? (flatten-errors nil)))
    (is (nil? (flatten-errors nil
                              [nil nil]
                              [nil [nil
                                    nil]]))))

  (testing "The flatten-errors function filters out nil values"
    (is (= (map->ErrorValue {:severity :fatal
                             :causes [(error-fatal "wrapped")]})
           (flatten-errors nil [nil (error-fatal "wrapped")]))))

  (testing "The flatten-errors function unpacks packaged errors"
    (is (= (map->ErrorValue {:severity :fatal
                             :causes [(map->ErrorValue {:_node-id 12345
                                                        :severity :fatal
                                                        :causes [(error-fatal "wrapped")]})]})
           (flatten-errors (package-errors 12345 (error-fatal "wrapped")))))))
