(ns dynamo.integration.validation
  (:require [clojure.test :refer :all]
            [dynamo.graph :as g]
            [support.test-support :refer [with-clean-system tx-nodes]]))

; TODO - hack since connections are not overwritten
(defn- disconnect-all [node-id label]
  (for [[src-node-id src-label] (g/sources-of node-id label)]
    (g/disconnect src-node-id src-label node-id label)))

(g/defnode ProducerNode
  (property data g/Str))

(g/defnode ConsumerNode
  (property producer-store {g/Str g/NodeID})
  (input data-input g/Str)
  (property data g/Str
            (value (g/fnk [data-input]
                          (if data-input
                            data-input
                            (g/error-severe :no-connection))))
            (set (fn [basis self old-value new-value]
                   (let [producer-store (g/node-value self :producer-store :basis basis)]
                     (concat
                      (disconnect-all self :data-input)
                      (if-let [producer (get producer-store new-value)]
                        (g/connect producer :data self :data-input)
                        [])))))))

(defn- prop [node label]
  (get-in (g/node-value node :_properties) [:properties label :value]))

(defn- prop! [node label val]
  (g/transact
    (g/set-property node label val)))

; Test roughly corresponding to how resource nodes are connected
(deftest test-connection-validation
  (with-clean-system
    (let [[a b c] (tx-nodes (g/make-nodes world [a [ProducerNode :data "producer-a-data"]
                                                 b [ProducerNode :data "producer-b-data"]
                                                 c [ConsumerNode :producer-store {"producer-a" a "producer-b" b} :data "producer-a"]]))]
      (is (= "producer-a-data" (prop c :data)))
      (prop! c :data "producer-b")
      (is (= "producer-b-data" (prop c :data)))
      (prop! c :data "")
      ; TODO - should work, but fails now
      ; TODO - this is actually returning an error for :_properties. That's too aggressive!
      (is (g/error? (prop c :data))))))


(g/defnode ComplexProperty
  (input data g/Str)
  (property own-data g/Str
            (validate (g/fnk [data own-data next-data]
                             (when-not (= data own-data next-data)
                               (g/error-severe :not-equal)))))
  (property next-data g/Str
            (validate (g/fnk [data own-data next-data]
                              (when-not (= data own-data next-data)
                                (g/error-severe :not-equal))))))

(deftest test-complex-validation
  (with-clean-system
    (let [[a b c] (tx-nodes (g/make-nodes world [a [ProducerNode :data "data"]
                                                 b [ComplexProperty :own-data "data" :next-data "data"]]
                                          (g/connect a :data b :data)))]
      (is (not (g/error? (prop b :own-data))))
      (is (not (g/error? (prop b :next-data))))
      (prop! b :own-data "new-data")
      (is (g/error? (prop b :own-data)))
      (is (g/error? (prop b :next-data)))
      (prop! b :own-data "data")
      (prop! a :data "new-data")
      (is (g/error? (prop b :own-data)))
      (is (g/error? (prop b :next-data)))
      (prop! a :data "data")
      (is (not (g/error? (prop b :own-data))))
      (is (not (g/error? (prop b :next-data)))))))

(deftest test-cycles
  (with-clean-system
    (let [[a b c] (tx-nodes (g/make-nodes world [a [ProducerNode :data "data"]
                                                 b [ComplexProperty :own-data "data" :next-data "data"]]
                                          (g/connect a :data b :data)))]
      (is (not (g/error? (prop b :own-data)))))))

