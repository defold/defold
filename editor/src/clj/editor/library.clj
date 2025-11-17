;; Copyright 2020-2025 The Defold Foundation
;; Copyright 2014-2020 King
;; Copyright 2009-2014 Ragnar Svensson, Christian Murray
;; Licensed under the Defold License version 1.0 (the "License"); you may not use
;; this file except in compliance with the License.
;;
;; You may obtain a copy of the License, together with FAQs at
;; https://www.defold.com/license
;;
;; Unless required by applicable law or agreed to in writing, software distributed
;; under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
;; CONDITIONS OF ANY KIND, either express or implied. See the License for the
;; specific language governing permissions and limitations under the License.

(ns editor.library
  (:require [clojure.java.io :as io]
            [clojure.string :as str]
            [editor.fs :as fs]
            [editor.localization :as localization]
            [editor.progress :as progress]
            [editor.resource :as resource]
            [editor.settings-core :as settings-core]
            [editor.system :as system]
            [editor.url :as url]
            [util.coll :refer [pair]]
            [util.fn :as fn])
  (:import [java.io File InputStream]
           [java.net HttpURLConnection URI]
           [java.nio.file.attribute FileTime]
           [java.util Base64]
           [java.util.zip ZipEntry ZipFile ZipInputStream ZipOutputStream]
           [org.apache.commons.codec.digest DigestUtils]
           [org.apache.commons.io FilenameUtils]))

(set! *warn-on-reflection* true)

(def ^:const connect-timeout 2000)
(def ^:const read-timeout 15000)

(defn parse-library-uris [uri-string]
  (settings-core/parse-setting-value {:type :list :element {:type :url}} uri-string))

(defn- mangle-library-uri [^URI uri]
  (DigestUtils/sha1Hex (str uri)))

(defn- str->b64 [^String s]
  (.encodeToString (Base64/getUrlEncoder) (.getBytes s "UTF-8")))

(defn- b64->str [^String b64str]
  (String. (.decode (Base64/getUrlDecoder) b64str) "UTF-8"))

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

