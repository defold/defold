(ns scratch
  (:require [dynamo.graph :as g]
            [internal.value-test :refer :all]
            [internal.graph.error-values :as ie]
            [support.test-support :as ts]))

(def inp {:array [1 2 3 (ie/info "msg") (ie/warning "msg")] :se (ie/warning "se") :scalar 2})

(ie/error-aggregate (vals inp))

(let [bad-errors (ie/worse-than 10 (flatten (vals inp)))]
  (if (not (empty? bad-errors))
    (ie/error-aggregate bad-errors))
  )

(let [bad-errors (ie/worse-than 0 (flatten (vals {:unary-no-sub (ie/fatal :scalar)})))]
  (if (not (empty? bad-errors))
    (ie/error-aggregate bad-errors))
  )


(ts/with-clean-system
  (let [[receiver const] (ts/tx-nodes
                          (g/make-nodes world
                                        [receiver SubstitutingInputsNode
                                         const    ConstantNode]
                                        (g/connect const :scalar-with-error receiver :unary-no-sub)))]
    (g/node-value receiver :unary-no-sub)

    #_(g/node-value const :scalar-with-error)
    ))
