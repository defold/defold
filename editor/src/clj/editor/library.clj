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
           [java.net URL URLConnection HttpURLConnection]
           [org.apache.commons.io FilenameUtils]
           [org.apache.commons.codec.digest DigestUtils]))

(set! *warn-on-reflection* true)

(defn parse-library-urls [url-string]
  (when url-string
    (into [] (keep url/try-parse) (str/split url-string #"[,\s]"))))

(defmethod settings-core/parse-setting-value :library-list [_ raw]
  (parse-library-urls raw))

(defmethod settings-core/render-raw-setting-value :library-list [_ value]
  (when (seq value) (str/join "," value)))

(defn- mangle-library-url [^URL url]
  (DigestUtils/sha1Hex (str url)))

(defn- str->b64 [^String s]
  (.encodeToString (java.util.Base64/getUrlEncoder) (.getBytes s "UTF-8")))

(defn- b64->str [^String b64str]
  (String. (.decode (java.util.Base64/getUrlDecoder) b64str) "UTF-8"))

(defn- library-url-to-file-name ^String [url tag]
  (str (mangle-library-url url) "-" (str->b64 (or tag "")) ".zip"))

(defn library-directory ^File [project-directory]
  (io/file (io/as-file project-directory) ".internal/lib"))

(defn library-file ^File [project-directory lib-url tag]
  (io/file (library-directory project-directory)
           (library-url-to-file-name lib-url tag)))

(defn library-files [project-directory]
  (seq (.listFiles (library-directory project-directory))))

(defn- library-file-regexp [lib-url]
  ;; matches any etag
  (re-pattern (str (mangle-library-url lib-url) "-(.*)\\.zip")))

(defn- find-matching-library [libs lib-url]
  (let [lib-regexp (library-file-regexp lib-url)]
    (first (keep #(when-let [match (re-matches lib-regexp (.getName ^File %))] {:file % :tag (b64->str (second match))}) libs))))

(defn- library-cache-info [project-directory lib-urls]
  (let [libs (library-files project-directory)]
    (map #(assoc (find-matching-library libs %) :url %) lib-urls)))

(defn current-library-state [project-directory lib-urls]
  (map #(assoc % :status :unknown) (library-cache-info project-directory (distinct lib-urls))))

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

(defn- make-auth-headers [prefs]
  {"X-Auth" (prefs/get-prefs prefs "token" nil)
   "X-Email" (prefs/get-prefs prefs "email" nil)})

(defn- headers-for-url [^URL lib-url]
  (let [host (str/lower-case (.getHost lib-url))]
    (when (some #{host} ["www.defold.com"])
      (make-auth-headers (prefs/make-prefs "defold")))))

(defn default-http-resolver [^URL url ^String tag]
  (let [http-headers (headers-for-url url)
        connection (.openConnection url)
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

(defn- fetch-library! [resolver ^URL url tag]
  (try
    (let [response (resolver url tag)]
      {:status (:status response)
       :new-file (when (= (:status response) :stale) (dump-to-temp-file! (:stream response)))
       :tag (:tag response)})
    (catch Exception e
      {:status :error
       :reason :fetch-failed
       :exception e})))

(defn- fetch-library-update! [{:keys [tag url] :as lib-state} resolver render-progress!]
  (let [progress (progress/make (str url))]
    (render-progress! progress)
    ;; tag may not be available ...
    (merge lib-state (fetch-library! resolver url tag))))

(defn- locate-zip-entry
  [url file-name]
  (with-open [zip (ZipInputStream. (io/input-stream url))]
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

(defn- purge-all-library-versions! [project-directory lib-url]
  (let [lib-regexp (library-file-regexp lib-url)
        lib-files (filter #(re-matches lib-regexp (.getName ^File %)) (library-files project-directory))]
    (doseq [^File lib-file lib-files]
      (fs/delete-file! lib-file {:fail :silently}))))

(defn- install-library! [project-directory {:keys [url tag ^File new-file]}]
  (fs/copy-file! new-file (library-file project-directory url tag)))

(defn- install-updated-library! [lib-state project-directory]
  (merge lib-state
         (try
           (purge-all-library-versions! project-directory (:url lib-state))
           (let [file (install-library! project-directory lib-state)]
             {:status :up-to-date
              :file file})
           (catch Exception e
             {:status :error
              :reason :io-failure
              :exception e}))))

(defn update-libraries!
  ([project-directory lib-urls]
   (update-libraries! project-directory lib-urls default-http-resolver))
  ([project-directory lib-urls resolver]
   (update-libraries! project-directory lib-urls resolver progress/null-render-progress!))
  ([project-directory lib-urls resolver render-progress!]
   (let [lib-states (current-library-state project-directory lib-urls)]
     (progress/progress-mapv
      (fn [lib-state progress]
          (let [lib-state (if (= (:status lib-state) :unknown)
                            (fetch-library-update! lib-state resolver
                                                   (progress/nest-render-progress render-progress! progress))
                            lib-state)
                lib-state (if (= (:status lib-state) :stale)
                            (validate-updated-library lib-state)
                            lib-state)
                lib-state (if (= (:status lib-state) :stale)
                            (install-updated-library! lib-state project-directory)
                            lib-state)]
            (dissoc lib-state :new-file)))
      lib-states
      render-progress!))))
