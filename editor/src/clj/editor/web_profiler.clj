(ns editor.web-profiler
  (:require [editor.handler :as handler]
            [editor.ui :as ui]
            [editor.fs :as fs]
            [util.http-server :as http-server]
            [clojure.java.io :as io])
  (:import  [com.defold.util Profiler]
            [java.io File]
            [java.net URI]))

(def ^:private template-path "profiler_template.html")
(def ^:private ^String target-path "tmp/profiler.html")

(defn- dump-profiler []
  (let [data (Profiler/dumpJson)
        html (-> (slurp (io/resource template-path))
                 (clojure.string/replace "$PROFILER_DATA" data))]
    (fs/create-file! (File. target-path) html)))

(handler/defhandler :profile :global
  (enabled? [] true)
  (run [] (dump-profiler)))

(handler/defhandler :profile-show :global
  (enabled? [] true)
  (run [web-server]
       (dump-profiler)
       (ui/open-url (format "%s/profiler" (http-server/local-url web-server)))))

(defn handler [req]
  {:code 200
   :headers {"Content-Type" "text/html"}
   :body (try
           (slurp target-path)
           (catch Throwable e
             (dump-profiler)
             (slurp target-path)))})
