;; Copyright 2020-2022 The Defold Foundation
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

(ns editor.connection-properties)

(def ^:const connection-properties
  {:sentry {:project-id "97739"
            :key "9e25fea9bc334227b588829dd60265c1"
            :secret "f694ef98d47d42cf8bb67ef18a4e9cdb"}
   :google-analytics {:tid "UA-83690-7"}
   :git-issues {:url "https://github.com/defold/defold"}
   :native-extensions {:build-server-url "https://build.defold.com"}
   :updater {:download-url-template "https://%s/archive/%s/%s/editor2/Defold-%s.zip"
             :update-url-template "https://%s/editor2/channels/%s/update-v4.json"}})
