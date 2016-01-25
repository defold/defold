(ns dynamo.override-test
  (:require [clojure.test :refer :all]
            [dynamo.graph :as g]
            [support.test-support :refer :all]
            [internal.util :refer :all]))

(g/defnode MainNode
  (property a-property g/Str)
  (property b-property g/Str)
  (property virt-property g/Str
            (value (g/fnk [a-property] a-property)))
  (input sub-nodes g/NodeID :array :cascade-delete)
  (output sub-nodes [g/NodeID] (g/fnk [sub-nodes] sub-nodes))
  (output cached-output g/Str :cached (g/fnk [a-property] a-property))
  (input cached-values g/Str :array)
  (output cached-values [g/Str] :cached (g/fnk [cached-values] cached-values)))

(g/defnode SubNode
  (property value g/Str))

(defn- override [node-id]
  (-> (g/override node-id {})
    :tx-data
    tx-nodes))

(defn- setup
  ([world]
    (setup world 0))
  ([world count]
    (let [nodes (tx-nodes (g/make-nodes world [main [MainNode :a-property "main" :b-property "main"]
                                               sub SubNode]
                                        (g/connect sub :_node-id main :sub-nodes)))]
      (loop [result [nodes]
             counter count]
        (if (> counter 0)
          (recur (conj result (override (first (last result)))) (dec counter))
          result)))))

(deftest test-sanity
  (with-clean-system
    (let [[[main sub]] (setup world)]
      (testing "Original graph connections"
               (is (= [sub] (g/node-value main :sub-nodes)))))))

(deftest default-behaviour
   (with-clean-system
     (let [[[main sub] [or-main or-sub]] (setup world 1)]
       (testing "Connection is overridden from start"
               (is (= [or-sub] (g/node-value or-main :sub-nodes)))
               (is (= or-main (g/node-value or-main :_node-id))))
       (testing "Using base values"
                (doseq [label [:a-property :b-property :virt-property :cached-output]]
                  (is (= "main" (g/node-value or-main label)))))
       (testing "Base node invalidates cache"
                (g/transact (g/set-property main :a-property "new main"))
                (is (= "new main" (g/node-value or-main :cached-output)))))))

(deftest delete
  (with-clean-system
    (let [[[main sub] [or-main or-sub]] (setup world 1)]
      (testing "Overrides can be removed"
               (g/transact (g/delete-node or-main))
               (doseq [nid [or-main or-sub]]
                 (is (nil? (g/node-by-id nid))))
               (is (empty? (g/overrides main)))))
    (let [[[main sub] [or-main or-sub]] (setup world 1)]
      (testing "Overrides are removed with base"
               (g/transact (g/delete-node main))
               (doseq [nid [main sub or-main or-sub]]
                 (is (nil? (g/node-by-id nid))))
               (is (empty? (g/overrides main)))))
    (let [[[main sub] [or-main or-sub] [or2-main or2-sub]] (setup world 2)]
      (testing "Delete hierarchy"
               (g/transact (g/delete-node main))
               (doseq [nid [main sub or-main or-sub or2-main or2-sub]]
                 (is (nil? (g/node-by-id nid))))
               (is (empty? (g/overrides main)))))))

