(ns leiningen.lint
  (:require [leiningen.core.main :as main]))

;; The idea with calling eastwood from code like this is that we can filter out
;; the exceptions from the output streams so that we don't have to look at the
;; stuff that eastwood cannot parse.
;; TODO: Do that.

(defn lint
  [project & rest]
  (main/resolve-and-apply project ["eastwood"]))
