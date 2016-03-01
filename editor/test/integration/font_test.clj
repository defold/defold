(ns integration.font-test
  (:require [clojure.test :refer :all]
            [clojure.string :as s]
            [dynamo.graph :as g]
            [support.test-support :refer [with-clean-system]]
            [integration.test-util :as test-util]
            [editor.workspace :as workspace]
            [editor.font :as font]
            [editor.defold-project :as project])
  (:import [java.io File]
           [java.nio.file Files attribute.FileAttribute]
           [org.apache.commons.io FilenameUtils FileUtils]))

(defn- prop [node-id label]
  (get-in (g/node-value node-id :_properties) [:properties label :value]))

(defn- prop! [node-id label val]
  (g/transact (g/set-property node-id label val)))

(deftest load-material-render-data
  (with-clean-system
    (let [workspace (test-util/setup-workspace! world)
          project   (test-util/setup-project! workspace)
          node-id   (test-util/resource-node project "/fonts/score.font")
          scene (g/node-value node-id :scene)]
      (is (not (nil? scene))))))

(deftest text-measure
  (with-clean-system
    (let [workspace (test-util/setup-workspace! world)
          project   (test-util/setup-project! workspace)
          node-id   (test-util/resource-node project "/fonts/score.font")
          font-map (g/node-value node-id :font-map)]
      (let [[w h] (font/measure font-map "test")]
        (is (> w 0))
        (is (> h 0))
        (let [[w' h'] (font/measure font-map "test\ntest")]
          (is (= w' w))
          (is (> h' h))
          (let [[w'' h''] (font/measure font-map "test test test" true w)]
          (is (= w'' w'))
          (is (> h'' h'))))))))

(deftest preview-text
  (with-clean-system
    (let [workspace (test-util/setup-workspace! world)
          project   (test-util/setup-project! workspace)
          node-id   (test-util/resource-node project "/fonts/score.font")
          font-map (g/node-value node-id :font-map)
          pre-text (g/node-value node-id :preview-text)
          no-break (s/replace pre-text " " "")
          [w h] (font/measure font-map pre-text true (:cache-width font-map))
          [ew eh] (font/measure font-map no-break true (:cache-width font-map))]
      (is (.contains pre-text " "))
      (is (not (.contains no-break " ")))
      (is (< w ew))
      (is (< eh h)))))
