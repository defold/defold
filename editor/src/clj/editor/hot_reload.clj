(ns editor.hot-reload
  (:require [clojure.java.io :as io]
            [clojure.string :as string]
            [dynamo.graph :as g]
            [editor.pipeline :as pipeline]
            [editor.workspace :as workspace]
            [editor.resource :as resource])
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
  (let [build-path (FilenameUtils/normalize (str (workspace/build-path workspace)))
        path (subs url (count url-prefix))
        full-path (format "%s%s" build-path path)]
   ;; Avoid going outside the build path with '..'
   (if (string/starts-with? full-path build-path)
     (let [etag (workspace/etag workspace path)
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

(defn- string->url
  ^URI [^String str]
  (when (some? str)
    (try
      (URI. str)
      (catch java.net.URISyntaxException _
        nil))))

(defn- body->valid-entries [workspace ^bytes body]
  (into []
        (comp
          (keep (fn [line]
                  (let [[url-string etag] (string/split line #" ")]
                    (when-some [url (some-> url-string string->url)]
                      [url etag]))))
          (keep (fn [[^URI url etag]]
                  (let [path (.. url normalize getPath)]
                    (when (string/starts-with? path url-prefix)
                      (let [proj-path (subs path (count url-prefix))]
                        (when (= etag (workspace/etag workspace proj-path))
                          path)))))))
        (string/split-lines (String. body))))

(defn- v-e-handler [workspace project {:keys [url method headers ^bytes body]}]
  (if (not= method "POST")
    bad-request
    (let [body-str (string/join "\n" (body->valid-entries workspace body))
          body (.getBytes body-str "UTF-8")]
      {:code 200
       :headers {"Content-Length" (str (count body))}
       :body body})))

(defn verify-etags-handler [workspace project request]
  (v-e-handler workspace project request))
