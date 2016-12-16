(ns editor.engine
  (:require [clojure.java.io :as io]
            [dynamo.graph :as g]
            [editor.prefs :as prefs]
            [editor
             [dialogs :as dialogs]
             [protobuf :as protobuf]
             [resource :as resource]
             [console :as console]
             [ui :as ui]
             [targets :as targets]
             [workspace :as workspace]])
  (:import [com.defold.editor Platform]
           [java.net HttpURLConnection URI URL]
           [java.io File InputStream]
           [java.lang Process ProcessBuilder]
           [org.apache.commons.io IOUtils FileUtils]
           [javax.ws.rs.core MediaType]
           [com.sun.jersey.api.client Client ClientResponse WebResource WebResource$Builder]
           [com.sun.jersey.api.client.config ClientConfig DefaultClientConfig]
           [com.sun.jersey.multipart FormDataMultiPart]
           [com.sun.jersey.multipart.impl MultiPartWriter]
           [com.sun.jersey.core.impl.provider.entity InputStreamProvider StringProvider]
           [com.sun.jersey.multipart.file FileDataBodyPart StreamDataBodyPart]))

(set! *warn-on-reflection* true)

(def ^:const timeout 2000)

(defn- get-connection [^URL url]
  (doto ^HttpURLConnection (.openConnection url)
    (.setRequestProperty "Connection" "close")
    (.setConnectTimeout timeout)
    (.setReadTimeout timeout)
    (.setDoOutput true)
    (.setRequestMethod "POST")))

(defn reload-resource [target resource]
  (let [url  (URL. (str target "/post/@resource/reload"))
        conn ^HttpURLConnection (get-connection url)]
    (try
      (let [os (.getOutputStream conn)]
        (.write os ^bytes (protobuf/map->bytes
                           com.dynamo.resource.proto.Resource$Reload
                           {:resource (str (resource/proj-path resource) "c")}))
        (.close os))
      (let [is (.getInputStream conn)]
        (while (not= -1 (.read is))
          (Thread/sleep 10))
        (.close is))
      (catch Exception e
        (ui/run-later (dialogs/make-alert-dialog (str "Error connecting to engine on " target))))
      (finally
        (.disconnect conn)))))

(defn reboot [target local-url]
  (let [url  (URL. (str target "/post/@system/reboot"))
        conn ^HttpURLConnection (get-connection url)]
    (try
      (let [os  (.getOutputStream conn)]
        (.write os ^bytes (protobuf/map->bytes
                           com.dynamo.engine.proto.Engine$Reboot
                           {:arg1 (str "--config=resource.uri=" local-url)
                            :arg2 (str local-url "/game.projectc")}))
        (.close os))
      (let [is (.getInputStream conn)]
        (while (not= -1 (.read is))
          (Thread/sleep 10))
        (.close is)
        (.disconnect conn)
        :ok)
      (catch Exception e
        (.disconnect conn)
        false))))

(defn- pump-output [^InputStream stdout]
  (let [buf (byte-array 1024)]
    (loop []
      (let [n (.read stdout buf)]
        (when (> n -1)
          (let [msg (String. buf 0 n)]
            (console/append-console-message! msg)
            (recur)))))))

(defn- parent-resource [r]
  (let [workspace (resource/workspace r)
        path (resource/proj-path r)
        parent-path (subs path 0 (dec (- (count path) (count (resource/resource-name r)))))
        parent (workspace/resolve-workspace-resource workspace parent-path)]
    parent))

(defn- do-launch [path launch-dir prefs]
  (let [pb (doto (ProcessBuilder. ^java.util.List (list path))
             (.redirectErrorStream true)
             (.directory launch-dir))]
    (when (prefs/get-prefs prefs "general-quit-on-esc" false)
      (doto (.environment pb)
        (.put "DM_QUIT_ON_ESC" "1")))
    (let [p  (.start pb)
          is (.getInputStream p)]
      (.start (Thread. (fn [] (pump-output is)))))))

(defn- launch-bundled [launch-dir prefs]
  (let [suffix (.getExeSuffix (Platform/getHostPlatform))
        path   (format "%s/bin/dmengine%s" (System/getProperty "defold.unpack.path") suffix)]
    (do-launch path launch-dir prefs)))

(defn- copy-file [source-path dest-path]
  (io/copy (io/file source-path) (io/file dest-path)))

;; TODO - prototype for cloud-building
;; Should be re-written when we have the backend in place etc.
(defn launch-compiled [workspace launch-dir prefs]
  (let [server-url "http://localhost:9000"
        cc (DefaultClientConfig.)
        ; TODO: Random errors wihtout this... Don't understand why random!
        ; For example No MessageBodyWriter for body part of type 'java.io.BufferedInputStream' and media type 'application/octet-stream"
        _ (.add (.getClasses cc) MultiPartWriter)
        _ (.add (.getClasses cc) InputStreamProvider)
        _ (.add (.getClasses cc) StringProvider)
        client (Client/create cc)
        platform "x86_64-osx"
        ^WebResource resource (.resource ^Client client (URI. server-url))
        ; TODO: Make sure we can have a debug mode using DYNAMO_HOME here -- bdbf6ddaefb79c22214c86f945ea91d7fa6171de
        ^WebResource build-resource (.path resource (format "/build/%s/" platform))
        ^WebResource$Builder builder (.accept build-resource #^"[Ljavax.ws.rs.core.MediaType;" (into-array MediaType []))
        resources (g/node-value workspace :resource-list)
        manifests (filter #(= "ext.manifest" (resource/resource-name %)) resources)
        all-resources (filter #(= :file (resource/source-type %)) (mapcat resource/resource-seq (map parent-resource manifests)))]

    (try
      (with-open [form (FormDataMultiPart.)]
        (doseq [r all-resources]
          (.bodyPart form (StreamDataBodyPart. (resource/proj-path r) (io/input-stream r))))
          ; NOTE: We need at least one part..
        (.bodyPart form (StreamDataBodyPart. "__dummy__" (java.io.ByteArrayInputStream. (.getBytes ""))))
        (let [^ClientResponse cr (.post ^WebResource$Builder (.type builder MediaType/MULTIPART_FORM_DATA_TYPE) ClientResponse form)
              f (File/createTempFile "engine" "")]
          (.deleteOnExit f)
          (if (= 200 (.getStatus cr))
            (do
              (FileUtils/copyInputStreamToFile (.getEntityInputStream cr) f)
              (let [out-path (format "%s%s/%s" (workspace/build-path workspace) platform "dmengine")]
                (io/make-parents out-path)
                (copy-file (.getPath f) out-path )
                (.setExecutable (io/file out-path) true)
                (do-launch out-path launch-dir prefs)) )
            (do
              (console/append-console-message! (str (.getEntity cr String) "\n"))))))
      (catch Throwable e
        (console/append-console-message! (str e "\n"))))))

(defn launch [prefs workspace launch-dir]
  (if (prefs/get-prefs prefs "enable-extensions" false)
    (launch-compiled workspace launch-dir prefs)
    (launch-bundled launch-dir prefs)))
