(ns internal.luanode-test
  (:require [clojure.test :refer :all]
            [editor.luart :as luart]
            [dynamo.graph :as g]
            [internal.defnode-test :as defnode-test]
            [internal.graph.types :as gt]
            [internal.node :as in]
            [support.test-support :refer [tx-nodes with-clean-system]]
            [internal.util :as util]
            [schema.core :as s])
  (:import clojure.lang.Compiler))

(comment
  (g/defnode TwoLayerDependencyNode
    (property a-property g/Str)

    (output direct-calculation g/Str (g/fnk [a-property] a-property))
    (output indirect-calculation g/Str (g/fnk [direct-calculation] direct-calculation))))

(def TestNodeTypeDef
  {:property-display-order (internal.node/merge-display-order nil [:a-property])
   :key :internal.defnode-test/TwoLayerDependencyNode
   :input-dependencies {:_node-id #{:_node-id}, :_declared-properties #{:_properties}, :a-property #{:_declared-properties :direct-calculation :a-property :indirect-calculation :_properties}, :_output-jammers #{:_output-jammers}, :_basis #{:_overridden-properties}, :direct-calculation #{:indirect-calculation}}
   :property {:_node-id {:value-type #internal.node.ValueTypeRef{:k :dynamo.graph/NodeID}, :flags #{:unjammable :internal}, :dependencies #{}}, :_output-jammers {:value-type #internal.node.ValueTypeRef{:k :dynamo.graph/KeywordMap}, :flags #{:unjammable :internal}, :dependencies #{}}, :a-property {:value-type #internal.node.ValueTypeRef{:k :dynamo.graph/Str}, :flags #{}, :dependencies #{}}}
   :name internal.defnode-test/TwoLayerDependencyNode,
   :cascade-deletes #{}
   :behavior {:_overridden-properties {:fn (var internal.defnode-test/TwoLayerDependencyNode$behavior$_overridden-properties)},
              :direct-calculation {:fn (var internal.defnode-test/TwoLayerDependencyNode$behavior$direct-calculation)},
              :a-property {:fn (var internal.defnode-test/TwoLayerDependencyNode$behavior$a-property)},
              :_output-jammers {:fn (var internal.defnode-test/TwoLayerDependencyNode$behavior$_output-jammers)}
              :indirect-calculation {:fn (var internal.defnode-test/TwoLayerDependencyNode$behavior$indirect-calculation)}
              :_properties {:fn (var internal.defnode-test/TwoLayerDependencyNode$behavior$_properties)}
              :_node-id {:fn (var internal.defnode-test/TwoLayerDependencyNode$behavior$_node-id)}
              :_declared-properties {:fn (var internal.defnode-test/TwoLayerDependencyNode$behavior$_declared-properties)}}
   :output {:_node-id {:value-type #internal.node.ValueTypeRef{:k :dynamo.graph/NodeID}
                       :flags #{:unjammable :internal}
;;                       :fn (var internal.defnode-test/TwoLayerDependencyNode$output$_node-id)
                       :arguments #{:this :_node-id}
                       :dependencies #{:this :_node-id}}
            :_properties {:value-type #internal.node.ValueTypeRef{:k :dynamo.graph/Properties}
                          :flags #{:explicit}
                          :options {}
                          :fn (var internal.defnode-test/TwoLayerDependencyNode$output$_properties)
                          :arguments #{:_declared-properties}:dependencies #{:_declared-properties}}
            :_output-jammers {:value-type #internal.node.ValueTypeRef{:k :dynamo.graph/KeywordMap}
                              :flags #{:unjammable :internal}
;;                              :fn (var internal.defnode-test/TwoLayerDependencyNode$output$_output-jammers)
                              :arguments #{:this :_output-jammers}
                              :dependencies #{:this :_output-jammers}}
            :_overridden-properties {:value-type #internal.node.ValueTypeRef{:k :dynamo.graph/KeywordMap},:flags #{:explicit}, :options {}, :fn (var internal.defnode-test/TwoLayerDependencyNode$output$_overridden-properties), :arguments #{:this :_basis}, :dependencies #{:this :_basis}}
            :a-property {:value-type #internal.node.ValueTypeRef{:k :dynamo.graph/Str}
                         :flags #{}
;;                         :fn (var internal.defnode-test/TwoLayerDependencyNode$output$a-property)
                         :arguments #{:this :a-property}
                         :dependencies #{:this :a-property}}
            :direct-calculation {:value-type #internal.node.ValueTypeRef{:k :dynamo.graph/Str}
                                 :flags #{:explicit}
                                 :options {}
                                 :fn (var internal.defnode-test/TwoLayerDependencyNode$output$direct-calculation)
                                 :arguments #{:a-property}
                                 :dependencies #{:a-property}}
            :indirect-calculation {:value-type #internal.node.ValueTypeRef{:k :dynamo.graph/Str}
                                   :flags #{:explicit}
                                   :options {}
                                   :fn (var internal.defnode-test/TwoLayerDependencyNode$output$indirect-calculation)
                                   :arguments #{:direct-calculation}
                                   :dependencies #{:direct-calculation}}
            :_declared-properties {:value-type #internal.node.ValueTypeRef{:k :dynamo.graph/Properties}, :flags #{:cached}, :arguments #{:a-property}, :dependencies #{:a-property}}}
   :property-behavior {:a-property {:fn (var internal.defnode-test/TwoLayerDependencyNode$property-behavior$a-property)}
                       :_output-jammers {:fn (var internal.defnode-test/TwoLayerDependencyNode$property-behavior$_output-jammers)}
                       :_node-id {:fn (var internal.defnode-test/TwoLayerDependencyNode$property-behavior$_node-id)}}
   :internal-property #{:_output-jammers :_node-id}
   :property-order-decl [:a-property]
   :declared-property #{:a-property}
   :supertypes (quote nil)
   :input nil
   :display-order-decl nil})

(def TestNodeType (internal.node/register-node-type ::TestNodeType (internal.node/map->NodeTypeImpl TestNodeTypeDef)))

(deftest hello
  (with-clean-system
    
    (let [nid (first (g/tx-nodes-added (g/transact
                                         (g/make-node world TestNodeType))))]
      (g/transact (g/set-property nid :a-property "hello"))
      (is (= "hello" (g/node-value nid :a-property)))
      #_(is (= (g/node-value nid :test) 4711)))))



(defn- overridden-properties-behavior-fn [this evaluation-context]
  {})

(defn- output-jammers-behavior-fn [this evaluation-context]
  {})

(defn node-id-behavior-fn [this evaluation-context]
  nil)

(defn properties-behavior-fn [this evaluation-context]
  )

(defn declared-properties-behavior-fn [this evaluation-context]
  )

(defn make-property-output-behavior-fn [k]
  (fn [node evaluation-context]
    (internal.graph.types/get-property node (:basis evaluation-context) k)))

(defn make-output-behavior-fn [transform lua-fn args outputs properties inputs]
  #_(println "making output behavior for" keyword-name "from args:" args "outputs:" outputs "properties:" properties "inputs:" inputs)
  #_(fn [node evaluation-context]
      (if (= transform :this)
        (:_node-id node)
        (if i princip hela node ns )

        
        (println :node node)
        (let [arg-vals [] ;(map #(gt/produce-value node % evaluation-context) args)
              state (luart/make-state)
              env (luart/make-env state)]
          (println args arg-vals)
          (first (luart/exec state lua-fn arg-vals))))))

(defn output-jammers-output-fn [this _output-jammers]
  _output-jammers)

(defn node-id-output-fn [this _node-id]
  _node-id)

(defn properties-output-fn [_declared-properties]
  _declared-properties)

(defn overridden-properties-output-fn [this _basis]
  nil)

(defn make-output-output-fn [k lua-fn args]
  
  )

(defn make-property-output-fn [k]
  
  )

(defn make-property-behavior-fn [k]
  (fn [node evaluation-context]
;;    (println "hej")
    1))


(defn- lua-node-def->node-def [lua-node-def]
  (def lua-node-def lua-node-def)
;;  (println lua-node-def)
  (let [def-type-name (get lua-node-def "type_name")
        type-keyword (keyword "lua-node" def-type-name)
        type-name (str "lua-node/" def-type-name)
        properties (into {}
                         (map (fn [[k params]]
                                [(keyword k) params]))
                         (lua-node-def "properties"))
        outputs (into {}
                      (map (fn [[k {:strs [fun args]}]]
                             [(keyword k) {:lua-fn fun
                                           :args (into {} (map (fn [[name params]] [(keyword name) params])) args)}]))
                      (lua-node-def "outputs"))
        intrinsic-output-behaviors {:_overridden-properties {:fn overridden-properties-behavior-fn}
                                    :_output-jammers {:fn output-jammers-behavior-fn}
                                    :_node-id {:fn node-id-behavior-fn}
                                    :_properties {:fn properties-behavior-fn}
                                    :_declared-properties {:fn declared-properties-behavior-fn}}
        property-output-behaviors (into {}
                                        (map (fn [[k _]] [k {:fn (make-property-output-behavior-fn k)}]))
                                        properties)
        output-behaviors (into {}
                               (map (fn [[k {:keys [lua-fn args]}]] [k {:fn (make-output-behavior-fn k lua-fn args (set (keys outputs)) (set (keys properties)) #{})}]))
                               outputs)
        behavior-def (merge intrinsic-output-behaviors
                            property-output-behaviors
                            output-behaviors)

        intrinsic-outputs {:_node-id {:value-type g/NodeID
                                      :flags #{:unjammable :internal}
                                      :fn node-id-output-fn
                                      :arguments #{:this :_node-id}
                                      :dependencies #{:this :_node-id}}
                           :_properties {:value-type g/Properties
                                         :flags #{:explicit}
                                         :options {}
                                         :fn properties-output-fn
                                         :arguments #{:_declared-properties}:dependencies #{:_declared-properties}}
                           :_output-jammers {:value-type g/KeywordMap
                                             :flags #{:unjammable :internal}
                                             :fn output-jammers-output-fn
                                             :arguments #{:this :_output-jammers}
                                             :dependencies #{:this :_output-jammers}}
                           :_overridden-properties {:value-type g/KeywordMap
                                                    :flags #{:explicit}
                                                    :options {}
                                                    :fn overridden-properties-output-fn
                                                    :arguments #{:this :_basis}
                                                    :dependencies #{:this :_basis}}}
        output-outputs (into {}
                             (map (fn [[k {:keys [lua-fn args]}]]
                                    [k {:value-type g/Any
                                        :flags #{:explicit}
                                        :options {}
                                        :fn (make-output-output-fn k lua-fn args)
                                        :arguments (set args)
                                        :dependencies (set args)}]))
                             outputs)
        property-outputs (into {}
                               (map (fn [[k _]]
                                      [k {:value-type g/Any
                                          :flags #{}
                                          :fn (make-property-output-fn k)
                                          :arguments #{:this k}
                                          :dependencies #{:this k}
                                          }]))
                               properties)

        output-def (merge intrinsic-outputs
                          output-outputs
                          property-outputs)

        property-behavior-def (into {}
                                    (map (fn [[k _]]
                                           [k {:fn (make-property-behavior-fn k)}]))
                                    properties)
        ]


    

    (-> {:property-display-order (internal.node/merge-display-order nil (mapv (fn [[k v]] (keyword k)) (lua-node-def "properties")))
         :key type-keyword
         :input-dependencies (merge {:_node-id #{:_node-id}
                                     :_declared-properties #{:_properties}
                                     :_output-jammers #{:_output-jammers}
                                     :_basis #{:_overridden-properties}}
                                    (into {}
                                          (map (fn [[name {:strs [args]}]]
                                                 [(keyword name) (into #{} (map (comp keyword first)) args)]))
                                          (get lua-node-def "outputs")))
         :property {:_node-id {:value-type #internal.node.ValueTypeRef{:k :dynamo.graph/NodeID}, :flags #{:unjammable :internal}, :dependencies #{}},
                    :_output-jammers {:value-type #internal.node.ValueTypeRef{:k :dynamo.graph/KeywordMap}, :flags #{:unjammable :internal}, :dependencies #{}}}
         :name type-name
         :cascade-deletes #{}

         :behavior behavior-def
         :output output-def

         :property-behavior property-behavior-def

         ;; :output
         ;; :property-behavior

         :internal-property #{:_output-jammers :_node-id} ; TODO: why in every node def?
         :property-order-decl (mapv (comp keyword first) (lua-node-def "properties"))
         :declared-property (set (keys properties))
         :supertypes (quote nil)
         :input nil
         :display-order-decl nil
         }
      (update :property merge (into {} (map (fn [[k v]] [k {:value-type g/Any :flags #{} :dependencies #{}}]) properties))))))


(defn- node-def-properties->defnode-clauses [node-def]
  (map
    (fn [[prop-name prop-def]]
      (list 'property (symbol prop-name) g/Any))
    (get node-def "properties")))

(defn- node-def-inputs->defnode-clauses [node-def]
  (map
    (fn [[input-name input-def]]
      (let [array? (contains? input-def "array")]
        (if array?
          (list 'input (symbol input-name) g/Any :array)
          (list 'input (symbol input-name) g/Any))))
    (get node-def "inputs")))

(defn- node-def-outputs->defnode-clauses [node-def]
  (let [node-type-name (get node-def "type_name")]
    (map
      (fn [[output-name out-def]]
        (let [argument-names (map first (sort-by second (get out-def "args")))]
          `(~'output ~(symbol output-name) g/Any
            (g/fnk ~(into ['_evaluation-context] (map symbol) argument-names)
              (let [lua-state# (:lua-state ~'_evaluation-context)
                    lua-env# (:lua-env ~'_evaluation-context)
                    lua-node-types# (editor.luart/table-get lua-env# "nodetypes")
                    lua-type-module# (editor.luart/table-get lua-node-types# ~node-type-name)
                    node-type-outputs# (editor.luart/table-get lua-type-module# "outputs")
                    output-def# (editor.luart/table-get node-type-outputs# ~output-name)
                    output-production-function# (editor.luart/table-get output-def# "fun")]
                (editor.luart/lua->clj
                  (first (editor.luart/exec lua-state#
                                            output-production-function#
                                            ~(into []
                                                   (comp
                                                     (map symbol)
                                                     (map (fn [argument-symbol]
                                                            `(editor.luart/clj->lua ~argument-symbol))))
                                                   argument-names)))))))))
      (get node-def "outputs"))))

(defn- node-def->defnode-clauses [node-def]
  `(~(symbol (get node-def "type_name"))
    ~@(node-def-properties->defnode-clauses node-def)
    ~@(node-def-inputs->defnode-clauses node-def)
    ~@(node-def-outputs->defnode-clauses node-def)))

(g/defnode DummyOutputNode
  (output something g/Any (g/fnk [] "whatever")))

(defn play []
  (let [state (luart/make-state)
        env (luart/make-env state)
        loader (luart/make-loader "node-loader")
        src (slurp "test/internal/node_def.lua")
        main (luart/load-text-chunk env loader "node_def" src)
        executor (luart/direct-executor)
        module (first (luart/exec! executor state main []))
        lua-node-def (luart/lua->clj module)
        node-type-name (get lua-node-def "type_name")
        defnode-clauses (node-def->defnode-clauses lua-node-def)]

    (def last-src src)
    (def last-defnode-clauses defnode-clauses)
    (def node-def lua-node-def)

    (eval (apply g/rt-defnode defnode-clauses))

    (let [dynamic-node-type (some-> (resolve (symbol node-type-name)) deref)
          nodetypes (luart/make-table)]
      (luart/table-set! nodetypes (luart/table-get module "type_name") module)
      (luart/table-set! env "nodetypes" nodetypes)
      (with-clean-system
        (let [[nid on] (g/tx-nodes-added (g/transact (concat
                                                       (g/make-node world dynamic-node-type)
                                                       (g/make-node world DummyOutputNode))))]
          (g/transact (g/set-property nid :a_property "hello"))
          (g/transact (g/connect on :something nid :some_input))
          (g/transact (g/connect on :something nid :some_array_input))
          (let [evaluation-context (g/make-evaluation-context {:lua-state state :lua-env env})]
            (println :a_property (g/node-value nid :a_property evaluation-context))
            (println :direct_calculcation (g/node-value nid :direct_calculation evaluation-context))
            (println :hello (g/node-value nid :hello evaluation-context))
            (println :indirect_calculation (g/node-value nid :indirect_calculation evaluation-context))
            (println :some_input (g/node-value nid :some_input evaluation-context))
            (println :some_array_input (g/node-value nid :some_array_input evaluation-context))))))))