(deftest override-property
  (with-clean-system
    (let [[[main sub] [or-main or-sub]] (setup world 1)]
      (testing "Overriding property"
               (g/transact (g/set-property or-main :a-property "overridden main"))
               (is (= "overridden main" (g/node-value or-main :a-property)))
               (is (= "main" (g/node-value main :a-property)))
               (doseq [label [:_properties :_declared-properties]
                       :let [props (-> (g/node-value or-main label) (get :properties))
                             a-p (:a-property props)
                             b-p (:b-property props)]]
                 (is (= "overridden main" (:value a-p)))
                 (is (= "main" (:original-value a-p)))
                 (is (= or-main (:node-id a-p)))
                 (is (= "main" (:value b-p)))
                 (is (= false (contains? b-p :original-value)))
                 (is (= or-main (:node-id b-p)))))
      (testing "Virtual property"
               (is (= "overridden main" (g/node-value or-main :virt-property))))
      (testing "Output production"
               (is (= "overridden main" (g/node-value or-main :cached-output)))
               (is (= "main" (g/node-value main :cached-output))))
      (testing "Clearing property"
               (g/transact (g/clear-property or-main :a-property))
               (is (= "main" (g/node-value or-main :a-property)))
               (is (= "main" (g/node-value or-main :cached-output))))
      (testing "Update property"
               (g/transact (g/update-property or-main :a-property (fn [prop] (str prop "_changed"))))
               (is (= "main_changed" (g/node-value or-main :a-property)))))))

(deftest new-node-created
  (with-clean-system
    (let [[main or-main sub] (tx-nodes (g/make-nodes world [main MainNode]
                                                     (:tx-data (g/override main {}))
                                                     (g/make-nodes world [sub SubNode]
                                                                   (g/connect sub :_node-id main :sub-nodes))))]
      (let [sub-nodes (g/node-value or-main :sub-nodes)]
        (is (= 1 (count sub-nodes)))
        (is (not= [sub] sub-nodes)))
      (g/transact (g/disconnect sub :_node-id main :sub-nodes))
      (is (empty? (g/node-value or-main :sub-nodes))))))

(deftest new-node-created-cache-invalidation
  (with-clean-system
    (let [[main or-main] (tx-nodes (g/make-nodes world [main MainNode]
                                                 (:tx-data (g/override main {}))))
          _ (is (= [] (g/node-value or-main :cached-values)))
          [sub] (tx-nodes (g/make-nodes world [sub [SubNode :value "sub"]]
                                        (for [[from to] [[:_node-id :sub-nodes]
                                                         [:value :cached-values]]]
                                          (g/connect sub from main to))))]
      (is (= ["sub"] (g/node-value or-main :cached-values)))
      (g/transact (concat
                    (g/disconnect sub :_node-id main :sub-nodes)
                    (g/disconnect sub :value main :cached-values)))
      (is (= [] (g/node-value or-main :cached-values))))))

(deftest multi-override
  (with-clean-system
    (let [[[main sub]
           [or-main or-sub]
           [or2-main or2-sub]] (setup world 2)]
      (testing "basic default behaviour"
               (is (= "main" (g/node-value or2-main :a-property))))
      (testing "cache-invalidation in multiple overrides"
               (g/transact (g/set-property main :a-property "new main"))
               (is (= "new main" (g/node-value or2-main :a-property))))
      (testing "override one below overrides"
               (g/transact (g/set-property or-main :a-property "main2"))
               (is (= "main2" (g/node-value or2-main :a-property))))
      (testing "top level override"
               (g/transact (g/set-property or2-main :a-property "main3"))
               (is (= "main3" (g/node-value or2-main :a-property)))))))

(deftest mark-defective
  (with-clean-system
    (let [[[main sub]
           [or-main or-sub]] (setup world 1)]
      (testing "defective base"
               (let [error (g/error-severe {:type :som-error :message "Something went wrong"})]
                 (g/transact
                   (g/mark-defective main error))
               (is (g/error? (g/node-value or-main :a-property))))))
    (let [[[main sub]
           [or-main or-sub]] (setup world 1)]
      (testing "defective override, which throws exception"
               (let [error (g/error-severe {:type :som-error :message "Something went wrong"})]
                 (is (thrown? Exception (g/transact
                                          (g/mark-defective or-main error)))))))))

(deftest copy-paste
  (with-clean-system
    (let [[[main sub]
           [or-main or-sub]] (setup world 1)]
      (g/transact (g/set-property or-main :a-property "override"))
      (let [fragment (g/copy [or-main] {:traverse? (constantly true)})
            paste-data (g/paste (g/node-id->graph-id or-main) fragment {})
            copy-id (first (:root-node-ids paste-data))]
        (g/transact (:tx-data paste-data))
        (is (= "override" (g/node-value copy-id :a-property)))))))

