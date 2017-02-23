(ns editor.system
  (:require [clojure.java.io :as io]))

(set! *warn-on-reflection* true)

(defn os-name
  ^String []
  (System/getProperty "os.name"))

(defn os-arch
  ^String []
  (System/getProperty "os.arch"))

(defn os-version
  ^String []
  (System/getProperty "os.version"))

(defn defold-version
  ^String []
  (System/getProperty "defold.version"))

(defn defold-sha1
  ^String []
  (System/getProperty "defold.sha1"))

(defn defold-build-time
  ^String []
  (System/getProperty "defold.buildtime"))

(defn defold-dev? []
  (not (defold-version)))

(defn java-home
  ^String []
  (System/getProperty "java.home"))

(defn user-home
  ^String []
  (System/getProperty "user.home"))

(defn java-runtime-version
  ^String []
  (System/getProperty "java.runtime.version"))

(defonce mac? (-> (os-name)
                  (.toLowerCase)
                  (.indexOf "mac")
                  (>= 0)))

(defonce fake-it-til-you-make-it? (.exists (io/file (user-home) ".defold" ".demo")))
