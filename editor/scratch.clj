;; TODO

; 1) Change the way that we are doing attach-output in node.clj to
; handle inheritance.  we are going to modify the transforms so that
; each of the labels has its own map with information about the
; inputs, fn, and output-types
;; 2) Do the same thing with attach-property
;;; 3) Continue on our way to finish transform-plumbing-map and finish
; adding attach-behaviors
;; 3.a.) Same thing for input labels (you can call node-value to get
;        an input value too)
;; 3.b.) Similar work for _properties magic output. Also touches property validation.
;; 4) Then produce-value is going to lookup up the behavior in the
; :behaviors key and call it


;;; questions
;; transform-types and and transforms :output-type are the same - do
;; we need both?
;;; between :declared-properties and :declared-outputs - we most
;; likely do not need them to be named differently

;;; weird cases

(ns scratch
  (:require [dynamo.graph :as g]
            [internal.node :as in]
            [internal.property :as ip]
            [internal.graph.types :as gt]
            [internal.util :as util]
            [clojure.string :as str])
  (:use clojure.repl))

(comment
  (g/defnode Foo
    (property a g/Str)
    (output a g/Str (g/fnk [a] (.toUpperCase a))))

  (get-in Foo [:transforms])


;;; output a depends on property a. production function is called with
;;; a map {:a (value of property a)} assembled by gather-inputs

  (g/defnode Foo
    (property b g/Str)
    (output b g/Str (g/fnk [b] (.toUpperCase b)))
    (output c g/Str (g/fnk [b] (.toLowerCase b))))

  (get-in Foo [:transforms])
  (:declared-properties Foo)

;;; output c depends on output b. output b depends on property b

  (g/defnode Foo
    (input a g/Str :array)
    (output a g/Str (g/fnk [a] (str/join ", " a))))

;;; output a depends on input a


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

  (pst 50)

  (g/defnode BazNode
    (input secret-input g/Str)
    (property zot g/Int)
    (property foo g/Str
              (value (g/fnk [this secret-input] secret-input)))
    (output happy-output g/Str (g/fnk [secret-input] (.toUpperCase secret-input))))

  (clojure.pprint/pprint
   (in/make-node-type-map
    (in/node-type-forms 'BazNode
                        '[(input secret-input g/Str)
                          (property zot g/Int)
                          (property foo g/Str
                                    (value (g/fnk [this secret-input] secret-input)))
                          (output happy-output g/Str (g/fnk [secret-input] (.toUpperCase secret-input)))]
                        )))


  BazNode
  ((-> BazNode :transforms :happy-output :fn) {:secret-input "baa"})
  (clojure.pprint/pprint (select-keys BazNode (keys BazNode)))

  (clojure.pprint/pprint
   (in/make-node-type-map
    (in/node-type-forms 'Quux
                        '[(input secret-input String)
                          (property a-property g/Str
                                    (dynamic visible (g/fnk [foo] true)))
                          (property foo String)])))

  (def default-in-val "push")

  (clojure.pprint/pprint
   (in/make-node-type-map
    (in/node-type-forms 'TwoLayerDependencyNode
                        '[(property a-property g/Str)
                          (property internal-property g/Num
                                    (dynamic internal-dynamic (g/fnk [third-input] false))
                                    (validate (g/fnk [a-property] (when (nil? a-property) (g/error-info "Select a resource")))))])))

  (g/defnode Simple
    (property in g/Str
              (default default-in-val)))

  (g/defnode Beta
    (property prop1 g/Str)
    (output foo g/Str (g/fnk [this prop1 in-a] "cake")))

  (g/defnode BetaSimple
    (inherits Simple)
    (output tea g/Str (g/fnk [this] "tea")))

  (get-in Beta [:transforms :foo])

  (get-in BetaSimple [:transforms :tea])

  (in/collect-argument-schema :foo  (get-in Beta [:transforms :foo :input-schema]) Beta)

  (:declared-properties Beta)
  (:transforms Beta)
  (:transform-types Beta)

;;; #1) this is what we want to get to so that we don't
  ;; lose information by the time we get to gather inputs
  {:transforms {:foo {:fn (g/fnk [this basis prop1 in-a])
                      :output-type g/Str
                      :input-schema #{this basis prop1 in-a}}}}

  (g/defnode Narf
    (inherits Simple)
    (property child g/Str))

;;; #1) it means we will have to adjust the tranforms for the
  ;; inherits as well
  {:transforms {:foo {:fn (get-in Simple [:transforms :foo :fn])
                      :output-type g/Str
                      :input-schema #{this basis prop1 in-a}}}}


;;; #4) From produce-value to behavior to production function (transform)
;;;
;;; (get-in Simple [:behaviors :in]) ; --> function compiled from #(node-output-value-function)
;;; which calls (get-in Simple [:transforms :in :fn])
;;; and supplies it a map of input names to input values (:in is a
;;; #property so it only gets the node map and the property itself.)
;;;



  (-> (in/node-type-forms 'Narf '[(inherits Simple)])
      in/make-node-type-map

      )

  (-> (in/node-type-forms 'Beta '[(input an-input g/Str)])
      in/make-node-type-map
      :behaviors
      )

  (-> (in/node-type-forms
       'SetterFnPropertyNode
       '[(property underneath g/Int)
         (property self-incrementing g/Int
                   (value (g/fnk [underneath] underneath))
                   (set (fn [basis self old-value new-value]
                          (g/set-property self :underneath (or (and new-value (inc new-value)) 0)))))])
      in/make-node-type-map
      clojure.pprint/pprint)

  (:declared-properties Beta)

  (g/defnode Beta
    (property foo g/Str (default "cookies"))
    (property a-property g/Str (default "a"))
    (output foo g/Str (g/fnk [foo] (str  foo " and cake")))
    (output bar g/Str (g/fnk [a-property] (str "bar-" a-property)))
    (output baz g/Str (g/fnk [bar] (str "really " bar))))

  (g/defnode Beta
    (input an-input g/Str)
    (output an-output g/Str (g/fnk [an-input] (str "hey" an-input))))



  (g/defnode Alpha
    (output an-ouput g/Str (g/fnk [])))

  (:inputs Beta)

  (def empty-ctx (in/make-evaluation-context nil (g/now) false false false))
  (def n1 (g/construct Beta))
  (gt/produce-value (gt/node-type n1) n1 :foo empty-ctx)
  (in/property-has-no-overriding-output? Beta :foo)

  ;; start here in the morning
  (g/defnode Simple
    (property in g/Str
              (default default-in-val))
    )

  (def n2 (g/construct Simple))
  (gt/produce-value (gt/node-type n2) n2 :in empty-ctx)
  (g/node-value n2 :in)
  (keys Simple)
  (:substitutes Simple)
  (in/ordinary-input-labels Simple)

  (not (contains? {:foo g/Str} :foo))

  (keyword (name (symbol ":foo")))

  )
