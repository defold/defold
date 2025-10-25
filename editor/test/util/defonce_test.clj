;; Copyright 2020-2025 The Defold Foundation
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

(ns util.defonce-test
  (:require [clojure.test :refer :all]
            [util.defonce :as defonce]))

;; -----------------------------------------------------------------------------
;; defonce/protocol
;; -----------------------------------------------------------------------------

(defonce/protocol Protocol "Original Protocol"
  (protocol-method [this] "Original Method"))

(defonce ^:private original-protocol-defs [Protocol])

(defonce/protocol Protocol "Redefined Protocol"
  (protocol-method [this] "Redefined Method"))

(deftest protocol-test
  (is (= original-protocol-defs [Protocol]))
  (is (= "Original Protocol" (:doc (meta #'Protocol))))
  (is (= "Original Method" (:doc (meta #'protocol-method)))))

;; -----------------------------------------------------------------------------
;; defonce/record
;; -----------------------------------------------------------------------------

(defonce/record Record [original]
  Protocol
  (protocol-method [_this] "Original Result"))

(defonce ^:private original-record-defs [Record ->Record map->Record])

(defonce/record Record [redefined]
  Protocol
  (protocol-method [_this] "Redefined Result"))

(deftest record-test
  (is (= original-record-defs [Record ->Record map->Record]))
  (is (= "Original Value" (.original (->Record "Original Value"))))
  (is (= "Original Result" (protocol-method (->Record "Original Value")))))

;; -----------------------------------------------------------------------------
;; defonce/type
;; -----------------------------------------------------------------------------

(defonce/type Type [original]
  Protocol
  (protocol-method [_this] "Original Result"))

(defonce ^:private original-type-defs [Type ->Type])

(defonce/type Type [redefined]
  Protocol
  (protocol-method [_this] "Redefined Result"))

(deftest type-test
  (is (= original-type-defs [Type ->Type]))
  (is (= "Original Value" (.original (->Type "Original Value"))))
  (is (= "Original Result" (protocol-method (->Type "Original Value")))))

;; -----------------------------------------------------------------------------
;; defonce/interface
;; -----------------------------------------------------------------------------

(defonce/interface Interface
  (interfaceMethod []))

(defonce ^:private original-interface-defs [Interface])

(defonce/interface Interface
  (interfaceMethod []))

(deftest interface-test
  (= (identical? original-interface-defs [Interface])))