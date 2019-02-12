(ns editor.engine
  (:require [clojure.java.io :as io]
            [clojure.string :as str]
            [editor.engine.native-extensions :as native-extensions]
            [editor.fs :as fs]
            [editor.process :as process]
            [editor.prefs :as prefs]
            [editor.process :as process]
            [editor.protobuf :as protobuf]
            [editor.resource :as resource]
            [editor.system :as system])
  (:import [com.defold.editor Platform]
           [com.dynamo.resource.proto Resource$Reload]
           [java.io BufferedReader File InputStream IOException]
           [java.net HttpURLConnection InetSocketAddress Socket URI]
           [java.util.zip ZipFile]))

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

(defn reload-build-resources! [target build-resources]
  (let [uri (URI. (str (:url target) "/post/@resource/reload"))
        conn ^HttpURLConnection (get-connection uri)
        proj-paths (mapv resource/proj-path build-resources)]
    (try
      (with-open [os (.getOutputStream conn)]
        (.write os ^bytes (protobuf/map->bytes
                            Resource$Reload
                            {:resources proj-paths})))
      (with-open [is (.getInputStream conn)]
        (ignore-all-output is))
      (finally
        (.disconnect conn)))))

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

(def ^:private exe-suffixes (into {}
                                  (map (fn [^Platform platform]
                                         [(.getPair platform) (.getExeSuffix platform)]))
                                  [Platform/Arm64Darwin
                                   Platform/Armv7Android
                                   Platform/Armv7Darwin
                                   Platform/JsWeb
                                   Platform/X86Win32
                                   Platform/X86_64Darwin
                                   Platform/X86_64Linux
                                   Platform/X86_64Win32]))

(defn- dmengine-filename [platform]
  (format "dmengine%s" (get exe-suffixes platform "")))

(defn- bundled-engine [platform]
  (let [path   (format "%s/%s/bin/%s" (system/defold-unpack-path) platform (dmengine-filename platform))
        engine (io/file path)]
    {:id {:type :bundled :path (.getCanonicalPath engine)} :dmengine engine :platform platform}))

(def custom-engine-pref-key "dev-custom-engine")

(defn- dev-custom-engine
  [prefs platform]
  (when (system/defold-dev?)
    (when-some [custom-engine (prefs/get-prefs prefs custom-engine-pref-key nil)]
      (when-not (str/blank? custom-engine)
        (assert (= platform (.getPair (Platform/getHostPlatform))) "Can't use custom engine for platform different than host")
        (let [engine (io/file custom-engine)]
          (assert (.exists engine))
          {:id {:type :dev :path (.getCanonicalPath engine)} :dmengine engine :platform platform})))))

(defn get-engine
  "Returns an engine descriptor map:
  {:id {:type <:dev :bundled :custom> + entries for sha/path - something identifying this engine version}
   :dmengine <File to bundled/dev dmengine>
   :engine-archive <File to custom engine archive downloaded from extension server>
   :extender-platform <String platform the engine was compiled for>}"
  [project evaluation-context prefs platform]
  (or (dev-custom-engine prefs platform)
      (if (native-extensions/has-extensions? project evaluation-context)
        (let [build-server (native-extensions/get-build-server-url prefs)]
          (native-extensions/get-engine-archive project evaluation-context platform build-server))
        (bundled-engine platform))))

(defn current-platform []
  (.getPair (Platform/getHostPlatform)))

(defn- unpack-dmengine!
  [^File engine-archive entry-name ^File engine-file]
  (with-open [zip-file (ZipFile. engine-archive)]
    (let [dmengine-entry (.getEntry zip-file entry-name)
          stream (.getInputStream zip-file dmengine-entry)]
      (io/copy stream engine-file)
      (fs/set-executable! engine-file)
      engine-file)))

(def ^:private dmengine-dependencies
  {"x86_64-win32" #{"OpenAL32.dll" "wrap_oal.dll"}
   "x86-win32"    #{"OpenAL32.dll" "wrap_oal.dll"}})

(defn- copy-dmengine-dependencies!
  [unpack-dir extender-platform]
  (let [bundled-engine-dir (io/file (system/defold-unpack-path) extender-platform "bin")]
    (doseq [dep (dmengine-dependencies extender-platform)]
      (fs/copy! (io/file bundled-engine-dir dep) (io/file unpack-dir dep)))))

(defn- engine-install-path ^File [^File project-directory engine-descriptor]
  (let [unpack-dir (io/file project-directory "build" (:extender-platform engine-descriptor))]
    (io/file unpack-dir (dmengine-filename (current-platform)))))

(defn copy-engine-executable! ^File [^File target-dmengine platform {:keys [^File dmengine ^File engine-archive] :as engine-descriptor}]
  (assert (or dmengine engine-archive))
  (cond
    (some? dmengine)
    (fs/copy-file! dmengine target-dmengine)

    (some? engine-archive)
    (unpack-dmengine! engine-archive (dmengine-filename platform) target-dmengine))
  target-dmengine)

(defn install-engine! ^File [^File project-directory {:keys [^File dmengine ^File engine-archive extender-platform] :as engine-descriptor}]
  (assert (or dmengine engine-archive))
  (cond
    (some? dmengine)
    dmengine

    (some? engine-archive)
    (let [engine-file (engine-install-path project-directory engine-descriptor)
          engine-dir (.getParentFile engine-file)]
      (fs/delete-directory! engine-dir {:missing :ignore})
      (fs/create-directories! engine-dir)
      (unpack-dmengine! engine-archive (dmengine-filename (current-platform)) engine-file)
      (copy-dmengine-dependencies! engine-dir extender-platform)
      engine-file)))

(defn launch! [^File engine project-directory prefs debug?]
  (let [defold-log-dir (some-> (System/getProperty "defold.log.dir")
                               (File.)
                               (.getAbsolutePath))
        command (.getAbsolutePath engine)
        args (cond-> []
               defold-log-dir
               (into ["--config=project.write_log=1"
                      (format "--config=project.log_dir=%s" defold-log-dir)])

               debug?
               (into ["--config=bootstrap.debug_init_script=/_defold/debugger/start.luac"]))
        env {"DM_SERVICE_PORT" "dynamic"
             "DM_QUIT_ON_ESC" (if (prefs/get-prefs prefs "general-quit-on-esc" false)
                                "1" "0")}
        opts {:directory project-directory
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
       :name (.getName engine)
       :log-stream is})))
