(ns editor.game-project-core
  (:require [editor.settings-core :as settings-core]))

(set! *warn-on-reflection* true)

(def basic-meta-info (with-open [r (-> "meta.edn"
                                       settings-core/resource-reader
                                       settings-core/pushback-reader)]
                       (settings-core/load-meta-info r)))

(defn default-settings []
  (-> (:settings basic-meta-info)
      settings-core/make-default-settings
      settings-core/make-settings-map))
