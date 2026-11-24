;; Copyright 2020-2026 The Defold Foundation
;; Copyright 2014-2020 King
;; Copyright 2009-2014 Ragnar Svensson, Christian Murray
;; Licensed under the Defold License version 1.0 (the "License"); you may not use
;; this file except in compliance with the License.
;;
;; You may obtain a copy of the License, together with FAQs at
;; https://www.defold.com/license
;;
;; Unless required by applicable law or agreed to in writing, software distributed
;; under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
;; CONDITIONS OF ANY KIND, either express or implied. See the License for the
;; specific language governing permissions and limitations under the License.

(ns editor.editor-extensions.coerce-test
  (:require [clojure.test :refer :all]
            [clojure.test.check.clojure-test :refer [defspec]]
            [clojure.test.check.generators :as gen]
            [clojure.test.check.properties :as prop]
            [editor.editor-extensions.coerce :as coerce]
            [editor.editor-extensions.runtime :as rt]
            [editor.editor-extensions.vm :as vm])
  (:import [org.luaj.vm2 LuaError LuaValue Varargs]))

(set! *warn-on-reflection* true)

(def ^:private mismatch ::mismatch)

(defn- token-set-coercer [accepted]
  (fn coerce-token-set [_ ^Varargs args]
    (let [lua-value (.arg1 args)]
      (if (and (.isinttype lua-value)
               (contains? accepted (.tolong lua-value)))
        (.tolong lua-value)
        (coerce/failure lua-value "does not match token set")))))

(defn- syntax [ops]
  (reduce
    (fn [result {:keys [key quantifier pattern]}]
      (cond-> (conj result key)
        quantifier (conj quantifier)
        true (conj (if-not (vector? pattern)
                     (token-set-coercer pattern)
                     (syntax pattern)))))
    []
    ops))

(defn- compile-regex [ops]
  (apply coerce/regex (syntax ops)))

(defn- valid-group-bodies? [ops]
  (reduce (fn [valid {:keys [pattern]}]
            (if-not (and valid (vector? pattern))
              valid
              (and (pos? (count pattern))
                   (valid-group-bodies? pattern))))
          true
          ops))

(declare reference-match-prefix)

(defn- attempt-pattern [pattern inputs input-index]
  (if (vector? pattern)
    (reference-match-prefix pattern inputs input-index)
    (if-not (< input-index (count inputs))
      mismatch
      (let [input (inputs input-index)]
        (if (contains? pattern input)
          {:value input :next-input-index (inc input-index)}
          mismatch)))))

(defn- reference-match-prefix [ops inputs start-input-index]
  (loop [result {}
         input-index start-input-index
         op-index 0]
    (if (= op-index (count ops))
      {:value result :next-input-index input-index}
      (let [{:keys [key quantifier pattern]} (ops op-index)
            match (attempt-pattern pattern inputs input-index)]
        (if (identical? mismatch match)
          (if (= :? quantifier)
            (recur result input-index (inc op-index))
            mismatch)
          (let [next-input-index (:next-input-index match)]
            (recur (if (and (= :? quantifier)
                            (vector? pattern)
                            (= input-index next-input-index))
                     result
                     (assoc result key (:value match)))
                   next-input-index
                   (inc op-index))))))))

(defn- reference-match [ops inputs]
  (let [match (reference-match-prefix ops inputs 0)]
    (if (or (identical? mismatch match)
            (not= (count inputs) (:next-input-index match)))
      mismatch
      (:value match))))

(defn- actual-match [coercer inputs]
  (coercer nil (apply rt/->varargs inputs)))

