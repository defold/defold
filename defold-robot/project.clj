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

(defproject defold-robot "0.7.0"
  :dependencies [[org.clojure/clojure "1.10.0"]
                 [org.clojure/tools.cli "0.3.5"]
                 [commons-io/commons-io "2.4"]
                 [org.apache.ant/ant "1.10.1"]
                 [org.openjfx/javafx-graphics "12"]
                 [org.openjfx/javafx-graphics "12" :classifier "linux"]
                 [org.openjfx/javafx-graphics "12" :classifier "mac"]
                 [org.openjfx/javafx-graphics "12" :classifier "win"]
                 [org.openjfx/javafx-swing "12"]
                 [org.openjfx/javafx-swing "12" :classifier "linux"]
                 [org.openjfx/javafx-swing "12" :classifier "mac"]
                 [org.openjfx/javafx-swing "12" :classifier "win"]]
  :resource-paths ["resources"]
  :main defold-robot.robot
  :aot [defold-robot.robot]
  :aliases {"tar-install" ["do" "uberjar" ["run" "-m" "lein-plugins.tar-install" :project/name :project/version :project/target-path]]}
  :release-tasks [["do" "tar-install"]])
