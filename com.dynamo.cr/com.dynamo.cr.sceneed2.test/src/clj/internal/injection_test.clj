(ns internal.injection-test
  (:require [clojure.test :refer :all]
            [schema.core :as s]
            [plumbing.core :refer [defnk]]
            [dynamo.node :as n]
            [internal.node :as in]))

(n/defnode Receiver
  (input surname String)
  (input samples [s/Num])
  (input label s/Any))

(defnk produce-string :- String
  []
  "nachname")

(n/defnode Sender1
  (output surname String produce-string))

(defnk produce-sample :- Integer
  []
  42)

(n/defnode Sampler
  (output sample Integer produce-sample))

(defnk produce-label :- s/Keyword
  []
  :a-keyword)

(n/defnode Labeler
  (output label s/Keyword produce-label))

(deftest compatible-inputs-and-outputs
  (let [recv    (make-receiver)
        sender  (make-sender-1)
        sampler (make-sampler)
        labeler (make-labeler)]
    (is (= [[:surname String :surname String]]  (in/injection-candidates recv sender)))
    (is (= [[:sample Integer :samples [s/Num]]] (in/injection-candidates recv sampler)))
    (is (= [[:label s/Keyword :label s/Any]]    (in/injection-candidates recv labeler)))))
