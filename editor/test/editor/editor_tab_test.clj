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

(ns editor.editor-tab-test
  (:require [clojure.test :refer :all]
            [editor.editor-tab :as editor-tab]
            [util.fn :as fn]))

(deftest register-type-test
  (testing "registration and lookup"
    (editor-tab/register-type! ::registered {:make-tab-spec-fn fn/constantly-nil})
    (is (= {:make-tab-spec-fn fn/constantly-nil}
           (editor-tab/resolve-type ::registered)))
    (editor-tab/unregister-type! ::registered))

  (testing "re-registration replaces the descriptor"
    (editor-tab/register-type! ::replaced {:make-tab-spec-fn fn/constantly-nil})
    (editor-tab/register-type! ::replaced {:make-tab-spec-fn fn/constantly-false})
    (is (= {:make-tab-spec-fn fn/constantly-false}
           (editor-tab/resolve-type ::replaced)))
    (editor-tab/unregister-type! ::replaced))

  (testing "unregistration"
    (editor-tab/register-type! ::unregistered {:make-tab-spec-fn fn/constantly-nil})
    (editor-tab/unregister-type! ::unregistered)
    (is (nil? (editor-tab/resolve-type ::unregistered))))

  (testing "unqualified type ids are rejected"
    (is (thrown? AssertionError
                 (editor-tab/register-type! :unqualified {:make-tab-spec-fn fn/constantly-nil})))
    (is (nil? (editor-tab/resolve-type :unqualified)))))
