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

(ns editor.os-test
  (:require [clojure.test :refer [deftest is]]
            [editor.os :as os]))

;; Verifies system-theme always resolves to one of the two theme keywords on
;; the running platform. Guards against a detection path returning nil or an
;; unexpected value, which would silently break theme selection.
(deftest system-theme-returns-valid-theme
  (is (contains? #{:dark :light} (os/system-theme))))

;; Verifies system-dark-mode? returns a strict boolean so callers can rely on
;; truthiness. Guards against a platform branch leaking a nil (e.g. from a
;; failed external command) or a regex match result out of the helpers.
(deftest system-dark-mode-returns-boolean
  (is (boolean? (os/system-dark-mode?))))

;; Verifies the supported-platform branches are exhaustive: os returns one of
;; :win32, :macos or :linux, and system-theme maps it onto a theme keyword.
;; Guards against adding a platform keyword without extending theme detection.
(deftest system-theme-covers-host-platform
  (is (contains? #{:win32 :macos :linux} (os/os)))
  (is (contains? #{:dark :light} (os/system-theme))))
