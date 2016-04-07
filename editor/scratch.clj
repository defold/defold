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

;;; weird cases

(comment
  (g/defnode6 Foo
   (property a g/Str)
   (output a g/Str (g/fnk [a] (.toUpperCase a))))

(get-in Foo [:transforms])
(:declared-properties-transforms Foo)


{:transforms {:_prop_a {:fn (g/fnk [this] (get this :_prop_a))
                           :output-type g/Str
                           :inputs [this]
              :a {:fn (g/fnk [a] (.toUpperCase a))
                           :output-type g/Str
                           :inputs [a]}}}}


;;; output a depends on property a. production function is called with
;;; a map {:a (value of property a)} assembled by gather-inputs

  (g/defnode6 Foo
    (property b g/Str)
    (output b g/Str (g/fnk [b] (.toUpperCase b)))
    (output c g/Str (g/fnk [b] (.toLowerCase b))))

(get-in Foo [:transforms])
(:declared-properties Foo)

;;; output c depends on output b. output b depends on property b

  (g/defnode6 Foo
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

  (g/defnode6 Beta
    (property prop1 g/Str)
    (output foo g/Str (g/fnk [this prop1 in-a] "cake")))

  (g/defnode6 BetaSimple
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

  (g/defnode6 Narf
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

  ((-> Simple :transforms :_prop_in :fn) {:this (g/construct Simple) :in "pull"})
  ((-> Simple :transforms :_prop_in :input-schema))

  (-> (in/node-type-forms6 'Narf '[(inherits Simple)])
      in/make-node-type-map
      :cached-outputs
      )

  (g/defnode6 Beta
    (property prop1 g/Str)
    (output foo g/Str (g/fnk [this prop1 in-a] "cake")))

  ((get-in Beta [:behaviors :foo]) Beta empty-ctx "Beta" 1 1 1)

   (g/defnode6 Narf
    (inherits Beta))

   (get-in Narf [:transforms])

  (-> (in/node-type-forms6 'Beta '[(property prop1 g/Str) (output foo g/Str (g/fnk [this prop1 in-a] "cake"))])
      in/make-node-type-map
      in/transform-plumbing-map)



  {:out (g/fnk [this] default)}

  {:out (get-in Simple [:transforms :out])}



  (in/lookup-from 'Simple Simple :transforms)
  (util/fnk-schema (g/fnk [a b c] (str a b c)))
  ()

  )
