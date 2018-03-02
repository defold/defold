(ns editor.library
  (:require [editor.prefs :as prefs]
            [editor.progress :as progress]
            [editor.settings-core :as settings-core]
            [editor.fs :as fs]
            [editor.url :as url]
            [clojure.java.io :as io]
            [clojure.string :as str])
  (:import [java.io File InputStream]
           [java.util.zip ZipInputStream]
           [java.net URI URLConnection HttpURLConnection]
           [org.apache.commons.io FilenameUtils]
           [org.apache.commons.codec.digest DigestUtils]))

(set! *warn-on-reflection* true)

(defn parse-library-uris [uri-string]
  (when uri-string
    (into []
          (keep url/try-parse)
          (str/split uri-string #"[,\s]"))))

(defmethod settings-core/parse-setting-value :library-list [_ raw]
  (parse-library-uris raw))

(defmethod settings-core/render-raw-setting-value :library-list [_ value]
  (when (seq value) (str/join "," value)))

(defn- mangle-library-uri [^URI uri]
  (DigestUtils/sha1Hex (str uri)))

(defn- str->b64 [^String s]
  (.encodeToString (java.util.Base64/getUrlEncoder) (.getBytes s "UTF-8")))

(defn- b64->str [^String b64str]
  (String. (.decode (java.util.Base64/getUrlDecoder) b64str) "UTF-8"))

(defn- library-uri-to-file-name ^String [uri tag]
  (str (mangle-library-uri uri) "-" (str->b64 (or tag "")) ".zip"))

(defn library-directory ^File [project-directory]
  (io/file (io/as-file project-directory) ".internal/lib"))

(defn library-file ^File [project-directory lib-uri tag]
  (io/file (library-directory project-directory)
           (library-uri-to-file-name lib-uri tag)))

(defn library-files [project-directory]
  (seq (.listFiles (library-directory project-directory))))

(defn- library-file-regexp [lib-uri]
  ;; matches any etag
  (re-pattern (str (mangle-library-uri lib-uri) "-(.*)\\.zip")))

(defn- find-matching-library [libs lib-uri]
  (let [lib-regexp (library-file-regexp lib-uri)]
    (first (keep #(when-let [match (re-matches lib-regexp (.getName ^File %))] {:file % :tag (b64->str (second match))}) libs))))

(defn- library-cache-info [project-directory lib-uris]
  (let [libs (library-files project-directory)]
    (map #(assoc (find-matching-library libs %) :uri %) lib-uris)))

(defn current-library-state [project-directory lib-uris]
  (map #(assoc % :status :unknown) (library-cache-info project-directory (distinct lib-uris))))

;; -----

(defn- http-response-code-to-status [code]
  (cond
    (= code HttpURLConnection/HTTP_NOT_MODIFIED) :up-to-date ; 304
    :else :stale))

(defn- parse-status [^HttpURLConnection http-connection]
  (if http-connection
    (http-response-code-to-status (.getResponseCode http-connection))
    :stale))

(defn- dump-to-temp-file! [^InputStream is]
  (let [f (fs/create-temp-file!)]
    (io/copy is f)
    f))

(defn- make-basic-auth-headers
  [^String user-info]
  {"Authorization" (format "Basic %s" (str->b64 user-info))})

(defn- make-defold-auth-headers
  [prefs]
  {"X-Auth" (prefs/get-prefs prefs "token" nil)
   "X-Email" (prefs/get-prefs prefs "email" nil)})

(defn- headers-for-uri [^URI lib-uri]
  (let [host (str/lower-case (.getHost lib-uri))
        user-info (.getUserInfo lib-uri)]
    (cond
      (some? user-info)
      (make-basic-auth-headers user-info)

      (some #{host} ["www.defold.com"])
      (make-defold-auth-headers (prefs/make-prefs "defold")))))

(defn default-http-resolver [^URI uri ^String tag]
  (let [http-headers (headers-for-uri uri)
        connection (.. uri toURL openConnection)
        http-connection (when (instance? HttpURLConnection connection) connection)]
    (when http-connection
      (doseq [[header value] http-headers]
        (.setRequestProperty http-connection header value))
      (when tag
        (.setRequestProperty http-connection "If-None-Match" tag)))
    (.connect connection)
    (let [status (parse-status http-connection)
          headers (.getHeaderFields connection)
          etag-keys ["ETag" "Etag"] ; Java HttpServer "normalises" headers to capitalised
          tag (or (first (some (partial get headers) etag-keys)) tag)]
      {:status status
       :stream (when (= status :stale) (.getInputStream connection))
       :tag tag})))

(defn- fetch-library! [resolver ^URI uri tag]
  (try
    (let [response (resolver uri tag)]
      {:status (:status response)
       :new-file (when (= (:status response) :stale) (dump-to-temp-file! (:stream response)))
       :tag (:tag response)})
    (catch Exception e
      {:status :error
       :reason :fetch-failed
       :exception e})))

(defn- fetch-library-update! [{:keys [tag uri] :as lib-state} resolver render-progress!]
  (let [progress (progress/make (str uri))]
    (render-progress! progress)
    ;; tag may not be available ...
    (merge lib-state (fetch-library! resolver uri tag))))

(defn- locate-zip-entry
  [zip-file file-name]
  (with-open [zip (ZipInputStream. (io/input-stream zip-file))]
    (loop [entry (.getNextEntry zip)]
      (if entry
        (let [parts (str/split (FilenameUtils/separatorsToUnix (.getName entry)) #"/")
              name (last parts)]
          (if (= file-name name)
            {:name name :path (str/join "/" (butlast parts))}
            (recur (.getNextEntry zip))))))))

(defn library-base-path
  [zip-file]
  (when-let [game-project-entry (locate-zip-entry zip-file "game.project")]
    (:path game-project-entry)))

(defn- validate-updated-library [lib-state]
  (merge lib-state
         (try
           (when-not (library-base-path (:new-file lib-state))
             {:status :error
              :reason :missing-game-project})
           (catch Exception e
             {:status :error
              :reason :invalid-zip
              :exception e}))))

(defn- purge-all-library-versions! [project-directory lib-uri]
  (let [lib-regexp (library-file-regexp lib-uri)
        lib-files (filter #(re-matches lib-regexp (.getName ^File %)) (library-files project-directory))]
    (doseq [^File lib-file lib-files]
      (fs/delete-file! lib-file {:fail :silently}))))

(defn- install-library! [project-directory {:keys [uri tag ^File new-file]}]
  (fs/copy-file! new-file (library-file project-directory uri tag)))

(defn- install-updated-library! [lib-state project-directory]
  (merge lib-state
         (try
           (purge-all-library-versions! project-directory (:uri lib-state))
           (let [file (install-library! project-directory lib-state)]
             {:status :up-to-date
              :file file})
           (catch Exception e
             {:status :error
              :reason :io-failure
              :exception e}))))

(defn fetch-library-updates [resolver render-progress! lib-states]
   (progress/mapv
     (fn [lib-state progress]
       (if (= (:status lib-state) :unknown)
         (fetch-library-update! lib-state resolver
                                (progress/nest-render-progress render-progress! progress))
         lib-state))
     lib-states
     render-progress!))

(defn validate-updated-libraries [lib-states]
  (mapv
    (fn [lib-state]
      (if (= (:status lib-state) :stale)
        (validate-updated-library lib-state)
        lib-state))
    lib-states))

(defn install-validated-libraries! [project-directory lib-states]
  (mapv
    (fn [lib-state]
      (-> (if (= (:status lib-state) :stale)
            (install-updated-library! lib-state project-directory)
            lib-state)
          (dissoc :new-file)))
    lib-states))

