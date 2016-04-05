;; This is what defnode should output.
;;
 ;; (defnode Foo
 ;;   (input x g/Str)
 ;;   (output bar g/Str (g/fnk [x] (.toUpperCase x))))

(require '[dynamo.graph :as g])

(clojure.pprint/pprint (macroexpand '(g/defnode6 Foo
                                       (input x g/Str)
                                       (property y g/Str)
                                       (output bar g/Str (g/fnk [x] (.toUpperCase x))))))



(g/defproperty prop1 g/Str)
(def schema1 {:firstname g/Str})

(clojure.pprint/pprint (macroexpand '(g/defnode6 Foo
                                       (output bar g/Str (g/fnk [this] (.toUpperCase "carin"))))))

(pst 30)

dynamo.graph/Properties

internal.graph.types/PropertyType

(do
  (def Foo
    (map->NodeTypeImpl {:inputs              {:x g/Str}
                        :injectable-inputs   #{}
                        :declared-properties {:y g/Str}
                        :passthroughs        #{:x}
                        :outputs             { ,,,}
                        :transforms          {}
                        :transform-types     {:bar }}
  )))


;;; parking lot
;;;
;;; - abstract outputs
;;; - override node type (as another impl of NodeType protocol)
;;; - ensure that production functions are fnks
;;; - review attach-properties-output. it got gross.
;;; - consider: build input arglist info during node type parsing
;;;     instead of rediscovering it during function generation.
;;; - revisit ip/property-value-type & its recursion

;; PROBLEM
;;
;; Inheritance.
;; By the time we get to inherit from a node def, it's already fully
;;evaluated. That means its functions have been turned into values and
;;protocol names have been replaced by protocol maps. We can't take a
;;value out of an already-defined node and re-emit it as code.

(inputs-needed x)

;; x is fnk -> input schema names
;; x is (fnk [this basis x y z]) -> #{:x :y :z}
;; x is property -> (union getter inputs, validation inputs, dynamics
;; inputs)

(require '[dynamo.graph :as g])
(require '[internal.node :as in])
(require '[internal.property :as ip])
(require '[internal.graph.types :as gt])

(in/inputs-needed '(fn [this basis x y z]))

(g/defproperty FooProp g/Str
  (dynamic hidden (g/fnk [this toggle-set]))
  (value (g/fnk [this old-value secret-input]))
  )

(mapcat internal.util/fnk-arguments (vals (ip/dynamic-attributes FooProp)))
(in/inputs-needed FooProp)

(ip/getter-for FooProp)

(g/defproperty BarProp g/Str
    (dynamic hidden (g/fnk [secret])))

BarProp
(g/defproperty BazProp BarProp
  (value (g/fnk [this secret-input] secret-input)))

BazProp
(ip/getter-for BarProp)

(ip/inheriting? 'BazProp)

(pst)

(g/defnode6 BazNode
  (input secret-input g/Str)
  (property zot g/Int)
  (property foo g/Str
            (value (g/fnk [this secret-input] secret-input)))
  (output happy-output g/Str (g/fnk [secret-input] (.toUpperCase secret-input))))

(require '[internal.util :as util])

BazNode
((-> BazNode :transforms :happy-output) {:secret-input "baa"})
(clojure.pprint/pprint (select-keys BazNode (keys BazNode)))

(def default-in-val "push")
(g/defnode6 Simple
  (property in g/Str
            (default default-in-val)))

(g/defnode6 Narf
  (inherits Simple)
  (property child g/Str))

(clojure.pprint/pprint (select-keys Narf (keys Narf)))

(= (-> Narf :transforms :in) (-> Simple :transforms :in))

(g/defproperty Wla {schema.core/Keyword schema.core/Any
                    :first-name g/Str})

(gt/declared-properties Narf)
(in/defaults Narf)

(gt/node-id   (g/construct Narf :in "pull"))
(gt/node-type (g/construct Narf))

(use 'clojure.repl)
(pst 40)

(gt/property-display-order Simple)
(gt/property-display-order Narf)

(ip/property-value-type g/Str)

(def empty-ctx (in/make-evaluation-context nil (g/now) false false false))

((-> Simple :transforms :in) {:this (g/construct Simple) :in "pull"})

(-> (in/node-type-forms6 'Narf '[(inherits Simple)])
     in/make-node-type-map
     :transforms
     :in)

(-> (in/node-type-forms6 'Narf '[(property in g/Str (default default-in-val))])
     in/make-node-type-map
     :transforms
     :in)


{:out (g/fnk [this] default)}

{:out (get-in Simple [:transforms :out])}


(def Narf (map->NodeTypeImpl
           {
            :transforms {:in (pc/fnk [this basis in] (gt/get-property this basis in))}
            :behaviors {:in ,,,xxx,,,

                        }

            }
           ))

{:this  ~(node-input-forms ,,,,)
 :basis ~(node-input-forms ,,,)
 :in    ~(node-input-forms ,,,)}

(in/lookup-from 'Simple Simple :transforms)





(ns first-ns
  (:require [dynamo.graph :as g]))
(in-ns 'first-ns)
(def default-in-val "first-ns/in")
(g/defnode6 Simple
  (property in g/Str
            (default default-in-val)))

(in-ns 'user)
(require '[first-ns :refer [Simple]])
(g/defnode6 Narf
  (inherits Simple)

)

Narf
