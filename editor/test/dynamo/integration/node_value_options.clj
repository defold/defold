(ns dynamo.integration.node-value-options
  (:require [clojure.test :refer :all]
            [dynamo.graph :as g]
            [internal.graph.error-values :as ie]
            [support.test-support :as ts]))

;;; ----------------------------------------
;;; :no-cache

(g/defnode CacheTestNode
  (input first-name g/Str)
  (input last-name  g/Str)
  (input operand    g/Str)

  (property scalar g/Str)

  (output uncached-value  g/Str         (g/fnk [scalar] scalar))
  (output expensive-value g/Str :cached (g/fnk [] "this took a long time to produce"))
  (output nickname        g/Str :cached (g/fnk [first-name] first-name))
  (output derived-value   g/Str :cached (g/fnk [first-name last-name] (str first-name " " last-name)))
  (output another-value   g/Str :cached (g/fnk [] "this is distinct from the other outputs"))
  (output nil-value       g/Str :cached (g/fnk [this] nil)))

(defn- cached? [cache node label]
  (contains? cache [node label]))

(defn- touch [node label] (g/node-value node label {:no-cache true}))

(defn- test-node [world] (first (ts/tx-nodes (g/make-node world CacheTestNode))))
(defn- connected-test-nodes
  [world]
  (ts/tx-nodes
   (g/make-nodes world
                 [node1 CacheTestNode
                  node2 CacheTestNode]
                 (g/connect node1 :derived-value node2 :first-name))))

(deftest node-value-allows-no-cache-option
  (testing ":no-cache options works on direct outputs"
    (ts/with-clean-system
      (let [node (test-node world)]
        (touch node :derived-value)
        (is (not (cached? cache node :derived-value))))))

  (testing ":no-cache option works on indirect outputs"
    (ts/with-clean-system
      (let [[node1 node2] (connected-test-nodes world)]
        (touch node2 :derived-value)
        (is (not (cached? cache node1 :derived-value)))
        (is (not (cached? cache node2 :derived-value)))))))


;;; ----------------------------------------
;;; :ignore-errors

(g/defnode ConstantNode
  (property constant g/Any))

(g/defnode ReceiverNode
  (input single g/Any)
  (input array  g/Any :array)

  (output single-output g/Any (g/fnk [single] single))
  (output array-output  [g/Any] (g/fnk [array] array)))

(defn error-test-nodes [world const]
  (ts/tx-nodes
   (g/make-nodes world [sender [ConstantNode :constant const]
                        receiver ReceiverNode]
                 (g/connect sender :constant receiver :single))))

(defn error-test-nodes-multiple [world & consts]
  (let [receiver (first (ts/tx-nodes (g/make-node world ReceiverNode)))]
    (doseq [const consts]
      (let [cnode (first (ts/tx-nodes (g/make-node world ConstantNode :constant const)))]
        (g/connect! cnode :constant receiver :array)))
    receiver))

(defn ignored-single? [error-level ignore-level]
  (ts/with-clean-system
    (let [[_ receiver]   (error-test-nodes world (assoc (ie/error-value error-level nil) :value 88))
          value-returned (g/node-value receiver :single-output {:ignore-errors ignore-level})]
      (not (ie/error? value-returned)))))

(defn ignored-multiple? [error-level ignore-level]
  (ts/with-clean-system
    (let [receiver       (error-test-nodes-multiple world (assoc (ie/error-value error-level nil) :value 10101))
          value-returned (g/node-value receiver :array-output {:ignore-errors ignore-level})]
      (not (ie/error? value-returned)))))

(deftest node-value-allows-ignore-errors-option
  (testing "errors at or less than the ignore level are indeed ignored"
    (are [error-level ignore-level] (ignored-single? error-level ignore-level)
      :info    :info
      :info    :warning
      :info    :fatal

      :warning :warning
      :warning :fatal

      :fatal   :fatal))

  (testing "errors in an array output at or less than the ignore level are indeed ignored"
    (are [error-level ignore-level] (ignored-multiple? error-level ignore-level)
      :info    :info
      :info    :warning
      :info    :fatal

      :warning :warning
      :warning :fatal

      :fatal   :fatal))

  (testing "errors worse than the ignore level are not ignored"
    (are [error-level ignore-level] (not (ignored-single? error-level ignore-level))
      :warning :info

      :fatal   :info
      :fatal   :warning))

  (testing "errors in an array output worse than the ignore level are not ignored"
    (are [error-level ignore-level] (not (ignored-multiple? error-level ignore-level))
      :warning :info

      :fatal   :info
      :fatal   :warning)))

(defn info [value] (assoc (ie/error-info    "message not used") :value value))
(defn warn [value] (assoc (ie/error-warning "message not used") :value value))

(deftest production-function-uses-contained-value
  (comment (testing "single-valued output"
     (ts/with-clean-system
       (let [[_ receiver] (error-test-nodes world (warn 88))]
         (is (= 88 (g/node-value receiver :single-output {:ignore-errors :fatal})))
         (is (= 88 (g/node-value receiver :single-output {:ignore-errors :warning})))
         (is (g/error? (g/node-value receiver :single-output {:ignore-errors :info})))))))

  (testing "multi-valued output"
    (ts/with-clean-system
      (let [receiver (error-test-nodes-multiple world 1 2 3 (warn 4) (info 5))]
        (is (= [1 2 3 4 5] (g/node-value receiver :array-output {:ignore-errors :warning})))
        (is (g/error? (g/node-value receiver :array-output {:ignore-errors :info})))
        (is (= :warning (:severity (g/node-value receiver :array-output {:ignore-errors :info}))))))))
