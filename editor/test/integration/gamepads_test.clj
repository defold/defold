;; Copyright 2020-2026 The Defold Foundation
;; Copyright 2014-2020 King
;; Copyright 2009-2014 Ragnar Svensson, Christian Murray
;; Licensed under the Defold License version 1.0 (the "License"); you may not use
;; this file except in compliance with the License.
;;
;; You may obtain a copy of the License, together with FAQs at
;; https://www.defold.com/license
;;
;; Unless required by applicable law or agreed to in writing, software distributed
;; under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
;; CONDITIONS OF ANY KIND, either express or implied. See the License for the
;; specific language governing permissions and limitations under the License.

(ns integration.gamepads-test
  (:require [clojure.java.io :as io]
            [clojure.string :as string]
            [clojure.test :refer :all]
            [editor.workspace :as workspace]
            [integration.test-util :as test-util]
            [support.test-support :refer [with-clean-system]])
  (:import [com.dynamo.bob Platform]
           [com.dynamo.input.proto Input$GamepadMapRuntime Input$GamepadMapsRuntime]
           [java.io File]))

(set! *warn-on-reflection* true)

(defn- host-platform-case []
  (let [platform (.getPair (Platform/getHostPlatform))]
    (cond
      (string/ends-with? platform "-macos")
      {:gamepad-db-platform "Mac OS X"
       :gamepad-db-device "Editor SDL Mac Pad"
       :ignored-gamepad-db-platform "Linux"
       :ignored-gamepad-db-device "Editor SDL Linux Pad"
       :default-gamepads-device "PLAYSTATION(R)3 Controller"
       :ignored-default-gamepads-device "Microsoft X-Box 360 pad"}

      (string/ends-with? platform "-linux")
      {:gamepad-db-platform "Linux"
       :gamepad-db-device "Editor SDL Linux Pad"
       :ignored-gamepad-db-platform "Mac OS X"
       :ignored-gamepad-db-device "Editor SDL Mac Pad"
       :default-gamepads-device "Microsoft X-Box 360 pad"
       :ignored-default-gamepads-device "PLAYSTATION(R)3 Controller"}

      (string/ends-with? platform "-win32")
      {:gamepad-db-platform "Windows"
       :gamepad-db-device "Editor SDL Windows Pad"
       :ignored-gamepad-db-platform "Linux"
       :ignored-gamepad-db-device "Editor SDL Linux Pad"
       :default-gamepads-device "cp"
       :ignored-default-gamepads-device "Microsoft X-Box 360 pad"}

      :else
      (throw (ex-info (str "Unsupported test platform: " platform)
                      {:platform platform})))))

(defn- gamepad-db-content [{:keys [gamepad-db-platform gamepad-db-device ignored-gamepad-db-platform ignored-gamepad-db-device]}]
  (format (str "03000000000000000000000000000001,%s,a:b0,platform:%s,\n"
               "03000000000000000000000000000002,%s,a:b0,platform:%s,\n")
          gamepad-db-device gamepad-db-platform
          ignored-gamepad-db-device ignored-gamepad-db-platform))

(defn- write-gamepad-db! [^File project-directory platform-case]
  (let [input-directory (io/file project-directory "input")
        gamepad-db-file (io/file input-directory "gamecontrollerdb.txt")]
    (.mkdirs input-directory)
    (spit gamepad-db-file (gamepad-db-content platform-case))))

(defn- write-default-gamepads! [^File project-directory]
  (let [input-directory (io/file project-directory "input")
        default-gamepads-file (io/file input-directory "default.gamepads")]
    (.mkdirs input-directory)
    (spit default-gamepads-file (slurp (io/file "test/resources/test_project/input/default.gamepads")))))

(defn- add-gamepad-settings! [^File project-directory gamepad-database-path]
  (let [game-project-file (io/file project-directory "game.project")
        game-project-content (slurp game-project-file)]
    (spit game-project-file
          (string/replace game-project-content
                          "game_binding = /input/game.input_bindingc"
                          (str "game_binding = /input/game.input_bindingc\n"
                               "gamepads = /input/default.gamepadsc\n"
                               "gamepad_database = " gamepad-database-path)))))

(defn- mapping-by-device
  ^Input$GamepadMapRuntime [^Input$GamepadMapsRuntime maps device]
  (some (fn [^Input$GamepadMapRuntime mapping]
          (when (= device (.getDevice mapping))
            mapping))
        (.getMappingsList maps)))

(defn- read-built-gamepad-maps
  ^Input$GamepadMapsRuntime [workspace]
  (with-open [input-stream (io/input-stream (io/file (workspace/build-path workspace) "input/default.gamepadsc"))]
    (Input$GamepadMapsRuntime/parseFrom input-stream)))

(deftest game-project-builds-gamepadsc-from-gamepads-and-gamecontrollerdb
  (with-clean-system
    (let [platform-case (host-platform-case)
          workspace (test-util/setup-scratch-workspace! world "test/resources/build_project/SideScroller")
          project-directory (workspace/project-directory workspace)]
      (write-default-gamepads! project-directory)
      (write-gamepad-db! project-directory platform-case)
      (add-gamepad-settings! project-directory "/input/gamecontrollerdb.txt")
      (workspace/resource-sync! workspace)
      (let [project (test-util/setup-project! workspace)
            game-project (test-util/resource-node project "/game.project")]
        (with-open [_ (test-util/build! game-project)]
          (let [maps (read-built-gamepad-maps workspace)
                default-mapping (mapping-by-device maps (:default-gamepads-device platform-case))
                gamepad-db-mapping (mapping-by-device maps (:gamepad-db-device platform-case))]
            (is (not (.exists (io/file (workspace/build-path workspace) "input/gamecontrollerdb.txt"))))
            (is (some? default-mapping))
            (is (not (.hasGuid default-mapping)))
            (is (some? gamepad-db-mapping))
            (is (.hasGuid gamepad-db-mapping))
            (is (nil? (mapping-by-device maps (:ignored-default-gamepads-device platform-case))))
            (is (nil? (mapping-by-device maps (:ignored-gamepad-db-device platform-case))))
            (is (not-any? (fn [^Input$GamepadMapRuntime mapping]
                            (zero? (.getMapCount mapping)))
                          (.getMappingsList maps)))))))))

(deftest game-project-requires-gamepad-database-txt-extension
  (with-clean-system
    (let [platform-case (host-platform-case)
          workspace (test-util/setup-scratch-workspace! world "test/resources/build_project/SideScroller")
          project-directory (workspace/project-directory workspace)
          input-directory (io/file project-directory "input")]
      (write-default-gamepads! project-directory)
      (.mkdirs input-directory)
      (spit (io/file input-directory "gamecontrollerdb.csv") (gamepad-db-content platform-case))
      (add-gamepad-settings! project-directory "/input/gamecontrollerdb.csv")
      (workspace/resource-sync! workspace)
      (let [project (test-util/setup-project! workspace)
            game-project (test-util/resource-node project "/game.project")
            build-error (test-util/build-error! game-project)]
        (is (some #{"input.gamepad_database must reference a .txt file."}
                  (keep :message (tree-seq :causes :causes build-error))))))))