(g/defnode ExternalNode
  (property value g/Keyword))

(g/defnode ResourceNode
  (property reference g/Keyword
            (value (g/fnk [in-reference] in-reference))
            (set (fn [basis node-id old-value new-value]
                   (concat
                     (g/disconnect-sources basis node-id :in-reference)
                     (let [gid (g/node-id->graph-id node-id)
                           node-store (g/graph-value gid :node-store)
                           src-id (get node-store new-value)]
                       (if src-id
                         (g/connect src-id :value node-id :in-reference)
                         []))))))
  (input in-reference g/Keyword))

(deftest connection-property
  (with-clean-system
    (let [[res a b] (tx-nodes (g/make-nodes world [res ResourceNode
                                                  a [ExternalNode :value :node-a]
                                                  b [ExternalNode :value :node-b]]
                                           (g/set-graph-value world :node-store {:node-a a :node-b b})))]
      (testing "linking through property setter"
               (g/transact (g/set-property res :reference :node-a))
               (is (= :node-a (g/node-value res :reference)))
               (g/transact (g/set-property a :value :node-a2))
               (is (= :node-a2 (g/node-value res :reference))))
      (let [or-res (first (override res))]
        (testing "override inherits the connection"
                 (is (= :node-a2 (g/node-value or-res :reference))))
        (testing "override the actual connection"
                 (g/transact (g/set-property or-res :reference :node-b))
                 (is (= :node-b (g/node-value or-res :reference))))
        (testing "clearing the property"
                 (g/transact (g/clear-property or-res :reference))
                 (is (= :node-a2 (g/node-value or-res :reference))))))))

(def IDPair [(g/one g/Str "id") (g/one g/NodeID "node-id")])

(g/defnode Node
  (property id g/Str)
  (property value g/Str)
  (output node-id IDPair (g/fnk [_node-id id] [id _node-id])))

(g/defnode Scene
  (property path g/Str)
  (input nodes g/NodeID :array :cascade-delete)
  (input node-ids IDPair :array)
  (input node-properties g/Properties :array)
  (output node-ids {g/Str g/NodeID} (g/fnk [node-ids] (into {} node-ids)))
  (output node-overrides g/Any :cached (g/fnk [node-ids node-properties]
                                              (let [node-ids (clojure.set/map-invert node-ids)
                                                    properties (group-by (comp node-ids :node-id second) (filter (comp :original-value second) (mapcat :properties node-properties)))]
                                                (into {} (map (fn [[nid props]] [nid (into {} (map (fn [[key value]] [key (:value value)]) props))]) properties))))))

(g/defnode Template
  (property path g/Any
            (set (fn [basis self old-value new-value]
                   (concat
                     (if-let [instance (g/node-value self :instance :basis basis)]
                       (g/delete-node instance)
                       [])
                     (let [gid (g/node-id->graph-id self)
                           path (:path new-value)]
                       (if-let [scene (get (g/graph-value basis gid :resources) path)]
                         (let [{:keys [id-mapping tx-data]} (g/override scene {})
                               mapping (comp id-mapping (g/node-value scene :node-ids :basis basis))
                               set-prop-data (for [[id props] (:overrides new-value)
                                                   :let [node-id (mapping id)]
                                                   [key value] props]
                                               (g/set-property node-id key value))]
                           (concat tx-data set-prop-data
                                   (for [[from to] [[:node-ids :node-ids]
                                                    [:node-overrides :node-overrides]]]
                                     (g/connect (id-mapping scene) from self to))))
                         []))))))

  (input instance g/NodeID)
  (input scene-path g/Str)
  (input node-ids g/Any)
  (input node-overrides g/Any))

