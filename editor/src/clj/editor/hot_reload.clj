(ns editor.hot-reload
  (:require [clojure.java.io :as io]
            [clojure.string :as string]
            [dynamo.graph :as g]
            [editor.pipeline :as pipeline]
            [editor.workspace :as workspace]
            [editor.resource :as resource]
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
  (let [build-path (FilenameUtils/normalize (workspace/build-path workspace))
        path (subs url (count url-prefix))
        full-path (format "%s%s" build-path path)]
   ;; Avoid going outside the build path with '..'
   (if (string/starts-with? full-path build-path)
     (let [etag (pipeline/etag workspace path)
           remote-etag (first (get headers "If-none-match"))
           cached? (when remote-etag (= etag remote-etag))
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
    (let [entries (->> (String. body)
                    string/split-lines
                    (keep (fn [line]
                            (when (seq line)
                              (let [[url etag] (string/split line #" ")
                                    path (-> (URI. url)
                                           (.normalize)
                                           (.getPath))
                                    local-path (subs path (count url-prefix))]
                                (when (= etag (pipeline/etag workspace local-path))
                                  path))))))
          out-body (string/join "\n" entries)]
      {:code 200
       :headers {"Content-Length" (str (count out-body))}
       :body out-body})))

(defn verify-etags-handler [workspace project request]
  (v-e-handler workspace project request))
