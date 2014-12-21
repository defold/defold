(ns editors.atlas-test
  (:require [clojure.test :refer :all]
            [editors.atlas :as atlas]))

(deftest bad-test
  (is (not (nil? (atlas/make-atlas-node)))))