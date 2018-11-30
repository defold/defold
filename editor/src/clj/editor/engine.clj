(ns editor.engine
  (:require [clojure.java.io :as io]
            [clojure.string :as str]
            [editor.defold-project :as project]
            [editor.engine.native-extensions :as native-extensions]
            [editor.prefs :as prefs]
            [editor.process :as process]
            [editor.protobuf :as protobuf]
            [editor.resource :as resource]
            [editor.system :as system]
            [editor.workspace :as workspace])
  (:import [com.defold.editor Platform]
           [com.dynamo.resource.proto Resource$Reload]
           [editor.workspace BuildResource]
           [java.io BufferedReader File InputStream IOException]
           [java.net HttpURLConnection InetSocketAddress Socket URI]
           [org.apache.commons.io FilenameUtils]))

(set! *warn-on-reflection* true)

(def ^:const timeout 2000)

(defn- get-connection [^URI uri]
  (doto ^HttpURLConnection (.openConnection (.toURL uri))
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
      (while (not= -1 (.read is buffer)))
      (catch IOException e
        ;; For instance socket closed because engine was killed
        nil))))

(defn- reload-resources-proj-path! [target proj-paths]
  (let [uri (URI. (str (:url target) "/post/@resource/reload"))
        conn ^HttpURLConnection (get-connection uri)]
    (try
      (with-open [os (.getOutputStream conn)]
        (.write os ^bytes (protobuf/map->bytes
                            Resource$Reload
                            {:resources proj-paths})))
      (with-open [is (.getInputStream conn)]
        (ignore-all-output is))
      (finally
        (.disconnect conn)))))

(defn reload-build-resources! [target build-resources]
  (let [build-resources-proj-path (into [] (keep (fn [resource]
                                                   (resource/proj-path resource))
                                                 build-resources))]
    (reload-resources-proj-path! target build-resources-proj-path)))

(defn reload-source-resource! [target resource]
  (assert (not (instance? BuildResource resource)))
  (let [build-ext (:build-ext (resource/resource-type resource))
        proj-path (resource/proj-path resource)
        build-resource-proj-path (str (FilenameUtils/removeExtension proj-path) "." build-ext)]
    (reload-resources-proj-path! target [build-resource-proj-path])))

(defn reboot! [target local-url debug?]
  (let [uri  (URI. (format "%s/post/@system/reboot" (:url target)))
        conn ^HttpURLConnection (get-connection uri)
        args (cond-> [(str "--config=resource.uri=" local-url)]
               debug?
               (conj (str "--config=bootstrap.debug_init_script=/_defold/debugger/start.luac"))

               true
               (conj (str local-url "/game.projectc")))]
    (try
      (with-open [os (.getOutputStream conn)]
        (.write os ^bytes (protobuf/map->bytes
                            com.dynamo.engine.proto.Engine$Reboot
                            (zipmap (map #(keyword (str "arg" (inc %))) (range)) args))))
      (with-open [is (.getInputStream conn)]
        (ignore-all-output is))
      :ok
      (finally
        (.disconnect conn)))))

(defn run-script! [target lua-module]
  (let [uri  (URI. (format "%s/post/@system/run_script" (:url target)))
        conn ^HttpURLConnection (get-connection uri)]
    (try
      (with-open [os  (.getOutputStream conn)]
        (let [bytes (protobuf/map->bytes com.dynamo.engine.proto.Engine$RunScript {:module lua-module})]
          (.write os ^bytes bytes)))
      (with-open [is (.getInputStream conn)]
        (ignore-all-output is))
      :ok
      (finally
        (.disconnect conn)))))

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

(def ^:private loopback-address "127.0.0.1")

(defn parse-launched-target-info [output]
  (let [log-port (second (re-find #"DLIB: Log server started on port (\d*)" output))
        service-port (second (re-find #"ENGINE: Engine service started on port (\d*)" output))]
    (merge (when service-port
             {:url (str "http://" loopback-address ":" service-port)
              :address loopback-address})
           (when log-port
             {:log-port log-port
              :address loopback-address}))))

(defn- do-launch [^File path launch-dir prefs debug?]
  (let [defold-log-dir (some-> (System/getProperty "defold.log.dir")
                               (File.)
                               (.getAbsolutePath))
        command (.getAbsolutePath path)
        args (cond-> []
               defold-log-dir
               (into ["--config=project.write_log=1"
                      (format "--config=project.log_dir=%s" defold-log-dir)])

               debug?
               (into ["--config=bootstrap.debug_init_script=/_defold/debugger/start.luac"]))
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
          is (.getInputStream p)]
      {:process p
       :name (.getName path)
       :log-stream is})))

(defn- bundled-engine [platform]
  (let [suffix (.getExeSuffix (Platform/getHostPlatform))
        path   (format "%s/%s/bin/dmengine%s" (system/defold-unpack-path) platform suffix)]
    (io/file path)))

(def custom-engine-pref-key "dev-custom-engine")

(defn- custom-engine
  [prefs platform]
  (when (system/defold-dev?)
    (when-some [custom-engine (prefs/get-prefs prefs custom-engine-pref-key nil)]
      (when-not (str/blank? custom-engine)
        (assert (= platform (.getPair (Platform/getHostPlatform))) "Can't use custom engine for platform different than host")
        (let [file (io/file custom-engine)]
          (assert (.exists file))
          file)))))

(defn get-engine [project prefs platform]
  (or (custom-engine prefs platform)
      (if (native-extensions/has-extensions? project)
        (let [build-server (native-extensions/get-build-server-url prefs)
              native-extension-roots (native-extensions/extension-roots project)]
          (native-extensions/get-engine project native-extension-roots platform build-server))
        (bundled-engine platform))))

(defn launch! [project prefs debug?]
  (let [launch-dir   (io/file (workspace/project-path (project/workspace project)))
        ^File engine (get-engine project prefs (.getPair (Platform/getJavaPlatform)))]
    (do-launch engine launch-dir prefs debug?)))