; Simulating a content pipeline build
(g/defnode ProjectNode
  (property resource-nodes #{g/NodeID})
  (input build-data g/Any :array))

(g/defnode ComponentNode
  (property my-data g/Any
            (validate (g/fnk [] (g/error-severe :failure)))))

(g/defnode ResourceNode
  (input sub-data g/Any :array)
  (output build-data g/Any (g/fnk [sub-data] sub-data)))

(deftest test-multi-errors
  (with-clean-system
    (let [all-nodes (tx-nodes (g/make-nodes world [; Two components
                                                   b ResourceNode
                                                   b-a [ComponentNode :my-data :b-a]
                                                   b-b [ComponentNode :my-data :b-b]
                                        ; Three components
                                                   c ResourceNode
                                                   c-a [ComponentNode :my-data :c-a]
                                                   c-b [ComponentNode :my-data :c-b]
                                                   c-c [ComponentNode :my-data :c-c]
                                        ; No components => no error
                                                   d ResourceNode
                                        ; Project
                                                   a [ProjectNode :resource-nodes #{b c d}]]
                                            (for [[res comp] [[b b-a]
                                                              [b b-b]
                                                              [c c-a]
                                                              [c c-b]
                                                              [c c-c]]]
                                              (g/connect comp :my-data res :sub-data))
                                            (for [res [b c d]]
                                              (g/connect res :build-data a :build-data))))
          project   (last all-nodes)
          b-a       (second all-nodes)]
      (let [nodes (g/node-value project :resource-nodes)
            build (g/node-value project :build-data)]
        (is (g/error? build))
        ; TODO - add code for verifying the nested structure of the error with:
        ; - value
        ; - node id
        ; - "message"
        ; possibly even transforming the structure into the "resource-list"
        ))))


(g/defnk validate-tile-width [tile-width tile-margin]
  (if-let [max-width 200]
    (let [total-width (+ tile-width tile-margin)]
      (when (> total-width max-width)
        (g/error-severe (format "The total tile width (including margin) %d is greater than the image width %d" total-width max-width))))))

(g/defnk validate-tile-height [tile-height tile-margin]
  (if-let [max-height 200]
    (let [total-height (+ tile-height tile-margin)]
      (when (> total-height max-height)
        (g/error-severe (format "The total tile height (including margin) %d is greater than the image height %d" total-height max-height))))))

(g/defnk validate-tile-dimensions [tile-width tile-height tile-margin :as all]
  (or (validate-tile-width all)
      (validate-tile-height all)))

(g/defnode TileSourceNode
  (property tile-width g/Int
            (default 0)
            (validate validate-tile-width))

  (property tile-height g/Int
            (default 0)
            (validate validate-tile-height))

  (property tile-margin g/Int
            (default 0)
            (validate validate-tile-dimensions)))

(defn- prop [node label]
  (get-in (g/node-value node :_properties) [:properties label :value]))

(defn- prop! [node label val]
  (g/transact
    (g/set-property node label val)))

(deftest test-cyclic-validation
  (with-clean-system
    (let [[ts] (tx-nodes (g/make-nodes world [ts [TileSourceNode :tile-width 1000 :tile-height 164 :tile-margin 100]]))]
      (is (g/error? (g/node-value ts :tile-width)))
      (is (g/error? (g/node-value ts :tile-height)))
      (is (g/error? (g/node-value ts :tile-margin))))))


(g/defnode CustomSetterNode
  (property plain-prop g/Int
    (validate (g/fnk [plain-prop] (when (< plain-prop 0) (g/error-severe "plain-prop must be positive")))))
  
  (property the-prop g/Int
    (value (g/fnk [] -4711))
    (set (fn [_ _ _ _]))
    (validate (g/fnk [the-prop]
                (when (< the-prop 0)
                  (g/error-severe "the-prop must be positive")))))
  
  (output output-probe g/Int (g/fnk [the-prop] "this should not be returned as the-prop is an error"))
  
  (property property-probe g/Int (value (g/fnk [the-prop] "this should not be returned as the-prop is an error")))

  (property dynamic-probe g/Int
    (default 0)
    (dynamic probe (g/fnk [the-prop] "this should not be returned as the-prop is an error"))))

(deftest custom-property-validation
  (with-clean-system
    (let [[csn] (tx-nodes (g/make-nodes world [csn [CustomSetterNode :the-prop -4711 :plain-prop -4711]]))]
      (testing "property is validated during node-value"
        (is (g/error? (g/node-value csn :plain-prop)))
        (is (g/error? (g/node-value csn :the-prop)))
        (is (g/error? (g/node-value csn :output-probe)))
        (is (g/error? (g/node-value csn :property-probe))))
      (let [props (g/node-value csn :_properties)]
        (testing "property is validated during node-value :_properties"
          (is (g/error? (get-in props [:properties :plain-prop :value])))
          (is (g/error? (get-in props [:properties :the-prop :value])))
          (is (g/error? (get-in props [:properties :property-probe :value])))
          (is (g/error? (get-in props [:properties :dynamic-probe :probe]))))))))

(g/defnode OverloadedNode
  (property another-kw g/Keyword
    (value (g/fnk [] :another)))

  (property prop-or-out-kw g/Keyword
    (value (g/fnk [] :prop)))
  (output prop-or-out-kw g/Keyword (g/fnk [] :out))

  (property prop-or-out-in-validation-kw g/Keyword
    (value (g/fnk [] :prop))
    (validate (g/fnk [prop-or-out-in-validation-kw]
                (g/error-severe {:value prop-or-out-in-validation-kw}))))
  (output prop-or-out-in-validation-kw g/Keyword (g/fnk [] :out))

  (property validate-prop-or-out-kw g/Keyword
    (default :validate-prop)
    (validate (g/fnk [prop-or-out-kw] (g/error-severe {:value prop-or-out-kw}))))

  (property dynamic-prop-or-out-kw g/Keyword
    (default :dummy)
    (dynamic dyno (g/fnk [prop-or-out-kw] prop-or-out-kw)))

  (property prop-poll-in-or-out-kw g/Keyword
    (value (g/fnk [in-or-out-kw] in-or-out-kw)))

  (input in-or-out-kw g/Keyword)
  (output in-or-out-kw g/Keyword (g/fnk [] :out))

  (output poll-prop-or-out-kw g/Keyword (g/fnk [prop-or-out-kw] prop-or-out-kw))
  (output poll-in-or-out-kw g/Keyword (g/fnk [in-or-out-kw] in-or-out-kw))
  (output poll-validate-prop-or-out-kw g/Keyword (g/fnk [validate-prop-or-out-kw] validate-prop-or-out-kw)))

(deftest overloaded-names-in-production-functions
  (with-clean-system
    (let [[p c] (tx-nodes (g/make-nodes world [p [OverloadedNode]
                                               c [OverloadedNode]]))]
      (g/transact (g/connect p :another-kw c :in-or-out-kw))
      (is (= :out (g/node-value c :prop-or-out-kw)))
      (is (= :out (g/node-value c :poll-prop-or-out-kw)))
      (is (= :out (g/node-value c :in-or-out-kw)))
      (is (= :out (g/node-value c :poll-in-or-out-kw)))
      (is (= :out (g/node-value c :prop-poll-in-or-out-kw)))
      (is (= :out (g/node-value c :prop-or-out-in-validation-kw)))
      (is (= :out (-> (g/node-value c :validate-prop-or-out-kw)
                    :causes
                    first
                    :user-data
                    :value)))
      (is (= :out (-> (g/node-value c :poll-validate-prop-or-out-kw)
                    :causes
                    first
                    :causes
                    first
                    :user-data
                    :value)))
      (let [props (g/node-value c :_properties)]
        (let [err (get-in props [:properties :prop-or-out-in-validation-kw :value])]
          (is (g/error? err))
          (is (= :out (get-in err [:user-data :value]))))
        (is (= :out (get-in props [:properties :dynamic-prop-or-out-kw :dyno])))))))
    
