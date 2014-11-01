(ns internal.type-test
  (:require [clojure.test :refer :all]
            [schema.core :as s]
            [schema.macros :as sm]
            [dynamo.types :as t]))

(sm/defrecord T1
  [ident :- String])

(sm/defrecord T2
  [value :- Integer])

(deftest type-compatibility
  (are [first second allow-collection? compatible?] (= compatible? (t/compatible? first second allow-collection?)))
  T1 T1               false    true
  T1 T1               true     false
  T1 T2               false    false
  T1 T2               true     false
  T1 [T1]             true     true
  T1 [T1]             false    false
  T1 [T2]             true     false
  T1 [T2]             false    false
  String String       false    true
  String String       true     false
  String [String]     false    false
  String [String]     true     true
  [String] String     false    false
  [String] String     true     false
  [String] [String]   false    true
  [String] [String]   true     false
  [String] [[String]] true     true
  Integer  Number     false    true
  Integer  s/Num      false    true
  T1       s/Any      false    true
  T1       s/Any      true     true
  T1       [s/Any]    false    false
  T1       [s/Any]    true     true
  String   s/Any      false    true
  String   s/Any      true     true
  String   [s/Any]    false    false
  String   [s/Any]    true     true
  [String] s/Any      false    true
  [String] s/Any      true     true
  [String] [s/Any]    false    true
  [String] [s/Any]    false    true
  )

(run-tests)