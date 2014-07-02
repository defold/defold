(defn macro-pretty-print
  [x]
  (clojure.pprint/write (macroexpand x) :dispatch clojure.pprint/code-dispatch))
