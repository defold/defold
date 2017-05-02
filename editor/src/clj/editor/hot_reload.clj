(ns editor.hot-reload
  (:require [clojure.java.io :as io]
            [editor
             [workspace :as workspace]
             [resource :as resource]]
            [util.digest :as digest])
  (:import [java.io FileNotFoundException]
           [org.apache.commons.io IOUtils]))

(set! *warn-on-reflection* true)

(def ^:const url-prefix "/build")

(defn- content->bytes [content]
  (-> content io/input-stream IOUtils/toByteArray))

(defn- handler [workspace {:keys [url method headers]}]
  (let [path (format "%s%s" (workspace/build-path workspace) (subs url (count url-prefix)))]
    (try
      (with-open [is (io/input-stream path)]
        (let [c (IOUtils/toByteArray is)
              etag (digest/sha1->hex c)
              remote-etag (first (get headers "If-none-match"))
              cached? (= etag remote-etag)
              response-headers (cond-> {"ETag" etag}
                                 (= method "GET") (assoc "Content-Length" (str (count c))))]
          (cond-> {:code (if cached? 304 200)
                   :headers response-headers}
            (and (= method "GET") (not cached?)) (assoc :body c))))
      (catch FileNotFoundException _
        {:code 404}))))

(defn build-handler [workspace req-headers]
  (handler workspace req-headers))
