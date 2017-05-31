(ns editor.analytics
  (:require [service.log :as log])
  (:import [com.defold.editor.analytics Analytics]
           [com.defold.editor Editor]))

(set! *warn-on-reflection* true)

(defonce analytics (when-let [version (System/getProperty "defold.version")]
                     (Analytics. (str (Editor/getSupportPath)) "UA-83690-7" version)))

(defn track-event [category action label]
  (when analytics
    (try
      (.trackEvent ^Analytics analytics category action label)
      (catch Throwable e
        (log/error :exception e)))))

(defn track-screen [screen-name]
  (when analytics
    (try
      (.trackScreen ^Analytics analytics "defold" screen-name)
      (catch Throwable e
        (log/error :exception e)))))
