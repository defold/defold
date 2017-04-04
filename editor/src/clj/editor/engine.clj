(ns editor.engine
  (:require [clojure.java.io :as io]
            [dynamo.graph :as g]
            [editor.prefs :as prefs]
            [editor.dialogs :as dialogs]
            [editor.error-reporting :as error-reporting]
            [editor.protobuf :as protobuf]
            [editor.resource :as resource]
            [editor.console :as console]
            [editor.ui :as ui]
            [editor.targets :as targets]
            [editor.defold-project :as project]
            [editor.workspace :as workspace]
            [editor.engine.native-extensions :as native-extensions])
  (:import [com.defold.editor Platform]
           [java.net HttpURLConnection InetSocketAddress Socket URI URL]
           [java.io BufferedReader File InputStream IOException]
           [java.lang Process ProcessBuilder]
           [org.apache.commons.io IOUtils FileUtils]))

(set! *warn-on-reflection* true)

(def ^:const timeout 2000)
(defonce engine-input-stream (atom nil))

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

(defn- swap-engine-input-stream! [new-input-stream]
  (swap! engine-input-stream (fn [^InputStream is new-is]
                               (when is (.close is))
                               new-is)
    new-input-stream))

(defn- pump-output [^InputStream is]
  (swap-engine-input-stream! is)
  (let [buf (byte-array 1024)]
    (try
      (loop []
        (let [n (.read is buf)]
          (when (> n -1)
            (let [msg (String. buf 0 n)]
              (console/append-console-message! msg)
              (recur)))))
      (catch IOException _
        ;; Losing the log connection is ok and even expected
        nil))))

(defn reboot [target local-url]
  (let [url  (URL. (format "%s/post/@system/reboot" (:url target)))
        conn ^HttpURLConnection (get-connection url)]
    (try
      (swap-engine-input-stream! nil)
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
        (.start (Thread. (fn []
                           ;; We try our best to connect to logging, fail without retry if we couldn't
                           (try
                             (let [port (Integer/parseInt (:log-port target))
                                   socket-addr (InetSocketAddress. ^String (:address target) port)]
                               (with-open [socket (doto (Socket.)
                                                    (.setSoTimeout timeout)
                                                    (.connect socket-addr timeout))]
                                 (let [is (.getInputStream socket)
                                       status (-> ^BufferedReader (io/reader is)
                                                (.readLine))]
                                   ;; The '0 OK' string is part of the log service protocol
                                   (if (= "0 OK" status)
                                     (do
                                       (console/append-console-message! (format "[Running on target '%s' (%s)]\n" (:name target) (:address target)))
                                       ;; Setting to 0 means wait indefinitely for new data
                                       (.setSoTimeout socket 0)
                                       (pump-output is))))))
                             (catch Exception e
                               (error-reporting/report-exception! e))))))
        :ok)
      (catch Exception e
        (.disconnect conn)
        false))))

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

(defn- bundled-engine [platform]
  (let [suffix (.getExeSuffix (Platform/getHostPlatform))
        path   (format "%s/%s/bin/dmengine%s" (System/getProperty "defold.unpack.path") platform suffix)]
    (io/file path)))

(defn get-engine [project prefs platform]
  (if-let [native-extension-roots (and (prefs/get-prefs prefs "enable-extensions" false)
                                       (native-extensions/extension-roots project))]
    (let [build-server (prefs/get-prefs prefs "extensions-server" native-extensions/defold-build-server-url)]
      (native-extensions/get-engine project native-extension-roots platform build-server))
    (bundled-engine platform)))

(defn launch [project prefs]
  (let [launch-dir   (io/file (workspace/project-path (project/workspace project)))
        ^File engine (get-engine project prefs (.getPair (Platform/getJavaPlatform)))]
    (do-launch (.getAbsolutePath engine) launch-dir prefs)))
