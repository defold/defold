(ns internal.property-test
  (:require [clojure.test :refer :all]
            [dynamo.graph :as g]
            [dynamo.util :as util]
            [editor.types :as types]
            [internal.graph.types :as gt]
            [plumbing.fnk.pfnk :as pf]
            [support.test-support :as ts])
  (:import [clojure.lang Compiler$CompilerException]))

(defprotocol MyProtocol)

(deftest compile-errors
  (is (thrown-with-msg?
       clojure.lang.Compiler$CompilerException #"Unable to resolve symbol: non-existent-symbol in this context"
       (eval '(dynamo.graph/defnode BadPropertyNode
                (property BadProp schema.core/Num (default non-existent-symbol))))))
  (is (thrown-with-msg?
       java.lang.AssertionError #"looks like a protocol"
       (eval '(dynamo.graph/defnode BadPropertyNode
                (property BadProp internal.property-test/MyProtocol)))))
  (is (thrown-with-msg?
       clojure.lang.Compiler$CompilerException #"Unable to resolve symbol: non-existent-symbol in this context"
       (eval '(dynamo.graph/defnode BadPropertyNode
                (property BadProp schema.core/Num (validate bad-case non-existent-symbol))))))
  (is (thrown-with-msg?
       clojure.lang.Compiler$CompilerException #"property dynamic requires a symbol"
       (eval '(dynamo.graph/defnode BadPropertyNode
                (property BadProp schema.core/Num (dynamic (enabled non-existent-symbol))))))))

(g/defnode PropertyFilledNode
  (property no-default             g/Num)
  (property with-default-value     g/Num (default 42))
  (property with-default-nil       g/Num (default nil))
  (property with-multiple-defaults g/Num (default 1) (default 2))
  (property with-default-value-fn  g/Num (default (constantly 23)))
  (property with-keyword-type      g/Keyword (default :some-keyword))
  (property with-symbol-type       g/Symbol  (default 'some-symbol)))

(deftest test-property-type-default-value
  (let [n (g/construct PropertyFilledNode)]
    (are [expected property] (= expected (get n property))
      nil           :no-default
      42            :with-default-value
      nil           :with-default-nil
      2             :with-multiple-defaults
      23            :with-default-value-fn
      :some-keyword :with-keyword-type
      'some-symbol  :with-symbol-type)))

(g/defnode ValidatingPropertyNode
  (property without-validation            g/Any)
  (property with-validation-fn-inline     g/Num (validate pos #(pos? %)))
  (property with-validation-fn-value      g/Num (validate pos (every-pred pos? even?)))
  (property with-validation-literal-true  g/Num (validate any-value true))
  (property with-validation-literal-false g/Num (validate no-value false))
  (property with-multiple-validations     g/Num (validate same-label pos?) (validate same-label neg?)))

(deftest test-property-type-validation
  (ts/with-clean-system
    (let [[n] (ts/tx-nodes (g/make-node world ValidatingPropertyNode))]
      (are [expected property value]
          (do
            (g/set-property! n property value)
            (= expected (empty? (get-in (g/node-value n :_properties) [:properties property :validation-problems]))))
        true   :without-validation            nil
        true   :without-validation            0
        true   :without-validation            1
        false  :with-validation-fn-inline     0
        true   :with-validation-fn-inline     1
        false  :with-validation-fn-value      0
        false  :with-validation-fn-value      1
        true   :with-validation-fn-value      2
        true   :with-validation-literal-true  0
        true   :with-validation-literal-true  1
        false  :with-validation-literal-false 0
        false  :with-validation-literal-false 1
        false  :with-multiple-validations     +1
        false  :with-multiple-validations     -1))))

(defn- make-formatter [s]
  (fn [x] (str x s)))

(g/defnode ValidationFormatterNode
  (property without-formatter g/Any (validate must-be-even even?))
  (property with-formatter-inline g/Num (validate must-be-even :message #(str % " is not even") even?))
  (property with-formatter-fn-value g/Num (validate must-be-even :message (make-formatter " is not even") even?))
  (property with-literal          g/Num (validate must-be-even :message "value must be even" even?))
  (property with-multiple         g/Num (validate must-be-even :message "value must be even" even?)
            (validate must-be-positive :message "value must be positive" pos?)))

(deftest test-property-type-validation-messages
  (ts/with-clean-system
    (let [[n] (ts/tx-nodes (g/make-node world ValidationFormatterNode))]
      (are [expected property value]
          (do
            (g/set-property! n property value)
            (= expected (get-in (g/node-value n :_properties) [:properties property :validation-problems])))
        [] :without-formatter 4
        ["invalid value"] :without-formatter 5
        ["invalid value"] :without-formatter "not a number"
        ["5 is not even"] :with-formatter-inline 5
        ["5 is not even"] :with-formatter-fn-value 5
        ["value must be even"]  :with-literal 5
        ["value must be even"]  :with-multiple 5
        ["value must be positive"] :with-multiple -4
        ["value must be even" "value must be positive"] :with-multiple -5))))

(deftest property-display-order-merging
  (are [expected _ sources] (= expected (apply g/merge-display-order sources))
    [:id :path]                                                     -> [[:id :path]]
    [:id :path :rotation :position]                                 -> [[:id :path] [:rotation :position]]
    [:rotation :position :id :path]                                 -> [[:rotation :position] [:id :path]]
    [:rotation :position :scale :id :path]                          -> [[:rotation :position] [:scale] [:id :path]]
    [["Transform" :rotation :position :scale]]                      -> [[["Transform" :rotation :position]] [["Transform" :scale]]]
    [["Transform" :rotation :position :scale] :path]                -> [[["Transform" :rotation :position]] [["Transform" :scale] :path]]
    [:id ["Transform" :rotation :scale] :path ["Foo" :scale] :cake] -> [[:id ["Transform" :rotation]] [["Transform" :scale] :path] [["Foo" :scale] :cake]]
    [:id :path ["Transform" :rotation :position :scale]]            -> [[:id :path ["Transform"]] [["Transform" :rotation :position :scale]]]
    [["Material" :specular :ambient] :position :rotation]           -> [[["Material" :specular :ambient]] [:specular :ambient] [:position :rotation]]))
