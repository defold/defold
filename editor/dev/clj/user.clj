(ns user
  (:require [clojure.tools.namespace.repl :as ctn]))

(defn dev []
  (require 'jfx)   ;; This needs to happen before all the other stuff is read in

  (ctn/refresh)
  (in-ns 'dev))