(deftest nullable-group-test
  (let [value (token-set-coercer #{0})
        tail (token-set-coercer #{1})]
    (doseq [ops [[:group []]
                 [:group :? []]
                 [:outer [:inner []]]]]
      (is (thrown-with-msg? IllegalArgumentException
                            #"regex group must include at least one op"
                            (apply coerce/regex ops))))
    (is (= {:group {}}
           (actual-match (coerce/regex :group [:value :? value]) [])))
    (is (= {}
           (actual-match (coerce/regex :group :? [:value :? value]) [])))
    (is (= {:group {} :tail 1}
           (actual-match (coerce/regex :group [:value :? value]
                                       :tail tail)
                         [1])))
    (is (= {:tail 1}
           (actual-match (coerce/regex :group :? [:value :? value]
                                       :tail tail)
                         [1])))
    (is (= {:group {:value 0}}
           (actual-match (coerce/regex :group :? [:value :? value]) [0])))
    (is (= {:outer {:inner {}}}
           (actual-match (coerce/regex :outer [:inner [:value :? value]]) [])))
    (is (= {:outer {}}
           (actual-match (coerce/regex :outer [:inner :? [:value :? value]]) [])))
    (is (= {:outer {:inner {:value 0}}}
           (actual-match (coerce/regex :outer [:inner [:value value]]) [0])))))

(deftest malformed-regex-specification-test
  (let [value (token-set-coercer #{0})]
    (doseq [ops [[:value]
                 [:value :?]
                 [:value :1]
                 [:value nil]
                 [0 value]
                 [:value :? :1 value]
                 [:group [:value]]
                 [:group [:value :?]]]]
      (is (thrown? IllegalArgumentException
                   (apply coerce/regex ops))))))

(def ^:private regex-ops-gen
  (let [key-gen (gen/elements [:a :b :? :1 :*])
        quantifier-gen (gen/elements [nil :1 :?])
        token-set-gen (gen/elements [#{0} #{1} #{2}
                                     #{0 1} #{0 2} #{1 2}
                                     #{0 1 2}])
        op-gen (fn [pattern-gen]
                 (gen/let [key key-gen
                           quantifier quantifier-gen
                           pattern pattern-gen]
                   {:key key
                    :quantifier quantifier
                    :pattern pattern}))
        pattern-gen (gen/recursive-gen
                      (fn [inner-pattern-gen]
                        (gen/vector (op-gen inner-pattern-gen) 0 3))
                      token-set-gen)]
    (gen/vector (op-gen pattern-gen) 0 3)))

(defspec generated-regex-matches-reference 500
  (prop/for-all [ops (gen/resize 8 regex-ops-gen)
                 inputs (gen/vector (gen/choose -1 2) 0 8)]
    (if (valid-group-bodies? ops)
      (let [expected (reference-match ops inputs)
            actual (actual-match (compile-regex ops) inputs)]
        (if (identical? mismatch expected)
          (coerce/failure? actual)
          (= expected actual)))
      (try
        (compile-regex ops)
        false
        (catch IllegalArgumentException _
          true)))))

(defn- exact-token-op [key token]
  {:key key
   :quantifier nil
   :pattern #{token}})

(defn- ensure-non-empty-groups [ops]
  (mapv (fn [op]
          (let [pattern (:pattern op)]
            (if-not (vector? pattern)
              op
              (let [pattern (ensure-non-empty-groups pattern)
                    pattern (if (zero? (count pattern))
                              [(exact-token-op :group-value 0)]
                              pattern)]
                (assoc op :pattern pattern)))))
        ops))

(defn- assign-unique-tokens [ops next-token]
  (reduce (fn [[result next-token] op]
            (let [pattern (:pattern op)]
              (if-not (vector? pattern)
                [(conj result (assoc op :pattern #{next-token})) (inc next-token)]
                (let [[pattern next-token] (assign-unique-tokens pattern next-token)]
                  [(conj result (assoc op :pattern pattern)) next-token]))))
          [[] next-token]
          ops))

(defn- witness-case [ops presence-bits start-bit-index]
  (reduce (fn [[inputs expected bit-index] {:keys [key quantifier pattern]}]
            (let [optional (= :? quantifier)
                  next-bit-index (if optional (inc bit-index) bit-index)]
              (if-not (or (not optional)
                          (presence-bits (mod bit-index (count presence-bits))))
                [inputs expected next-bit-index]
                (if-not (vector? pattern)
                  [(conj inputs (first pattern))
                   (assoc expected key (first pattern))
                   next-bit-index]
                  (let [[nested-inputs nested-expected next-bit-index]
                        (witness-case pattern presence-bits next-bit-index)]
                    [(into inputs nested-inputs)
                     (if (and optional (zero? (count nested-inputs)))
                       expected
                       (assoc expected key nested-expected))
                     next-bit-index])))))
          [[] {} start-bit-index]
          ops))

(defspec generated-regex-accepts-witness 250
  (prop/for-all [ops (gen/resize 8 regex-ops-gen)
                 presence-bits (gen/vector gen/boolean 1 16)]
    (let [[ops] (assign-unique-tokens (ensure-non-empty-groups ops) 0)
          [inputs expected] (witness-case ops presence-bits 0)]
      (= expected (actual-match (compile-regex ops) inputs)))))

(defspec generated-regex-rejects-invalid-token 150
  (prop/for-all [ops (gen/resize 8 regex-ops-gen)
                 presence-bits (gen/vector gen/boolean 1 16)
                 insertion-index gen/nat]
    (let [[ops] (assign-unique-tokens (ensure-non-empty-groups ops) 0)
          [inputs] (witness-case ops presence-bits 0)
          insertion-index (mod insertion-index (inc (count inputs)))]
      (coerce/failure?
        (actual-match (compile-regex ops)
                      (into (conj (subvec inputs 0 insertion-index) -1)
                            (subvec inputs insertion-index)))))))

(defn- nested-prefix [tokens depth]
  (if-not (zero? depth)
    [{:key (keyword (str "nested" depth))
      :quantifier nil
      :pattern (nested-prefix tokens (dec depth))}]
    (reduce-kv (fn [ops index token]
                 (conj ops (exact-token-op (keyword (str "p" index)) token)))
               []
               tokens)))

(defn- fallback-ops [tokens]
  (reduce-kv (fn [ops index token]
               (conj ops (exact-token-op (keyword (str "f" index)) token)))
             []
             tokens))

(defspec generated-optional-group-rolls-back-atomically 100
  (prop/for-all [prefix-length (gen/choose 1 5)
                 depth (gen/choose 0 4)]
    (let [inputs (vec (range prefix-length))
          attempt (conj (nested-prefix inputs depth)
                        (exact-token-op :missing prefix-length))
          fallback (fallback-ops inputs)
          ops (into [{:key :attempt :quantifier :? :pattern attempt}]
                    fallback)
          expected (into {} (map (juxt :key #(first (:pattern %)))) fallback)]
      (= expected (actual-match (compile-regex ops) inputs)))))

(defspec generated-optional-group-success-commits-greedily 100
  (prop/for-all [prefix-length (gen/choose 1 5)
                 depth (gen/choose 0 4)]
    (let [inputs (vec (range prefix-length))
          ops (into [{:key :attempt
                      :quantifier :?
                      :pattern (nested-prefix inputs depth)}]
                    (fallback-ops inputs))]
      (coerce/failure? (actual-match (compile-regex ops) inputs)))))

(deftest capture-semantics-test
  (testing "duplicate keys"
    (is (= {:value 1}
           (actual-match (coerce/regex :value (token-set-coercer #{0})
                                       :value (token-set-coercer #{1}))
                         [0 1])))
    (is (= {:value 0}
           (actual-match (coerce/regex :value (token-set-coercer #{0})
                                       :value :? (token-set-coercer #{1}))
                         [0]))))
  (testing "position-sensitive keyword keys"
    (is (= {:? 0 :1 1 :* 2}
           (actual-match (coerce/regex :? (token-set-coercer #{0})
                                       :1 (token-set-coercer #{1})
                                       :* (token-set-coercer #{2}))
                         [0 1 2]))))
  (testing "nil and false values"
    (let [coercer (coerce/regex :nil (coerce/enum nil)
                                :false (coerce/enum false))]
      (is (coerce/failure? (actual-match coercer [])))
      (is (= {:nil nil :false false}
             (actual-match coercer [nil false])))))
  (testing "untouched identity"
    (let [lua-value (LuaValue/valueOf "value")
          coercer (coerce/regex :group [:value coerce/untouched])]
      (is (identical? lua-value
                      (get-in (coercer nil lua-value) [:group :value]))))))

(deftest regex-reuse-and-attempt-count-test
  (let [calls (atom [])
        counting-coercer (fn [id accepted]
                           (let [coercer (token-set-coercer accepted)]
                             (fn [vm args]
                               (swap! calls conj id)
                               (coercer vm args))))
        a (counting-coercer :a #{0})
        b (counting-coercer :b #{1})
        tail (counting-coercer :tail #{0})
        coercer (coerce/regex :group :? [:a a :b b]
                              :tail tail)]
    (is (= {:tail 0} (actual-match coercer [0])))
    (is (= [:a :tail] @calls))
    (reset! calls [])
    (is (= {:group {:a 0 :b 1} :tail 0}
           (actual-match coercer [0 1 0])))
    (is (= [:a :b :tail] @calls))
    (reset! calls [])
    (is (coerce/failure? (actual-match coercer [0 2])))
    (is (= [:a :b :tail] @calls))
    (reset! calls [])
    (is (= {:group {:head 0}}
           (actual-match (coerce/regex :group [:head (token-set-coercer #{0})
                                               :value :? a])
                         [0])))
    (is (= [] @calls))))

(deftest grouped-failure-message-test
  (let [coercer (coerce/regex :group :? [:a :? (token-set-coercer #{0})
                                         :b (token-set-coercer #{1})]
                              :tail (token-set-coercer #{2}))]
    (is (thrown-with-msg? LuaError
                          #"(?s)^Invalid argument:\n(?!.*Invalid argument:)"
                          (coerce/coerce (vm/make) coercer (rt/->varargs 3))))))

(deftest zero-width-group-preserves-outer-failure-message-test
  (let [coercer (coerce/regex :outer :? coerce/string
                              :group :? [:inside :? coerce/boolean]
                              :tail (coerce/enum nil))]
    (is (thrown-with-msg? LuaError
                          #"(?s)^Invalid argument:\n- 3 is not a string\n- 3 is not a boolean\n- 3 is not nil$"
                          (coerce/coerce (vm/make) coercer (rt/->varargs 3))))))
