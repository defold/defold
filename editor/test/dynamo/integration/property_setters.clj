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
            (value (g/fnk [this]
                   (ffirst (g/sources-of (g/node-id this) :source))))
            (set (fn [basis this value]
                   (if value
                     (g/connect value :contents (g/node-id this) :source)
                      (when-let [old-source (ffirst (g/sources-of (g/node-id this) :source))]
                        (g/disconnect old-source :contents (g/node-id this) :source))))))

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
      (is (= [] (g/basis-sources (g/now) user :source)))
      (is (instance? Long provider))

      (g/set-property! user :reference provider)

      (is (= 1 (count (g/basis-sources (g/now) user :source))))
      (is (= [provider :contents] (first (g/basis-sources (g/now) user :source))))

      (is (= provider (g/node-value user :reference)))
      (is (= provider (get-in (g/node-value user :_properties) [:properties :reference :value])))
      (is (= provider (g/node-value user :upstream))))))

(defn- modified? [tx-report node property] (some #{[node property]} (:outputs-modified tx-report)))

(deftest dependencies-through-properties
  (testing "an output uses a property with a getter, changing the upstream node affects the output"
    (ts/with-clean-system
      (let [[provider user] (g/tx-nodes-added
                             (g/transact
                              (g/make-nodes world
                                            [provider [ResourceNode :path "/images/something.png"]
                                             user     ResourceUser])))]
        (is (= [] (g/basis-sources (g/now) user :source)))
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
