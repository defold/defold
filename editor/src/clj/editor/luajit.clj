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
  [arch]
  (str (system/defold-unpack-path) "/" (.getPair (Platform/getJavaPlatform)) (str "/bin/luajit-" arch)))

(defn- luajit-lua-path
  []
  (str (system/defold-unpack-path) "/shared/luajit"))

(defn luajit-path-to-chunk
  [^String s]
  (subs s (max 0 (- (count s) 59))))

(defn- compile-file
  [proj-path ^File input ^File output arch]
  (let [{:keys [exit out err]} (shell/sh (luajit-exec-path arch)
                                         "-bgf"
                                         ; Take the last 59 chars from the path as Lua chunkname.
                                         ; Prefix "=" which tells Lua this is a literal file path,
                                         ; the total length is now at maximum 60 chars (which is the
                                         ; maximum length of chunknames in Lua).
                                         (str "=" (luajit-path-to-chunk proj-path))
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
  [source proj-path arch]
  (let [input (fs/create-temp-file! "script" ".lua")
        output (fs/create-temp-file! "script" ".luajitbc")]
    (try
      (io/copy source input)
      (compile-file proj-path input output arch)
      (with-open [buf (ByteArrayOutputStream.)]
        (io/copy output buf)
        (.toByteArray buf))
      (finally
        (fs/delete-file! input {:fail :silently})
        (fs/delete-file! output {:fail :silently})))))
