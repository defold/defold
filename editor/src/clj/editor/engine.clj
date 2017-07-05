(ns editor.engine
  (:require [clojure.java.io :as io]
            [editor.prefs :as prefs]
            [editor.dialogs :as dialogs]
            [editor.error-reporting :as error-reporting]
            [editor.protobuf :as protobuf]
            [editor.resource :as resource]
            [editor.ui :as ui]
            [editor.defold-project :as project]
            [editor.workspace :as workspace]
            [editor.engine.native-extensions :as native-extensions])
  (:import [com.defold.editor Platform]
           [java.net HttpURLConnection InetSocketAddress Socket URI URL]
           [java.io BufferedReader File InputStream ByteArrayOutputStream IOException]
           [java.nio.charset Charset StandardCharsets]
           [java.lang Process ProcessBuilder]))

(set! *warn-on-reflection* true)

(def ^:const timeout 2000)
(def ^:const engine-ports-output-timeout 3000)

(defn- get-connection [^URL url]
  (doto ^HttpURLConnection (.openConnection url)
    (.setRequestProperty "Connection" "close")
    (.setConnectTimeout timeout)
    (.setReadTimeout timeout)
    (.setDoOutput true)
    (.setRequestMethod "POST")))

(defn- ignore-all-output [^InputStream is]
  ;; Using a too small byte array here may fill the buffer and halt a
  ;; spamming engine. Same thing if using plain (.read).
  (let [buffer (byte-array 1024)]
    (try
      (while (not= -1 (.read is buffer))
        (Thread/sleep 10))
      (catch IOException e
        ;; For instance socket closed because engine was killed
        nil))))

(defn reload-resource [target resource]
  (let [url  (URL. (str target "/post/@resource/reload"))
        conn ^HttpURLConnection (get-connection url)]
    (try
      (with-open [os (.getOutputStream conn)]
        (.write os ^bytes (protobuf/map->bytes
                            com.dynamo.resource.proto.Resource$Reload
                            {:resource (str (resource/proj-path resource) "c")})))
      (with-open [is (.getInputStream conn)]
        (ignore-all-output is))
      (catch Exception e
        (ui/run-later (dialogs/make-alert-dialog (str "Error connecting to engine on " target))))
      (finally
        (.disconnect conn)))))

(defn reboot [target local-url]
  (let [url  (URL. (format "%s/post/@system/reboot" (:url target)))
        conn ^HttpURLConnection (get-connection url)]
    (try
      (with-open [os  (.getOutputStream conn)]
        (.write os ^bytes (protobuf/map->bytes
                            com.dynamo.engine.proto.Engine$Reboot
                            {:arg1 (str "--config=resource.uri=" local-url)
                             :arg2 (str local-url "/game.projectc")})))
      (with-open [is (.getInputStream conn)]
        (ignore-all-output is))
      (.disconnect conn)
      :ok
      (catch Exception e
        (.disconnect conn)
        false))))

(defn get-log-connection [target]
  (let [port (Integer/parseInt (:log-port target))
        socket-addr (InetSocketAddress. ^String (:address target) port)
        socket (doto (Socket.) (.setSoTimeout timeout))]
    (try
      (.connect socket socket-addr timeout)
      (let [is (.getInputStream socket)
            status (-> ^BufferedReader (io/reader is) (.readLine))]
        ;; The '0 OK' string is part of the log service protocol
        (if (= "0 OK" status)
          (do
            ;; Setting to 0 means wait indefinitely for new data
            (.setSoTimeout socket 0)
            {:socket socket :stream is})
          (do
            (.close socket)
            nil)))
      (catch Exception e
        (.close socket)
        (throw e)))))

(defn- read-output-until [^InputStream stream pred]
  ;; Can't use io/reader like in app_view log pump since closing the
  ;; reader also closes the stream, closing/crashing dmengine.
  (let [byte-stream (ByteArrayOutputStream.)
        buffer (byte-array 1024)]
    (loop []
      (let [output (String. (.toByteArray byte-stream) StandardCharsets/UTF_8)]
        (if (pred output)
          output
          (let [byte-count (.read stream buffer)]
            (when (> byte-count 0)
              (.write byte-stream buffer 0 byte-count)
              (recur))))))))

(def ^:private loopback-address "127.0.0.1")

(defn- parse-target-info [output]
  (let [log-port (second (re-find #"LOG: Started on port (\d*)" output))
        service-port (second (re-find #"SERVICE: Started on port (\d*)" output))]
    (when (and log-port service-port)
      {:url (str "http://" loopback-address ":" service-port)
       :log-port log-port
       :address loopback-address})))

(defn- do-launch [^File path launch-dir prefs]
  (let [defold-log-dir (some-> (System/getProperty "defold.log.dir")
                               (File.)
                               (.getAbsolutePath))
        ^java.util.List args (cond-> [(.getAbsolutePath path)]
                               defold-log-dir (conj "--config=project.write_log=1" (format "--config=project.log_dir=%s" defold-log-dir))
                               true list*)
        pb (doto (ProcessBuilder. args)
             (.redirectErrorStream true)
             (.directory launch-dir))]
    (when (prefs/get-prefs prefs "general-quit-on-esc" false)
      (doto (.environment pb)
        (.put "DM_QUIT_ON_ESC" "1")))
    (doto (.environment pb)
      (.put "DM_SERVICE_PORT" "dynamic")) ; let the engine service choose an available port
    ;; Closing "is" seems to cause any dmengine output to stdout/err
    ;; to generate SIGPIPE and close/crash. Also we need to read
    ;; the output of dmengine because there is a risk of the stream
    ;; buffer filling up, stopping the process.
    ;; https://www.securecoding.cert.org/confluence/display/java/FIO07-J.+Do+not+let+external+processes+block+on+IO+buffers
    (let [p (.start pb)
          is (.getInputStream p)]
      (if-let [local-target (some-> (deref (future (read-output-until is parse-target-info)) engine-ports-output-timeout nil)
                                    (parse-target-info))]
        (do (.start (Thread. (fn [] (ignore-all-output is))))
            (assoc local-target :process p :name (.getName path)))
        (do (.destroy p)
            nil)))))

(defn- bundled-engine [platform]
  (let [suffix (.getExeSuffix (Platform/getHostPlatform))
        path   (format "%s/%s/bin/dmengine%s" (System/getProperty "defold.unpack.path") platform suffix)]
    (io/file path)))

(defn get-engine [project prefs platform]
  (if-let [native-extension-roots (native-extensions/extension-roots project)]
    (let [build-server (native-extensions/get-build-server-url prefs)]
      (native-extensions/get-engine project native-extension-roots platform build-server))
    (bundled-engine platform)))

(defn launch [project prefs]
  (let [launch-dir   (io/file (workspace/project-path (project/workspace project)))
        ^File engine (get-engine project prefs (.getPair (Platform/getJavaPlatform)))]
    (do-launch engine launch-dir prefs)))
