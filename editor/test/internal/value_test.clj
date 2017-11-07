(ns internal.value-test
  (:require [clojure.test :refer :all]
            [dynamo.graph :as g]
            [internal.util :as util]
            [internal.node :as in]
            [internal.transaction :as it]
            [support.test-support :refer :all]
            [internal.graph.error-values :as ie])
  (:import [internal.graph.error_values ErrorValue]))

(def ^:dynamic *calls*)

(defn tally [node fn-symbol]
  (swap! *calls* update-in [node fn-symbol] (fnil inc 0)))

(defn get-tally [node fn-symbol]
  (get-in @*calls* [node fn-symbol] 0))

(defmacro expect-call-when [node fn-symbol & body]
  `(let [calls-before# (get-tally ~node ~fn-symbol)]
     ~@body
     (is (= (inc calls-before#) (get-tally ~node ~fn-symbol)))))

(defmacro expect-no-call-when [node fn-symbol & body]
  `(let [calls-before# (get-tally ~node ~fn-symbol)]
     ~@body
     (is (= calls-before# (get-tally ~node ~fn-symbol)))))

(defn- cached? [cache node-id label]
  (contains? cache [node-id label]))

(g/defnode CacheTestNode
  (input first-name g/Str)
  (input last-name  g/Str)
  (input operand    g/Str)

  (property scalar g/Str)

  (output uncached-value  g/Str
          (g/fnk [_node-id scalar]
                 (tally _node-id 'produce-simple-value)
                 scalar))

  (output expensive-value g/Str :cached
          (g/fnk [_node-id]
                 (tally _node-id 'compute-expensive-value)
                 "this took a long time to produce"))

  (output nickname g/Str :cached
          (g/fnk [_node-id first-name]
                 (tally _node-id 'passthrough-first-name)
                 first-name))

  (output derived-value g/Str :cached
          (g/fnk [_node-id first-name last-name]
                 (tally _node-id 'compute-derived-value)
                 (str first-name " " last-name)))

  (output another-value g/Str :cached
          (g/fnk [_node-id]
                 "this is distinct from the other outputs"))

  (output nil-value g/Str :cached
          (g/fnk [this]
                 (tally this 'compute-nil-value)
                 nil)))

(defn build-sample-project
  [world]
  (g/tx-nodes-added
   (g/transact
    (g/make-nodes world
                  [name1     [CacheTestNode :scalar "Jane"]
                   name2     [CacheTestNode :scalar "Doe"]
                   combiner  CacheTestNode
                   expensive CacheTestNode
                   nil-val   CacheTestNode]
                  (g/connect name1 :uncached-value combiner :first-name)
                  (g/connect name2 :uncached-value combiner :last-name)
                  (g/connect name1 :uncached-value expensive :operand)))))

(defn with-function-counts
  [f]
  (binding [*calls* (atom {})]
    (f)))

(use-fixtures :each with-function-counts)

(deftest project-cache
  (with-clean-system
    (let [[name1 name2 combiner expensive] (build-sample-project world)]
      (testing "uncached values are unaffected"
        (is (= "Jane" (g/node-value name1 :uncached-value)))))))

(deftest caching-avoids-computation
  (testing "cached values are only computed once"
    (with-clean-system
      (let [[name1 name2 combiner expensive] (build-sample-project world)]
        (is (= "Jane Doe" (g/node-value combiner :derived-value)))
        (expect-no-call-when combiner 'compute-derived-value
                             (doseq [x (range 100)]
                               (g/node-value combiner :derived-value))))))

  (testing "cached nil values are only computed once"
    (with-clean-system
      (let [[name1 name2 combiner expensive nil-value] (build-sample-project world)]
        (is (nil? (g/node-value nil-value :nil-value)))
        (expect-no-call-when nil-value 'compute-nil-value
                             (doseq [x (range 100)]
                               (g/node-value nil-value :nil-value)))
        (let [cache (g/cache)]
          (is (cached? cache nil-value :nil-value))))))

  (testing "modifying inputs invalidates the cached value"
    (with-clean-system
      (let [[name1 name2 combiner expensive] (build-sample-project world)]
        (is (= "Jane Doe" (g/node-value combiner :derived-value)))
        (expect-call-when combiner 'compute-derived-value
                          (g/transact (it/update-property name1 :scalar (constantly "John") []))
                          (is (= "John Doe" (g/node-value combiner :derived-value)))))))

  (testing "transmogrifying a node invalidates its cached value"
    (with-clean-system
      (let [[name1 name2 combiner expensive] (build-sample-project world)]
        (is (= "Jane Doe" (g/node-value combiner :derived-value)))
        (expect-call-when combiner 'compute-derived-value
                          (g/transact (it/become name1 (g/construct CacheTestNode)))
                          (is (= "Jane Doe" (g/node-value combiner :derived-value)))))))

  (testing "cached values are distinct"
    (with-clean-system
      (let [[name1 name2 combiner expensive] (build-sample-project world)]
        (is (= "this is distinct from the other outputs" (g/node-value combiner :another-value)))
        (is (not= (g/node-value combiner :another-value) (g/node-value combiner :expensive-value))))))

  (testing "cache invalidation only hits dependent outputs"
    (with-clean-system
      (let [[name1 name2 combiner expensive] (build-sample-project world)]
        (is (= "Jane" (g/node-value combiner :nickname)))
        (expect-call-when combiner 'passthrough-first-name
                          (g/transact (it/update-property name1 :scalar (constantly "Mark") []))
                          (is (= "Mark" (g/node-value combiner :nickname))))
        (expect-no-call-when combiner 'passthrough-first-name
                             (g/transact (it/update-property name2 :scalar (constantly "Brandenburg") []))
                             (is (= "Mark" (g/node-value combiner :nickname)))
                             (is (= "Mark Brandenburg" (g/node-value combiner :derived-value))))))))

(g/defnode OverrideValueNode
  (property int-prop g/Int)
  (property name g/Str)
  (input overridden g/Str)
  (input an-input g/Str)
  (output output g/Str (g/fnk [overridden] overridden))
  (output foo    g/Str (g/fnk [an-input] an-input)))

(defn build-override-project
  [world]
  (let [nodes (tx-nodes
               (g/make-node world OverrideValueNode)
               (g/make-node world CacheTestNode :scalar "Jane"))
        [override jane]  nodes]
    (g/transact
     (g/connect jane :uncached-value override :overridden))
    nodes))

(deftest invalid-resource-values
  (with-clean-system
    (let [[override jane] (build-override-project world)]
      (testing "requesting a non-existent label throws"
        (is (thrown? AssertionError (g/node-value override :aint-no-thang)))))))

(deftest update-sees-in-transaction-value
  (with-clean-system
    (let [[node]            (tx-nodes (g/make-node world OverrideValueNode :name "a project" :int-prop 0))
          after-transaction (g/transact
                             (concat
                              (g/update-property node :int-prop inc)
                              (g/update-property node :int-prop inc)
                              (g/update-property node :int-prop inc)
                              (g/update-property node :int-prop inc)))]
      (is (= 4 (g/node-value node :int-prop))))))

(g/defnode OutputChaining
  (property a-property g/Int (default 0))

  (output chained-output g/Int :cached
          (g/fnk [a-property]
                 (inc a-property))))

(deftest output-caching-does-not-accidentally-cache-inputs
  (with-clean-system
    (let [[node-id]       (tx-nodes (g/make-node world OutputChaining))]
      (g/node-value node-id :chained-output)
      (let [cache (g/cache)]
        (is (cached? cache node-id :chained-output))
        (is (not (cached? cache node-id :a-property)))))))

(g/defnode Source
  (property constant g/Keyword))

(g/defnode ValuePrecedence
  (property overloaded-output-input-property g/Keyword (default :property))
  (output   overloaded-output-input-property g/Keyword (g/fnk [] :output))

  (input    overloaded-input-property g/Keyword)
  (output   overloaded-input-property g/Keyword (g/fnk [overloaded-input-property] overloaded-input-property))

  (property the-property g/Keyword (default :property))

  (output   output-using-overloaded-output-input-property g/Keyword (g/fnk [overloaded-output-input-property] overloaded-output-input-property))

  (input    eponymous g/Keyword)
  (output   eponymous g/Keyword (g/fnk [eponymous] eponymous))


  (property position g/Keyword (default :position-property))
  (output   position g/Str     (g/fnk [position] (name position)))
  (output   transform g/Str    (g/fnk [position] position))

  (input    renderables           g/Keyword :array)
  (output   renderables           g/Str     (g/fnk [renderables] (apply str (mapcat name renderables))))
  (output   transform-renderables g/Str     (g/fnk [renderables] renderables)))


(deftest node-value-precedence
  (with-clean-system
    (let [[node s1] (tx-nodes (g/make-node world ValuePrecedence)
                              (g/make-node world Source :constant :input))]
      (g/transact
       (concat
        (g/connect s1 :constant node :overloaded-input-property)
        (g/connect s1 :constant node :eponymous)))
      (is (= :output             (g/node-value node :overloaded-output-input-property)))
      (is (= :input              (g/node-value node :overloaded-input-property)))
      (is (= :property           (g/node-value node :the-property)))
      (is (= :output             (g/node-value node :output-using-overloaded-output-input-property)))
      (is (= :input              (g/node-value node :eponymous)))
      (is (= "position-property" (g/node-value node :transform)))))

  (testing "output uses another output, which is a function of an input with the same name"
    (with-clean-system
      (let [[combiner s1 s2 s3] (tx-nodes (g/make-node world ValuePrecedence)
                                          (g/make-node world Source :constant :source-1)
                                          (g/make-node world Source :constant :source-2)
                                          (g/make-node world Source :constant :source-3))]
       (g/transact
        (concat
         (g/connect s1 :constant combiner :renderables)
         (g/connect s2 :constant combiner :renderables)
         (g/connect s3 :constant combiner :renderables)))
       (is (= "source-1source-2source-3" (g/node-value combiner :transform-renderables)))))))

(deftest invalidation-across-graphs
  (with-clean-system
    (let [project-graph (g/make-graph! :history true)
          view-graph    (g/make-graph! :volatility 100)
          [content-node aux-node] (tx-nodes (g/make-node project-graph CacheTestNode :scalar "Snake")
                                            (g/make-node project-graph CacheTestNode :scalar "Plissken"))
          [view-node]    (tx-nodes (g/make-node view-graph CacheTestNode))]
      (g/transact
       [(g/connect content-node :scalar view-node :first-name)
        (g/connect aux-node     :scalar view-node :last-name)])

      (expect-call-when
       view-node 'compute-derived-value
       (is (= "Snake Plissken" (g/node-value view-node :derived-value))))

      (g/transact (g/set-property aux-node :scalar "Solid"))

      (expect-call-when
       view-node 'compute-derived-value
       (is (= "Snake Solid" (g/node-value view-node :derived-value))))

      (g/transact
       [(g/disconnect     aux-node :scalar view-node :last-name)
        (g/disconnect content-node :scalar view-node :first-name)
        (g/connect        aux-node :scalar view-node :first-name)
        (g/connect    content-node :scalar view-node :last-name)])

      (expect-call-when
       view-node 'compute-derived-value
       (is (= "Solid Snake" (g/node-value view-node :derived-value))))

      (expect-no-call-when
       view-node 'compute-derived-value
       (is (= "Solid Snake" (g/node-value view-node :derived-value)))))))

(g/defnode SubstitutingInputsNode
  (input unary-no-sub     g/Any)
  (input multi-no-sub     g/Any :array)
  (input unary-with-sub   g/Any :substitute 99)
  (input multi-with-sub   g/Any :array :substitute (fn [err]
                                                     (util/map-vals #(if (g/error? %) 4848 %) err)))

  (output unary-no-sub    g/Any  (g/fnk [unary-no-sub] unary-no-sub))
  (output multi-no-sub    g/Any  (g/fnk [multi-no-sub] multi-no-sub))
  (output unary-with-sub  g/Any  (g/fnk [unary-with-sub] unary-with-sub))
  (output multi-with-sub  g/Any  (g/fnk [multi-with-sub] multi-with-sub)))

(g/defnode ConstantNode
  (output nil-output        g/Any  (g/fnk [] nil))
  (output scalar-with-error g/Any  (g/fnk [] (g/error-fatal :scalar)))
  (output scalar            g/Any  (g/fnk [] 1))
  (output everything        g/Any  (g/fnk [] 42)))

(defn arrange-sv-error
  [label connected? source-label]
  (with-clean-system
    (let [[receiver const] (tx-nodes (g/make-node world SubstitutingInputsNode)
                                     (g/make-node world ConstantNode))]
     (when connected?
       (g/transact (g/connect const source-label receiver label)))
     (def sv-val (g/node-value receiver label))
     (g/node-value receiver label))))

(deftest error-value-replacement
  (testing "source doesn't send errors"
    (with-clean-system
      (are [label connected? source-label expected-pfn-val]
          (= expected-pfn-val (arrange-sv-error label connected? source-label))
        ;; output-label connected? source-label  expected-pfn
        :unary-no-sub   false      :dontcare         nil
        :multi-no-sub   false      :dontcare         '()
        :unary-with-sub false      :dontcare         nil
        :multi-with-sub false      :dontcare         '()

        :unary-no-sub   true       :nil-output       nil
        :unary-with-sub true       :nil-output       nil

        :unary-no-sub   true       :scalar          1
        :unary-with-sub true       :everything      42

        :multi-no-sub   true       :scalar          '(1)
        :multi-with-sub true       :everything      '(42))))

  (binding [in/*suppress-schema-warnings* true]
    (testing "source sends errors"
      (testing "unary inputs"
        (with-clean-system
         (let [[receiver const] (tx-nodes
                                 (g/make-nodes world
                                               [receiver SubstitutingInputsNode
                                                const    ConstantNode]
                                               (g/connect const :scalar-with-error receiver :unary-no-sub)
                                               (g/connect const :scalar-with-error receiver :unary-with-sub)))]
           (is (g/error? (g/node-value receiver :unary-no-sub)))
           (is (= 99     (g/node-value receiver :unary-with-sub))))))
      (testing "multivalued inputs"
        (with-clean-system
          (let [[receiver const] (tx-nodes
                                 (g/make-nodes world
                                               [receiver SubstitutingInputsNode
                                                const    ConstantNode]
                                               (g/connect const :scalar            receiver :multi-no-sub)
                                               (g/connect const :scalar-with-error receiver :multi-no-sub)
                                               (g/connect const :scalar            receiver :multi-no-sub)
                                               (g/connect const :everything        receiver :multi-no-sub)

                                               (g/connect const :scalar            receiver :multi-with-sub)
                                               (g/connect const :scalar-with-error receiver :multi-with-sub)
                                               (g/connect const :scalar            receiver :multi-with-sub)
                                               (g/connect const :everything        receiver :multi-with-sub)))]
           (is (g/error?           (g/node-value receiver :multi-no-sub)))
           (is (= [1 4848 1 42]    (g/node-value receiver :multi-with-sub)))))))))


(g/defnode StringInputIntOutputNode
  (input string-input g/Str)
  (output int-output g/Str (g/fnk [] 1))
  (output combined g/Str (g/fnk [string-input] string-input)))

(deftest input-schema-validation-warnings
  (binding [in/*suppress-schema-warnings* true]
    (testing "schema validations on inputs"
     (with-clean-system
       (let [[node1] (tx-nodes (g/make-node world StringInputIntOutputNode))]
         (g/transact (g/connect node1 :int-output node1 :string-input))
         (is (thrown-with-msg? Exception #"SCHEMA-VALIDATION" (g/node-value node1 :combined))))))))

(g/defnode ConstantPropertyNode
  (property a-property g/Any))

(defn- cause [ev]
  (when (instance? ErrorValue ev)
    (first (:causes ev))))

(deftest error-values-are-not-wrapped-from-properties
  (with-clean-system
    (let [[node]      (tx-nodes (g/make-node world ConstantPropertyNode))
          _           (g/mark-defective! node (g/error-fatal "bad"))
          error-value (g/node-value node :a-property)]
      (is (g/error?      error-value))
      (is (empty?        (:causes   error-value)))
      (is (= node        (:_node-id error-value)))
      (is (= :a-property (:_label   error-value)))
      (is (= :fatal     (:severity error-value))))))

(g/defnode ErrorReceiverNode
  (input single g/Any)
  (input multi  g/Any :array)

  (output single-output g/Any (g/fnk [single] single))
  (output multi-output  g/Any (g/fnk [multi]  multi)))

(deftest error-values-are-aggregated
  (testing "single-valued input with an error results in single error out."
    (with-clean-system
      (let [[sender receiver] (tx-nodes
                               (g/make-nodes world
                                             [sender   ConstantPropertyNode
                                              receiver ErrorReceiverNode]
                                             (g/connect sender :a-property receiver :single)))
            _                 (g/mark-defective! sender (g/error-fatal "Bad news, my friend."))
            error-value       (g/node-value receiver :single-output)]
        (are [node label sev e] (and (= node (:_node-id e)) (= label (:_label e)) (= sev (:severity error-value)))
          receiver :single-output :fatal   error-value
          receiver :single        :fatal   (cause error-value)
          sender   :a-property    :fatal   (cause (cause error-value))))))

  (testing "multi-valued input with an error results in a single error out."
    (with-clean-system
      (let [[sender1 sender2 sender3 receiver] (tx-nodes
                                                (g/make-nodes world
                                                              [sender1 [ConstantPropertyNode :a-property 1]
                                                               sender2 [ConstantPropertyNode :a-property 2]
                                                               sender3 [ConstantPropertyNode :a-property 3]
                                                               receiver ErrorReceiverNode]
                                                              (g/connect sender1 :a-property receiver :multi)
                                                              (g/connect sender2 :a-property receiver :multi)
                                                              (g/connect sender3 :a-property receiver :multi)))
            _                                  (g/mark-defective! sender2 (g/error-fatal "Bad things have happened"))
            error-value                        (g/node-value receiver :multi-output)]
        (are [node label sev e] (and (= node (:_node-id e)) (= label (:_label e)) (= sev (:severity error-value)))
          receiver :multi-output :fatal   error-value
          receiver :multi        :fatal   (cause error-value)
          sender2  :a-property   :fatal   (cause (cause error-value)))))))

(g/defnode ListOutput
  (output list-output       g/Any (g/fnk []                  (list 1)))
  (output recycle           g/Any (g/fnk [list-output]       list-output))
  (output inner-list-output g/Any (g/fnk []                  [(list 1)]))
  (output inner-recycle     g/Any (g/fnk [inner-list-output] inner-list-output)))

(g/defnode ListInput
  (input list-input       g/Any)
  (input inner-list-input g/Any))

(g/defnode VecOutput
  (output vec-output       g/Any (g/fnk []                  [1]))
  (output recycle          g/Any (g/fnk [vec-output]        vec-output))
  (output inner-vec-output g/Any (g/fnk []                  (list [1])))
  (output inner-recycle    g/Any (g/fnk [inner-vec-output]  inner-vec-output)))

(g/defnode VecInput
  (input vec-input       g/Any)
  (input inner-vec-input g/Any))

(deftest list-values-are-preserved
  (with-clean-system
    (let [list-type      (type (list 1))
          [output input] (tx-nodes
                          (g/make-nodes world
                                        [output ListOutput
                                         input  ListInput]
                                        (g/connect output :list-output       input :list-input)
                                        (g/connect output :inner-list-output input :inner-list-input)))]
      (is (= list-type (type (g/node-value output :recycle))))
      (is (= list-type (type (g/node-value input  :list-input))))
      (is (= list-type (type (first (g/node-value output :inner-recycle)))))
      (is (= list-type (type (first (g/node-value input :inner-list-input))))))))

(deftest vec-values-are-preserved
  (with-clean-system
    (let [vec-type       (type (vector 1))
          [output input] (tx-nodes
                          (g/make-nodes world
                                        [output VecOutput
                                         input  VecInput]
                                        (g/connect output :vec-output       input :vec-input)
                                        (g/connect output :inner-vec-output input :inner-vec-input)))]
      (is (= vec-type (type (g/node-value output :recycle))))
      (is (= vec-type (type (g/node-value input  :vec-input))))
      (is (= vec-type (type (first (g/node-value output :inner-recycle)))))
      (is (= vec-type (type (first (g/node-value input :inner-vec-input))))))))

(g/defnode ConstantOutputNode
  (property real-val g/Any (default (list 1)))
  (output val g/Any (g/fnk [real-val] real-val)))

(deftest values-are-not-reconstructed-on-happy-path
  (with-clean-system
    (let [[const input] (tx-nodes
                         (g/make-nodes world
                                       [const  ConstantOutputNode
                                        input  ListInput]
                                       (g/connect const :val input :list-input)))]
      (is (identical? (g/node-value const :val) (g/node-value const :val)))
      (is (identical? (g/node-value const :val) (g/node-value input :list-input))))))
