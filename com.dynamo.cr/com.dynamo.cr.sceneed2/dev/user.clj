(defn macro-pretty-print
  [x]
  (clojure.pprint/write (macroexpand x) :dispatch clojure.pprint/code-dispatch))

(comment

  (def proj (ref (make-project (dynamo.project-test/->bitbucket))))
  (dosync (alter proj assoc :eclipse-project (.getProject (Activator/getDefault))))
  (load-resource proj (resource-at-path proj "/content/builtins/tools/atlas/core.clj"))
  (get-resource-value proj (get-in @proj [:graph :nodes 1]) :namespace)


  )


