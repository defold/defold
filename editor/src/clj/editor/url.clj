(ns editor.url
  (:import [java.net HttpURLConnection MalformedURLException URL URISyntaxException URI]
           [java.io IOException]))

(defn defold-hosted?
  [^URI uri]
  (= "www.defold.com" (.getHost uri)))

(defn reachable?
  [^URI uri]
  (let [url (.toURL uri)
        conn (doto ^HttpURLConnection (.openConnection url)
               (.setAllowUserInteraction false)
               (.setInstanceFollowRedirects false)
               (.setConnectTimeout 2000)
               (.setReadTimeout 2000)
               (.setUseCaches false))]
    (try
      (.connect conn)
      (.getContentLength conn)
      true
      (catch IOException _
        false)
      (finally
        (.disconnect conn)))))

(defn strip-path
  ^URI [^URI uri]
  (URI. (.getScheme uri) nil (.getHost uri) (.getPort uri) nil nil nil))

(defn try-parse
  ^URI [^String s]
  (try (.toURI (URL. s))
       (catch MalformedURLException _ nil)
       (catch URISyntaxException _ nil)))
