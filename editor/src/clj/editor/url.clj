(ns editor.url
  (:import [java.net HttpURLConnection MalformedURLException URL]
           [java.io IOException]))

(defn defold-hosted?
  [^URL url]
  (= "www.defold.com" (.getHost url)))

(defn reachable?
  [^URL url]
  (let [conn (doto ^HttpURLConnection (.openConnection url)
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
  ^URL [^URL url]
  (URL. (.getProtocol url) (.getHost url) (.getPort url) ""))

(defn try-parse
  ^URL [^String s]
  (try (URL. s) (catch MalformedURLException _ nil)))
