(ns editor.analytics
  (:require [service.log :as log])
  (:import [com.defold.editor.analytics Analytics]
           [com.defold.editor Editor]))

(set! *warn-on-reflection* true)

(defonce *analytics* (Analytics. (str (Editor/getSupportPath)) "UA-83690-7" (System/getProperty "defold.version" "NO VERSION")))

(defn track-event [category action label]
  (try
    (.trackEvent ^Analytics *analytics* category action label)
    (catch Throwable e
      (log/error :exception e))))

(defn track-screen [screen-name]
  (try
    (.trackScreen ^Analytics *analytics* "defold" screen-name)
    (catch Throwable e
      (log/error :exception e))))
