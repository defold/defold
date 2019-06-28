(ns compile
  (:require [clojure.java.io :as io]
            [clojure.java.shell :as shell])
  (:import [java.io File]))

(defn -main
  [& args]
  (let [java-src (filter #(.isFile ^File %) (file-seq (io/file "src/java")))
        [javac-name pathsep] (if (re-matches #"^.*Windows.*$" (System/getProperty "os.name"))
                               ["javac.exe" \\]
                               ["javac" \/])
        javac (str (System/getenv "JAVA_HOME") pathsep "bin" pathsep javac-name)
        cp (System/getProperty "java.class.path")]
    (spit "java-compile-commands.txt"
          (with-out-str
            (println (str "-cp " cp))
            (doseq [^File file java-src]
              (println (.getCanonicalPath file)))
            (println (str "-d target" pathsep "classes"))))
    (println (str "Compiling " (count java-src) " java source files."))
    (println (shell/sh javac "@java-compile-commands.txt")))
  (shutdown-agents))