(defn current-library-state
  "Returns a list of lib-states for the lib-uris.

  The lib-states have the following keys
  :file A File object to the corresponding lib zip in the library cache (.internal/lib), if it is already downloaded
  :tag The version/etag of the library, parsed from the file name, originally from the resolver response
  :status The current library status. Always :unknown since we don't know yet if this is the latest version of the library

  Statuses used elsewhere:
  * :error Something is wrong with the library, reason given in :reason, exception in :exception
  * :up-to-date The uri not modified according to resolver, etag the same, i.e. library is up to date.
  * :stale The uri modified according to resolver, etags differ, i.e. newer/other version available"
  [project-directory lib-uris]
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

(defn- replace-user-info-env-variable [^String user-info-token]
  (if (and (some? user-info-token)
           (str/starts-with? user-info-token "__")
           (str/ends-with? user-info-token "__"))
    (let [env-key (subs user-info-token 2 (- (count user-info-token) 2))
          env-value (System/getenv env-key)]
      (or env-value user-info-token))
    user-info-token))

(defn- make-basic-auth-headers
  [^String user-info]
  (let [[username password] (str/split user-info #":")
        username (replace-user-info-env-variable username)
        password (replace-user-info-env-variable password)
        user-info (cond-> username
                          password (str ":" password))
        user-info-b64 (str->b64 user-info)
        authorization (format "Basic %s" user-info-b64)]
    {"Authorization" authorization}))

(defn- headers-for-uri [^URI lib-uri]
  (let [user-info (.getUserInfo lib-uri)]
    (cond
      (some? user-info)
      (make-basic-auth-headers user-info))))

(defn default-http-resolver [^URI uri ^String tag]
  (let [http-headers (headers-for-uri uri)
        connection (.. uri toURL openConnection)
        http-connection (when (instance? HttpURLConnection connection) connection)]
    (when http-connection
      (doseq [[header value] http-headers]
        (.setRequestProperty http-connection header value))
      (when tag
        (.setRequestProperty http-connection "If-None-Match" tag))
      (.setRequestProperty http-connection "Accept" "application/zip"))
    (.setConnectTimeout connection connect-timeout)
    (.setReadTimeout connection read-timeout)
    (.connect connection)
    (let [status (parse-status http-connection)
          headers (.getHeaderFields connection)
          etag-keys ["ETag" "Etag"] ; Java HttpServer "normalises" headers to capitalised
          tag (or (first (some (partial get headers) etag-keys)) tag)]
      {:status status
       :stream (when (= status :stale) (.getInputStream connection))
       :tag tag})))

;; In dev builds, we replace patterns such as {{defold.extension.spine.url}}
;; with JVM property values. This ensures we can use a specific branch of an
;; extension in the integration tests as we develop new features. If there is a
;; corresponding system property `defold.extension.spine.path` that points to a
;; valid directory, we will use the contents of said directory in place of the
;; library url.
(def ^:private use-local-extension-dir!
  (if-not (system/defold-dev?)
    (constantly nil)
    (let [local-extension-dirs-by-library-uris
          (into {}
                (keep
                  (fn [[key value]]
                    (when (re-matches #"defold\.extension\.(.+?).url" key)
                      (when-some [library-uri (url/try-parse value)]
                        (let [local-path-key (str (subs key 0 (- (count key) 4)) ".path")
                              local-path (System/getProperty local-path-key)]
                          (when (some? local-path)
                            (let [local-dir (io/file local-path)]
                              (when (.isDirectory local-dir)
                                (pair library-uri (.getCanonicalFile local-dir))))))))))
                (System/getProperties))

          write-local-library-zip!
          (fn write-local-library-zip! [^File output-file ^File library-repo-dir]
            {:pre [(or (not (.exists output-file)) (.isFile output-file))
                   (.isDirectory library-repo-dir)]}
            (let [library-repo-dir (.getCanonicalFile library-repo-dir)]
              (with-open [zip-output-stream (ZipOutputStream. (io/output-stream output-file))]
                (reduce
                  (fn [_ ^File source-file]
                    (let [zip-entry-pathname (resource/relative-path library-repo-dir source-file)
                          zip-entry (doto (ZipEntry. zip-entry-pathname)
                                      (.setSize (.length source-file))
                                      (.setLastModifiedTime (FileTime/fromMillis (.lastModified source-file))))]
                      (.putNextEntry zip-output-stream zip-entry)
                      (io/copy source-file zip-output-stream)))
                  nil
                  (fs/file-walker library-repo-dir false ["build"]))
                (.finish zip-output-stream))))]

      (fn use-local-extension-dir! [^URI library-uri]
        (when-some [^File local-extension-dir (local-extension-dirs-by-library-uris library-uri)]
          (let [name (str "local-" (.getName local-extension-dir))]
            {:status :stale
             :tag name
             :new-file (doto (fs/create-temp-file! name ".zip")
                         (write-local-library-zip! local-extension-dir))}))))))

(defn- fetch-library-raw! [resolver ^URI uri tag]
  (try
    (or (use-local-extension-dir! uri)
        (let [response (resolver uri tag)]
          {:status (:status response)
           :new-file (when (= (:status response) :stale) (dump-to-temp-file! (:stream response)))
           :tag (:tag response)}))
    (catch Exception e
      {:status :error
       :reason :fetch-failed
       :exception e})))

(def ^:private fetch-library!
  ;; The tests are creating various new projects inside temporary folders. We
  ;; can speed things up by caching the fetched artifacts, so they do not have
  ;; to be downloaded over and over for each new project. This assumes the
  ;; libraries will not be updated on the server during our process lifetime.
  ;; True during tests, or when using the :cache-libraries lein profile.
  (if (Boolean/getBoolean "defold.cache.libraries")
    (fn/memoize fetch-library-raw!)
    fetch-library-raw!))

(defn- fetch-library-update! [{:keys [tag uri] :as lib-state} resolver render-progress!]
  (let [progress (progress/make (localization/message "progress.fetching" {"uri" uri}))]
    (render-progress! progress)
    ;; tag may not be available ...
    (merge lib-state (fetch-library! resolver uri tag))))

(defn- locate-zip-entry
  [^File zip-file file-name]
  (with-open [zip (ZipFile. zip-file)]
    (stream-reduce!
      (fn [_ ^ZipEntry entry]
        (let [parts (str/split (FilenameUtils/separatorsToUnix (.getName entry)) #"/")
              name (last parts)]
          (when (= file-name name)
            (reduced {:name name :path (str/join "/" (butlast parts))}))))
      nil
      (.stream zip))))

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

(defn- install-library!
  ^File [project-directory {:keys [uri tag ^File new-file]}]
  (second (first (fs/copy-file! new-file (library-file project-directory uri tag)))))

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

(defn fetch-library-updates
  "Fetch updates for libraries with :status :unknown.

  Will update:
  * :status to :up-to-date, :stale or :error (with :reason, :exception)
  * :new-file to downloaded file if any
  * :tag with etag from resolver"
  [resolver render-progress! lib-states]
  (progress/progress-mapv
    (fn [lib-state progress]
      (if (= (:status lib-state) :unknown)
        (fetch-library-update! lib-state resolver
                               (progress/nest-render-progress render-progress! progress))
        lib-state))
    lib-states
    render-progress!))

(defn validate-updated-libraries
  "Validate newly downloaded libraries (:status is :stale).

  Will update:
  :status to :error (with :reason, :exception) if the library is invalid"
  [lib-states]
  (mapv
    (fn [lib-state]
      (if (= (:status lib-state) :stale)
        (validate-updated-library lib-state)
        lib-state))
    lib-states))

(defn install-validated-libraries!
  "Installs the newly downloaded libraries (:status is still :stale).

  Will update:
  :status to :up-to-date or :error (with :reason, :exception)
  :file to File in the library cache
  Also, :new-file is removed since no longer interesting"
  [project-directory lib-states]
  (mapv
    (fn [lib-state]
      (-> (if (= (:status lib-state) :stale)
            (install-updated-library! lib-state project-directory)
            lib-state)
          (dissoc :new-file)))
    lib-states))
