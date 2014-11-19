(ns internal.either-test
  (:require [clojure.test :refer :all]
            [internal.either :as e]))

(deftest test-either
  (let [e-foo (e/bind :foo)
        e-foo-else-bar    (e/or-else e-foo (fn [_] :bar))
        e-foo-else-assert (e/or-else e-foo (fn [_] (assert false "failed assertion!")))]
    (is (satisfies? e/Either e-foo))
    (is (satisfies? e/Either e-foo-else-bar))
    (is (satisfies? e/Either e-foo-else-assert))
    (is (= :foo (e/result e-foo)))
    (is (= :foo (e/result e-foo-else-bar)))
    (is (= :foo (e/result e-foo-else-assert))))
  (let [ex (ex-info "some exception" {:some :ex-data})
        e-throw (e/bind (throw ex))
        e-throw-else-bar    (e/or-else e-throw (fn [_] :bar))
        e-throw-else-assert (e/or-else e-throw (fn [_] (assert false "failed assertion!")))
        e-throw-else-return (e/or-else e-throw (fn [x] {:arg x}))]
    (is (satisfies? e/Either e-throw))
    (is (satisfies? e/Either e-throw-else-bar))
    (is (satisfies? e/Either e-throw-else-assert))
    (is (satisfies? e/Either e-throw-else-return))
    (try
      (e/result e-throw)
      (is false "expected e/result to throw an exception")
      (catch Throwable caught-ex
        (is (= ex caught-ex))))
    (is (= :bar (e/result e-throw-else-bar)))
    (is (thrown? java.lang.AssertionError (e/result e-throw-else-assert)))
    (is (= {:arg ex} (e/result e-throw-else-return))))
  (is (= :quux (-> (e/bind            (assert false "foo"))
                   (e/or-else (fn [_] (assert false "bar")))
                   (e/or-else (fn [_] (assert false "baz")))
                   (e/or-else (fn [_] :quux))
                   (e/result)))))