(deftest scene-loading
  (with-clean-system
    (let [[node scene template] (tx-nodes (g/make-nodes world [node [Node :id "my-node" :value "initial"]
                                                               scene [Scene :path "my-scene"]
                                                               template [Template]]
                                                       (g/set-graph-value world :resources {"my-scene" scene})
                                                       (for [[output input] [[:_node-id :nodes]
                                                                             [:node-id :node-ids]
                                                                             [:_properties :node-properties]]]
                                                         (g/connect node output scene input))))
          overrides {"my-node" {:value "new value"}}]
      (g/transact (g/set-property template :path {:path "my-scene" :overrides overrides}))
      (is (= "initial" (g/node-value node :value)))
      (let [or-node (get (g/node-value template :node-ids) "my-node")]
        (is (= "new value" (g/node-value or-node :value))))
      (is (= overrides (g/node-value template :node-overrides))))))

;; User-defined dynamic properties

(g/defnode Script
  (property script-properties g/Any)
  (output _properties g/Properties :cached (g/fnk [_node-id _declared-properties script-properties]
                                                  (-> _declared-properties
                                                    (update :properties dissoc :script-properties)
                                                    (update :properties merge (into {} (map (fn [[key value]] [key {:value value
                                                                                                                    :type (g/make-property-type key (type value))
                                                                                                                    :node-id _node-id
                                                                                                                    :path [:script-properties key]}]) script-properties)))
                                                    (update :display-order (comp vec (partial remove #{:script-properties})))))))

(g/defnode Component
  (property id g/Str)
  (property component g/Any
            (set (fn [basis self old-value new-value]
                   (concat
                     (if-let [instance (g/node-value self :instance :basis basis)]
                       (g/delete-node instance)
                       [])
                     (let [gid (g/node-id->graph-id self)
                           path (:path new-value)]
                       (if-let [script (get (g/graph-value basis gid :resources) path)]
                         (let [{:keys [id-mapping tx-data]} (g/override script {})
                               or-script (id-mapping script)
                               script-props (g/node-value script :_properties :basis basis)
                               set-prop-data (for [[key value] (:overrides new-value)
                                                   :let [prop-path (get-in script-props [:properties key :path])]]
                                               (if prop-path
                                                 (g/update-property or-script (first prop-path) assoc-in (rest prop-path) value)
                                                 (g/set-property or-script key value)))
                               conn-data (for [[src tgt] [[:_node-id :instance]
                                                          [:_properties :script-properties]]]
                                           (g/connect or-script src self tgt))]
                           (concat tx-data set-prop-data conn-data))
                         []))))))
  (input instance g/NodeID)
  (input script-properties g/Properties)
  (output _properties g/Properties (g/fnk [_declared-properties script-properties]
                                          (-> _declared-properties
                                            (update :properties merge (:properties script-properties))
                                            (update :display-order concat (:display-order script-properties))))))

(deftest custom-properties
  (with-clean-system
    (let [[script comp] (tx-nodes (g/make-nodes world [script [Script :script-properties {:speed 0}]]
                                               (g/set-graph-value world :resources {"script.script" script}))
                                  (g/make-nodes world [comp [Component :component {:path "script.script" :overrides {:speed 10}}]]))]
      (let [p (get-in (g/node-value comp :_properties) [:properties :speed])]
        (is (= 10 (:value p)))
        (is (= 0 (:original-value p)))
        (g/transact (g/update-property (:node-id p) (first (:path p)) assoc-in (rest (:path p)) 20))
        (let [p' (get-in (g/node-value comp :_properties) [:properties :speed])]
          (is (= 20 (:value p')))
          (is (= 0 (:original-value p'))))
        (g/transact (g/update-property (:node-id p) (first (:path p)) dissoc (last (:path p))))
        (let [p' (get-in (g/node-value comp :_properties) [:properties :speed])]
          (is (= 0 (:value p')))
          (is (not (contains? p' :original-value))))))))
