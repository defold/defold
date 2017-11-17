(ns leiningen.util.http-cache
  (:require
    [clojure.java.io :as io]
    [clojure.string :as string])
  (:import
    (java.io File FilenameFilter FileNotFoundException)
    (java.net URI URL HttpURLConnection)
    (java.util.zip ZipFile ZipEntry)
    (org.apache.commons.io FilenameUtils)
    (org.apache.commons.io.filefilter WildcardFileFilter)))

(defn- glob [dir pattern]
  (let [^FilenameFilter filter (WildcardFileFilter. pattern)]
    (vec (.listFiles ^File (io/file dir) filter))))

(defn encode [s]
  (apply str (map #(format "%02x" %) (.getBytes s "UTF-8"))))

(defn decode [s]
  (let [bytes (into-array Byte/TYPE
                 (map (fn [[x y]]
                    (unchecked-byte (Integer/parseInt (str x y) 16)))
                       (partition 2 s)))]
    (String. bytes "UTF-8")))

(defn- mangle [url]
  (let [url (URL. url)]
    (format "%s%s" (string/replace (.getHost url) "." "_") (string/replace (.getPath url) "/" "-"))))

(defn- ->cache [root max-size]
  (let [root (string/replace-first root "~" (System/getProperty "user.home"))]
    (doto (File. root)
      (.mkdirs))
    {:root root :max-size max-size}))

(defn- join-paths [& ps]
  (string/join File/separator ps))

(defn- url->path [cache url]
  (join-paths (:root cache) (mangle url)))

(defn- cache-get [cache url]
  (let [path (url->path cache url)
        pattern (format "%s-*" (FilenameUtils/getName path))
        matches (glob (:root cache) pattern)]
    (when-let [^File f (first matches)]
      (if (.endsWith (.getName f) "_tmp")
        (do
          (io/delete-file f true)
          nil)
        (let [key (last (string/split (.getName f) #"-"))]
          (.setLastModified f (System/currentTimeMillis))
          [(.getAbsolutePath f) (decode key)])))))

(defn- accomodate [cache size]
  (let [matches (->> (glob (:root cache) "*")
                  (sort-by (fn [^File f] (- (.lastModified f)))))
        max-size (- (:max-size cache) size)
        keepers (loop [total-size 0
                       matches matches
                       result #{}]
                  (if-let [^File f (first matches)]
                    (let [ts (+ total-size (.length f))]
                      (if (< ts max-size)
                        (recur ts (next matches) (conj result f))
                        result))
                    result))]
    (doseq [^File f (remove keepers matches)]
      (io/delete-file f))))

(defn- cache-put [cache url key size]
  (let [path (url->path cache url)
        pattern (format "%s-*" (FilenameUtils/getName path))
        matches (glob (:root cache) pattern)]
    (doseq [^File f matches]
      (io/delete-file f true))
    (accomodate cache size)
    (format "%s-%s" path (encode key))))

(defn- http-get [url headers]
  (let [^HttpURLConnection conn (.openConnection (URL. url))]
    (doseq [[^String key ^String value] headers]
      (.setRequestProperty conn key value))
    (.connect conn)
    (try
      (let [headers (loop [i 1
                           headers {}]
                      (if-let [^String key (.getHeaderFieldKey conn i)]
                        (let [^String value (.getHeaderField conn i)]
                          (recur (inc i) (assoc headers key value)))
                        headers))]
        {:code (.getResponseCode conn)
         :input-stream (.getInputStream conn)
         :headers headers})
      (catch FileNotFoundException e
        {:code 404}))))

(defn download [url]
  (let [c (->cache "~/.dcache" (* 4 1000000000))
        hit (cache-get c url)
        headers (if hit
                  {"If-None-Match" (second hit)}
                  {})
        resp (http-get url headers)]
    (case (:code resp)
      200 (let [headers (:headers resp)
                size (Integer/parseInt (get headers "Content-Length" "0"))
                key (get headers "ETag" "")
                path (cache-put c url key size)
                tmp (str path "_tmp")]
            (println (format "downloading file '%s' -> '%s'" url path))
            (with-open [in (:input-stream resp)
                        out (io/output-stream tmp)]
              (io/copy in out))
            (let [dst (File. path)]
              (doto (File. tmp)
                (.renameTo dst))
              dst))
      304 (let [path (first hit)]
            (println (format "using cached file '%s' -> '%s'" url path))
            (io/file path))
      (do
        (println (format "could not find '%s'" url))
        nil))))
