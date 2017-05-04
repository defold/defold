(ns editor.hot-reload
  (:require [clojure.java.io :as io]
            [clojure.string :as string]
            [dynamo.graph :as g]
            [editor
             [workspace :as workspace]
             [resource :as resource]]
            [util.digest :as digest])
  (:import [java.io FileNotFoundException]
           [java.net URI]
           [org.apache.commons.io FilenameUtils IOUtils]))

(set! *warn-on-reflection* true)

(def ^:const url-prefix "/build")
(def ^:const verify-etags-url-prefix "/__verify_etags__")

(def ^:const not-found {:code 404})
(def ^:const bad-request {:code 400})

(defn- content->bytes [content]
  (-> content io/input-stream IOUtils/toByteArray))

(defn- handler [workspace project {:keys [url method headers]}]
  (let [build-path (workspace/build-path workspace)
        path (subs url (count url-prefix))
        full-path (FilenameUtils/normalize (format "%s%s" build-path (subs url (count url-prefix))))]
   ;; Avoid going outside the build path with '..'
   (if (string/starts-with? full-path build-path)
     (let [etags-cache @(g/node-value project :etags-cache)
           etag (get etags-cache path)
           remote-etag (first (get headers "If-none-match"))
           cached? (= etag remote-etag)
           content (when (not cached?)
                     (try
                       (with-open [is (io/input-stream full-path)]
                         (IOUtils/toByteArray is))
                       (catch FileNotFoundException _
                         :not-found)))]
       (if (= content :not-found)
         not-found
         (let [response-headers (cond-> {"ETag" etag}
                                  (= method "GET") (assoc "Content-Length" (if content (str (count content)) "-1")))]
           (cond-> {:code (if cached? 304 200)
                    :headers response-headers}
             (and (= method "GET") (not cached?)) (assoc :body content)))))
     not-found)))

(defn build-handler [workspace project request]
  (handler workspace project request))

(defn- v-e-handler [workspace project {:keys [url method headers ^bytes body]}]
  (if (not= method "POST")
    bad-request
    (let [etags-cache @(g/node-value project :etags-cache)
          entries (->> (String. body)
                    string/split-lines
                    (keep (fn [line] (let [[url etag] (string/split line #" ")
                                           path (-> (URI. url)
                                                  (.normalize)
                                                  (.getPath))
                                           local-path (subs path (count url-prefix))]
                                       (when (= etag (get etags-cache local-path))
                                         path)))))
          body (string/join "\n" entries)]
      {:code 200
       :headers {"Content-Length" (str (count body))}
       :body body})))

(defn verify-etags-handler [workspace project request]
  (v-e-handler workspace project request))
