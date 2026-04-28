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

(ns editor.render-program-utils-test
  (:require [clojure.test :refer :all]
            [editor.protobuf :as protobuf]
            [editor.render-program-utils :as render-program-utils]))

(deftest inverse-constant-types-are-treated-as-non-editable
  (let [constant {:name "view_proj_inv" :type :constant-type-viewproj-inv}
        editable (first (render-program-utils/constants->editable-constants [constant]))
        saved (first (render-program-utils/editable-constants->constants [editable]))]
    (is (= protobuf/vector4-zero (:value editable)))
    (is (= constant saved))
    (is (= constant (render-program-utils/sanitize-constant (assoc constant :value [protobuf/vector4-zero]))))))
