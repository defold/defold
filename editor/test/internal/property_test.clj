(ns internal.property-test
  (:require [clojure.test :refer :all]
            [dynamo.graph :as g]
            [editor.types :as types]
            [internal.graph.types :as gt]
            [internal.util :as util]
            [plumbing.fnk.pfnk :as pf]
            [schema.core :as s]
            [support.test-support :refer [tx-nodes with-clean-system]]
            [internal.property :as ip])
  (:import clojure.lang.Compiler))

(defprotocol MyProtocol)

(def var-to-str s/Str)
(def var-to-str-vec [s/Str])

(deftest resolve-value-type-tests
  (are [x] (= java.lang.String (ip/resolve-value-type x))
    's/Str
    var-to-str
    #'var-to-str
    String)

  (are [x] (= [java.lang.String] (ip/resolve-value-type x))
    '[s/Str]
    #'var-to-str-vec
    var-to-str-vec)

  (are [x] (= MyProtocol (ip/resolve-value-type x))
    MyProtocol
    `MyProtocol))

(def default-value -5)

(def prop-without-default               (ip/property-type-descriptor 'g/Num '[]))
(def prop-with-default                  (ip/property-type-descriptor 'g/Num '[(default 42)]))
(def prop-with-explicit-nil-default     (ip/property-type-descriptor 'g/Any '[(default nil)]))
(def prop-with-multiple-default-clauses (ip/property-type-descriptor 'g/Num '[(default 1) (default 2)]))
(def prop-with-default-from-fn          (ip/property-type-descriptor 'g/Num '[(default (constantly 23))]))
(def prop-with-default-as-symbol        (ip/property-type-descriptor 'g/Num '[(default default-value)]))

(g/defnk custom-getter [in1 in2])

(def prop-with-getter           (ip/property-type-descriptor 'g/Any '[(value (g/fnk [in1 in2]))]))
(def prop-with-getter-as-symbol (ip/property-type-descriptor 'g/Any '[(value custom-getter)]))
(def prop-with-getter-as-var    (ip/property-type-descriptor 'g/Any '[(value #'custom-getter)]))

(g/defnk fooable-fn [an-input another-input])

(def prop-with-dynamic                   (ip/property-type-descriptor 'g/Any '[(dynamic fooable (g/fnk [an-input] (pos? an-input)))]))
(def prop-with-dynamic-as-symbol         (ip/property-type-descriptor 'g/Num '[(dynamic fooable fooable-fn)]))
(def prop-with-dynamic-as-var            (ip/property-type-descriptor 'g/Num '[(dynamic fooable #'fooable-fn)]))
(def prop-with-constant-valued-dynamic   (ip/property-type-descriptor 'g/Num '[(dynamic fooable 42)]))

(def prop-with-validator                 (ip/property-type-descriptor 'g/Any '[(validate (g/fnk [an-input] (pos? an-input)))]))
(def prop-with-validator-as-symbol       (ip/property-type-descriptor 'g/Num '[(validate fooable-fn)]))
(def prop-with-validator-as-var          (ip/property-type-descriptor 'g/Num '[(validate #'fooable-fn)]))
(def prop-with-constant-valued-validator (ip/property-type-descriptor 'g/Num '[(validate false)]))

(defmacro make-fnk [fld msg] `(g/fnk [~fld] (assert ~fld ~msg)))

(def prop-with-validator-macro           (ip/property-type-descriptor 'g/Num '[(validate (make-fnk an-input "Need a thing"))]))

(deftest test-property-type-definition
  (testing "basic property syntax"
    (let [property-defn (ip/property-type-descriptor 'g/Any '[])]
      (is (= g/Any (::ip/value-type property-defn)))))

  (testing "default values"
    (are [p x] (= (::ip/default p) x)
      prop-without-default               nil
      prop-with-default                  42
      prop-with-explicit-nil-default     nil
      prop-with-multiple-default-clauses 2
      prop-with-default-from-fn          '(constantly 23)
      prop-with-default-as-symbol        'default-value))

  (testing "a type is required"
    (is (thrown-with-msg? AssertionError #"doesn't seem like a real schema" (eval '(ip/property-type-descriptor 12 '[]))))
    (is (thrown-with-msg? AssertionError #"doesn't seem like a real schema" (eval '(ip/property-type-descriptor internal.property-test/default-value '[]))))
    (is (thrown-with-msg? AssertionError #"doesn't seem like a real schema" (eval '(ip/property-type-descriptor `nil '[])))))

  (testing "naked protocols are allowed"
    (is (= `(s/protocol internal.property-test/MyProtocol) (ip/value-type
                                                            (eval '(ip/property-type-descriptor 'internal.property-test/MyProtocol '[]))))))

  (testing "properties depend on their getters' inputs"
    (are [p x] (= (::ip/dependencies p) x)
      prop-with-getter           #{:in1 :in2}
      prop-with-getter-as-symbol #{:in1 :in2}
      prop-with-getter-as-var    #{:in1 :in2}))

  (testing "properties depend on their dynamic attributes' inputs"
    (are [p x] (= (::ip/dependencies p) x)
      prop-with-dynamic           #{:an-input}
      prop-with-dynamic-as-symbol #{:an-input :another-input}
      prop-with-dynamic-as-var    #{:an-input :another-input}))

  (testing "properties depend on their validation functions' inputs"
    (are [p x] (= (::ip/dependencies p) x)
      prop-with-validator           #{:an-input}
      prop-with-validator-as-symbol #{:an-input :another-input}
      prop-with-validator-as-var    #{:an-input :another-input}))

  (testing "dynamics allow constant values"
    (is (= `(plumbing.core/fnk [] 42) (ip/dynamic-evaluator prop-with-constant-valued-dynamic :fooable))))

  (testing "validators allow constant values"
    (is (= `(plumbing.core/fnk [] false) (ip/validation-evaluator prop-with-constant-valued-validator))))

  (testing "fnks can be created by macros"
    (are [p x] (= (::ip/dependencies p) x)
      prop-with-validator-macro #{:an-input})))
