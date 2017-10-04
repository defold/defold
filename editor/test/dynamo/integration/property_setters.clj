(ns dynamo.integration.property-setters
  (:require [clojure.test :refer :all]
            [dynamo.graph :as g]
            [support.test-support :as ts]))

(g/defnode ResourceNode
  (property path g/Str (default ""))
  (output contents g/Str (g/fnk [path] path)))

(g/defnode ResourceUser
  (input source g/Str)

  (property reference g/Int
            (value (g/fnk [_node-id]
                   (ffirst (g/sources-of _node-id :source))))
            (set (fn [basis self old-value new-value]
                   (if new-value
                     (g/connect new-value :contents self :source)
                      (when-let [old-source (ffirst (g/sources-of self :source))]
                        (g/disconnect old-source :contents self :source))))))

  (property derived-property g/Str
            (value (g/fnk [source]
                          (and source (.toUpperCase source)))))

  (output transformed g/Str :cached (g/fnk [source] (and source (.toUpperCase source))))
  (output upstream    g/Int (g/fnk [reference] reference)))

(deftest fronting-a-connection-via-a-property
  (ts/with-clean-system
    (let [[provider user] (g/tx-nodes-added
                           (g/transact
                            (g/make-nodes world
                                          [provider [ResourceNode :path "/images/something.png"]
                                           user     ResourceUser])))]
      (is (= [] (g/sources (g/now) user :source)))
      (is (instance? Long provider))

      (g/set-property! user :reference provider)

      (is (= 1 (count (g/sources (g/now) user :source))))
      (is (= [provider :contents] (first (g/sources (g/now) user :source))))

      (is (= provider (g/node-value user :reference)))
      #_(is (= provider (get-in (g/node-value user :_properties) [:properties :reference :value])))
      #_(is (= provider (g/node-value user :upstream))))))

(defn- modified? [tx-report node property] (some #{[node property]} (:outputs-modified tx-report)))

(deftest dependencies-through-properties
  (testing "an output uses a property with a getter, changing the upstream node affects the output"
    (ts/with-clean-system
      (let [[provider user] (g/tx-nodes-added
                             (g/transact
                              (g/make-nodes world
                                            [provider [ResourceNode :path "/images/something.png"]
                                             user     ResourceUser])))]
        (is (= [] (g/sources (g/now) user :source)))
        (is (instance? Long provider))

        (g/set-property! user :reference provider)

        (let [tx-report (g/set-property! provider :path "/a/new/path")]
          (is      (modified? tx-report provider :path))
          (is      (modified? tx-report provider :_properties))
          (is      (modified? tx-report user     :derived-property))
          (is      (modified? tx-report user     :transformed))
          (is (not (modified? tx-report user     :upstream))))

        (let [tx-report (g/set-property! user :reference nil)]
          (is (not (modified? tx-report provider :path)))
          (is (not (modified? tx-report provider :_properties)))
          (is      (modified? tx-report user     :derived-property))
          (is      (modified? tx-report user     :transformed))
          (is      (modified? tx-report user     :upstream)))))))

(g/defnode ChainedProps
  (property final g/Str)
  (property chain-one g/Str (set (fn [basis self old-value new-value] (g/set-property self :final new-value))))
  (property chain-two g/Str (set (fn [basis self old-value new-value] (g/set-property self :chain-one new-value)))))

(deftest chained-props
  (testing "chain of properties being set through other properties' setters"
    (ts/with-clean-system
      (let [[chain] (g/tx-nodes-added
                      (g/transact
                        (g/make-nodes world [chain [ChainedProps :chain-two "test-val"]])))]
        (is (= "test-val" (g/node-value chain :final)))))))

(g/defnode DefaultSetter
  (property final g/Str)
  (property chain g/Str
    (default "test-val")
    (set (fn [basis self old-value new-value] (g/set-property self :final new-value)))))

(deftest default-setter
  (testing "default values are used even when there is a setter"
    (ts/with-clean-system
      (let [[node] (g/tx-nodes-added
                     (g/transact
                       (g/make-nodes world [node DefaultSetter])))]
        (is (= "test-val" (g/node-value node :final)))))))
