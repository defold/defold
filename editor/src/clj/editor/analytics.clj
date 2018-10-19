(ns editor.analytics
  (:require [editor.system :as sys]
            [service.log :as log])
  (:import [com.defold.editor.analytics Analytics]
           [com.defold.editor Editor]))

(set! *warn-on-reflection* true)

(defonce ^:private ^Analytics analytics
  (when-some [version (sys/defold-version)]
    (Analytics. (str (Editor/getSupportPath)) "UA-83690-7" version)))

(defn enabled? []
  (some? analytics))

(defn- with-analytics [track-fn]
  (when analytics
    (try
      (track-fn)
      (catch Throwable error
        (log/error :exception error)))))

(defn set-user-id! [user-id]
  (assert (or (nil? user-id)
              (and (integer? user-id)
                   (pos? user-id))))
  (with-analytics #(.setUID analytics (str user-id))))

(defn track-event [category action label]
  (with-analytics #(.trackEvent analytics category action label)))

(defn track-exception [exception]
  (let [description (.getSimpleName (class exception))]
    (with-analytics #(.trackException analytics description))))

(defn track-screen [screen-name]
  (with-analytics #(.trackScreen analytics "defold" screen-name)))
