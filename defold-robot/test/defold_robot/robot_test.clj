(ns defold-robot.robot-test
  (:require [clojure.test :refer :all]
            [clojure.java.io :as io]
            [defold-robot.robot :as robot])
  (:import [java.io BufferedWriter File]
           [java.nio.file Files]
           [java.nio.file.attribute FileAttribute]
           [org.apache.commons.io FileUtils]))

(set! *warn-on-reflection* true)

(defn- tmp-log ^File []
  (doto (File/createTempFile "robot" "log.txt")
    (.deleteOnExit)))

(defn- f->desc [^File f]
  {"pattern" (.getName f)
   "dir" (.getParent f)})

(defn- run [script]
  (let [output (.toFile (Files/createTempDirectory "robot-output" (into-array FileAttribute [])))]
    (try
      (robot/run script output)
      (finally
        (FileUtils/deleteDirectory output)))))

(defn- sleep [ms]
  (try
    (Thread/sleep ms)
    (catch InterruptedException _)))

(defn- write-line [^BufferedWriter w ^String s]
  (.write w s)
  (.newLine w)
  (.flush w))

(deftest await-log []
  (testing "time-out waiting for log"
    (let [log (tmp-log)]
      (is (false? (run {"logs" {"log" (f->desc log)}
                        "steps" [["await-log" "log" 10 "no output here"]]})))))
  (testing "wait for log when log was written before reading"
    (let [log (tmp-log)
          f (future
              (with-open [w (io/writer log)]
                (sleep 200)
                (write-line w "first output")
                (sleep 100)
                (write-line w "second output")))]
      (is (true? (run {"logs" {"log" (f->desc log)}
                       "steps" [["await-log" "log" 1000 "first output"]
                                ["wait" 400]
                                ["await-log" "log" 1000 "second output"]]}))))))
