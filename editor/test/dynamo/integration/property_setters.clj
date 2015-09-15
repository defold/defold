(ns dynamo.integration.property-setters
  (:require [clojure.test :refer :all]
            [dynamo.graph :as g]
            [dynamo.integration.property-setters :refer :all]
            [support.test-support :as ts]))

(g/defnode ResourceNode
  (property path g/Str (default ""))
  (output contents g/Str (g/fnk [path] path)))

(g/defnode ResourceUser
  (input source g/Str)

  (property reference g/Int
            (get (fn [basis this prop]
                   (ffirst (g/basis-sources basis (g/node-id this) :source))))
            (set (fn [basis this prop value]
                   (g/basis-connect basis value :contents (g/node-id this) :source))))

  (output transformed g/Str (g/fnk [source] (and source (.toUpperCase source))))
  (output upstream    g/Int (g/fnk [reference] reference)))

(deftest fronting-a-connection-via-a-property
  (ts/with-clean-system
    (let [[provider user] (g/tx-nodes-added
                           (g/transact
                            (g/make-nodes world
                                          [provider [ResourceNode :path "/images/something.png"]
                                           user     ResourceUser])))]
      (is (= [] (g/basis-sources (g/now) user :source)))
      (is (instance? Long provider))

      (g/set-property! user :reference provider)

      (is (= 1 (count (g/basis-sources (g/now) user :source))))
      (is (= [provider :contents] (first (g/basis-sources (g/now) user :source))))

      (is (= provider (g/node-value user :reference)))
      (is (= provider (get-in (g/node-value user :_properties) [:properties :reference :value])))
      (is (= provider (g/node-value user :upstream))))))
