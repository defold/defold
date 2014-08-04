(ns dynamo.test-environment
  (:require [clojure.osgi.core :refer [*bundle*]]
            [clojure.java.io :refer [file as-url]]))

(defn locate-test-file [p n]
  (if *bundle*
    (.nextElement (.findEntries *bundle* p n true))
    (as-url (file (str p java.io.File/separator n)))))
