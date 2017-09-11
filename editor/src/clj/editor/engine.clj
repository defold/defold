(ns editor.engine
  (:require [clojure.java.io :as io]
            [editor.process :as process]
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
           [java.io SequenceInputStream BufferedReader File InputStream ByteArrayOutputStream IOException]
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

(defn get-log-service-stream [target]
  (let [port (Integer/parseInt (:log-port target))
        socket-addr (InetSocketAddress. ^String (:address target) port)
        socket (doto (Socket.) (.setSoTimeout timeout))]
    (try
      (.connect socket socket-addr timeout)
      ;; closing is will also close the socket
      ;; https://docs.oracle.com/javase/7/docs/api/java/net/Socket.html#getInputStream()
      (let [is (.getInputStream socket)
            status (-> ^BufferedReader (io/reader is) (.readLine))]
        ;; The '0 OK' string is part of the log service protocol
        (if (= "0 OK" status)
          (do
            ;; Setting to 0 means wait indefinitely for new data
            (.setSoTimeout socket 0)
            is)
          (do
            (.close socket)
            nil)))
      (catch Exception e
        (.close socket)
        (throw e)))))

(defn- read-bytes-until [^InputStream stream pred]
  (let [byte-stream (ByteArrayOutputStream.)
        buffer (byte-array 1024)]
    (loop []
      (let [bytes (.toByteArray byte-stream)]
        (if (pred bytes)
          bytes
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

(defn- ->byte-parser [parser-fn]
  (fn [^bytes bytes] (parser-fn (String. bytes StandardCharsets/UTF_8))))

(defn ->enumeration [coll]
  (let [coll (atom coll)]
    (reify java.util.Enumeration
      (hasMoreElements [this]
        (boolean (seq @coll)))
      (nextElement [this]
        (let [val (first @coll)]
          (swap! coll rest)
          val)))))

(defn- sequence-input-stream [& streams]
  (SequenceInputStream. (->enumeration streams)))

(defn- do-launch [^File path launch-dir prefs]
  (let [defold-log-dir (some-> (System/getProperty "defold.log.dir")
                               (File.)
                               (.getAbsolutePath))
        command (.getAbsolutePath path)
        args (when defold-log-dir
               ["--config=project.write_log=1" (format "--config=project.log_dir=%s" defold-log-dir)])
        env {"DM_SERVICE_PORT" "dynamic"
             "DM_QUIT_ON_ESC" (if (prefs/get-prefs prefs "general-quit-on-esc" false)
                                "1" "0")}
        opts {:directory launch-dir
              :redirect-error-stream? true
              :env env}]
    ;; Closing "is" seems to cause any dmengine output to stdout/err
    ;; to generate SIGPIPE and close/crash. Also we need to read
    ;; the output of dmengine because there is a risk of the stream
    ;; buffer filling up, stopping the process.
    ;; https://www.securecoding.cert.org/confluence/display/java/FIO07-J.+Do+not+let+external+processes+block+on+IO+buffers
    (let [p (process/start! command args opts)
          is (.getInputStream p)
          parse-target-info-from-bytes (->byte-parser parse-target-info)]
      (if-let [initial-bytes-output (some-> (deref (future (read-bytes-until is parse-target-info-from-bytes)) engine-ports-output-timeout nil))]
        (let [target-info (parse-target-info-from-bytes initial-bytes-output)]
          (assoc target-info
                 :process p
                 :name (.getName path)
                 :log-stream (sequence-input-stream (io/input-stream initial-bytes-output) is)))
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
