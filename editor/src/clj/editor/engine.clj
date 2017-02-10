(ns editor.engine
  (:require [clojure.java.io :as io]
            [dynamo.graph :as g]
            [editor.prefs :as prefs]
            [editor.dialogs :as dialogs]
            [editor.protobuf :as protobuf]
            [editor.resource :as resource]
            [editor.console :as console]
            [editor.ui :as ui]
            [editor.targets :as targets]
            [editor.defold-project :as project]
            [editor.workspace :as workspace]
            [editor.engine.native-extensions :as native-extensions])
  (:import [com.defold.editor Platform]
           [java.net HttpURLConnection URI URL]
           [java.io File InputStream]
           [java.lang Process ProcessBuilder]
           [org.apache.commons.io IOUtils FileUtils]))

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
