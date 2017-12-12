(ns editor.luajit
  (:require
   [clojure.java.io :as io]
   [clojure.java.shell :as shell]
   [clojure.string :as str]
   [editor.fs :as fs]
   [editor.system :as system])
  (:import
   (java.io File ByteArrayOutputStream)
   (com.defold.editor Platform)))

(set! *warn-on-reflection* true)

(defn- parse-compilation-error
  [s]
  (-> (zipmap [:exec :filename :line :message] (map str/trim (str/split s #":")))
      (update :line #(try (Integer/parseInt %) (catch Exception _)))))


(defn- luajit-exec-path
  []
  (str (system/defold-unpack-path) "/" (.getPair (Platform/getJavaPlatform)) "/bin/luajit"))

(defn- luajit-lua-path
  []
  (str (system/defold-unpack-path) "/shared/luajit"))

(defn- compile-file
  [proj-path ^File input ^File output]
  (let [{:keys [exit out err]} (shell/sh (luajit-exec-path)
                                         "-bgf"
                                         (str "=" proj-path)
                                         (.getAbsolutePath input)
                                         (.getAbsolutePath output)
                                         :env {"LUA_PATH" (str (luajit-lua-path) "/?.lua")})]
    (if-not (zero? exit)
      (let [{:keys [filename line message]} (parse-compilation-error err)]
        (throw (ex-info (format "Compilation failed: %s" message)
                        {:exit exit
                         :out out
                         :err err
                         :filename filename
                         :line line
                         :message message}))))))

(defn bytecode
  [source proj-path]
  (let [input (fs/create-temp-file! "script" ".lua")
        output (fs/create-temp-file! "script" ".luajitbc")]
    (try
      (io/copy source input)
      (compile-file proj-path input output)
      (with-open [buf (ByteArrayOutputStream.)]
        (io/copy output buf)
        (.toByteArray buf))
      (finally
        (fs/delete-file! input {:fail :silently})
        (fs/delete-file! output {:fail :silently})))))
