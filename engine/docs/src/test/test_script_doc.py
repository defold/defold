# Copyright 2020-2025 The Defold Foundation
# Copyright 2014-2020 King
# Copyright 2009-2014 Ragnar Svensson, Christian Murray
# Licensed under the Defold License version 1.0 (the "License"); you may not use
# this file except in compliance with the License.
#
# You may obtain a copy of the License, together with FAQs at
# https://www.defold.com/license
#
# Unless required by applicable law or agreed to in writing, software distributed
# under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
# CONDITIONS OF ANY KIND, either express or implied. See the License for the
# specific language governing permissions and limitations under the License.

import unittest
import script_doc
import script_doc_ddf_pb2

class TestParse(unittest.TestCase):

    def test_empty1(self):
        doc= """
/*

 */
"""
        elements = script_doc.parse_document(doc).elements
        self.assertEqual(0, len(elements))

    def test_empty2(self):
        doc= """
foobar
"""
        elements = script_doc.parse_document(doc).elements
        self.assertEqual(0, len(elements))

    def test_simple(self):
        doc= """
/*#
 * MY_DESC
 *
 * @name MY_NAME
 */
"""
        elements = script_doc.parse_document(doc).elements
        self.assertEqual(1, len(elements))
        self.assertEqual(u'MY_DESC', elements[0].brief)
        self.assertEqual(u'MY_DESC', elements[0].description)
        self.assertEqual('MY_NAME', elements[0].name)
        self.assertEqual(script_doc_ddf_pb2.FUNCTION, elements[0].type)

    def test_simple_brief(self):
        doc= """
/*# MY_BRIEF
 *
 * @name MY_NAME
 */
"""
        elements = script_doc.parse_document(doc).elements
        self.assertEqual(1, len(elements))
        self.assertEqual(u'MY_BRIEF', elements[0].brief)
        self.assertEqual(u'MY_BRIEF', elements[0].description)
        self.assertEqual('MY_NAME', elements[0].name)
        self.assertEqual(script_doc_ddf_pb2.FUNCTION, elements[0].type)

    def test_simple_variable(self):
        doc= """
/*#
 * MY_DESC
 * @name MY_NAME
 * @variable
 */
"""
        elements = script_doc.parse_document(doc).elements
        self.assertEqual(1, len(elements))
        self.assertEqual(u'MY_DESC', elements[0].description)
        self.assertEqual('MY_NAME', elements[0].name)
        self.assertEqual('MY_NAME', elements[0].name)
        self.assertEqual(script_doc_ddf_pb2.VARIABLE, elements[0].type)

    def test_description_markdown(self):
        doc= """
/*#
 * _EMPHASIS_
 *
 * - MY_DESC
 * - MY_DESC
 * @name MY_NAME
 */
"""
        elements = script_doc.parse_document(doc).elements
        self.assertEqual(1, len(elements))
        self.assertEqual(u'<em>EMPHASIS</em>\n<ul>\n<li>MY_DESC</li>\n<li>MY_DESC</li>\n</ul>', elements[0].description)
        self.assertEqual('MY_NAME', elements[0].name)

    def test_multiple(self):
        doc= """
/*#
 * MY_DESC1
 * @name MY_NAME1
 */

/*#
 * MY_DESC2
 * @name MY_NAME2
 */

"""
        elements = script_doc.parse_document(doc).elements
        self.assertEqual(2, len(elements))
        self.assertEqual(u'MY_DESC1', elements[0].description)
        self.assertEqual('MY_NAME1', elements[0].name)
        self.assertEqual(u'MY_DESC2', elements[1].description)
        self.assertEqual('MY_NAME2', elements[1].name)

    def test_param(self):
        doc= """
/*#
 * MY_DESC
 * @name MY_NAME
 * @param param_x DOC X
 * @param param_y DOC Y
 * @return what_a DOC ZX
 * @return what_b DOC ZY
 */
"""
        elements = script_doc.parse_document(doc).elements
        self.assertEqual(1, len(elements))
        self.assertEqual(u'MY_DESC', elements[0].description)
        self.assertEqual('MY_NAME', elements[0].name)
        self.assertEqual(2, len(elements[0].parameters))
        p1 = elements[0].parameters[0]
        p2 = elements[0].parameters[1]
        self.assertEqual('param_x', p1.name)
        self.assertEqual(u'DOC X', p1.doc)
        self.assertEqual('param_y', p2.name)
        self.assertEqual(u'DOC Y', p2.doc)
        self.assertEqual(2, len(elements[0].returnvalues))
        r1 = elements[0].returnvalues[0]
        r2 = elements[0].returnvalues[1]
        self.assertEqual('what_a', r1.name)
        self.assertEqual(u'DOC ZX', r1.doc)
        self.assertEqual('what_b', r2.name)
        self.assertEqual(u'DOC ZY', r2.doc)

    def test_types_optional(self):
        doc= """
/*#
 * MY_DESC
 * @name MY_NAME
 * @param param_x [type: type_a] DOCX
 * @param [param_y] [type:type_b] DOCY
 * @param [param_z] [type:*type_c*] DOCZ
 * @param param_w [type:type_d|type_e] DOCW
 */
"""
        elements = script_doc.parse_document(doc).elements
        self.assertEqual(1, len(elements))
        self.assertEqual(u'MY_DESC', elements[0].description)
        self.assertEqual('MY_NAME', elements[0].name)
        self.assertEqual(4, len(elements[0].parameters))
        p1 = elements[0].parameters[0]
        p2 = elements[0].parameters[1]
        p3 = elements[0].parameters[2]
        p4 = elements[0].parameters[3]
        self.assertEqual('param_x', p1.name)
        self.assertEqual(u'type_a', p1.types[0])
        self.assertEqual(u'DOCX', p1.doc)
        self.assertEqual('[param_y]', p2.name)
        self.assertEqual(u'type_b', p2.types[0])
        self.assertEqual(u'DOCY', p2.doc)
        self.assertEqual('[param_z]', p3.name)
        self.assertEqual(u'*type_c*', p3.types[0])
        self.assertEqual(u'DOCZ', p3.doc)
        self.assertEqual('param_w', p4.name)
        self.assertEqual(u'type_d', p4.types[0])
        self.assertEqual(u'type_e', p4.types[1])
        self.assertEqual(u'DOCW', p4.doc)


    def test_document(self):
        doc= """
/*#
 * MY_DESC
 * @name MY_DOC
 * @document
 * @namespace NS
 */
"""
        info = script_doc.parse_document(doc).info
        self.assertEqual(u'MY_DESC', info.description)
        self.assertEqual('MY_DOC', info.name)
        self.assertEqual('NS', info.namespace)

    def test_message(self):
        doc= """
/*#
 * MY_DESC
 * @name MY_MESSAGE
 * @message
 * @param param_x DOC X
 */
"""
        elements = script_doc.parse_document(doc).elements
        self.assertEqual(1, len(elements))
        self.assertEqual(script_doc_ddf_pb2.MESSAGE, elements[0].type)
        self.assertEqual(u'MY_DESC', elements[0].description)
        self.assertEqual('MY_MESSAGE', elements[0].name)
        p1 = elements[0].parameters[0]
        self.assertEqual('param_x', p1.name)
        self.assertEqual(u'DOC X', p1.doc)

    def test_message2(self):
        doc= """
/*#
 * MY_DESC
 * @name MY_MESSAGE
 * @message
 */
"""
        elements = script_doc.parse_document(doc).elements
        self.assertEqual(1, len(elements))
        self.assertEqual(script_doc_ddf_pb2.MESSAGE, elements[0].type)
        self.assertEqual(u'MY_DESC', elements[0].description)
        self.assertEqual('MY_MESSAGE', elements[0].name)

    def test_examples_code(self):
        doc= """
/*#
 * MY_DESC
 * @name MY_MESSAGE
 * @message
 * @examples example:
 *
 * ```language
 * MY_EXAMPLE
 * ```
 * @param param_x DOC X
 */
"""
        elements = script_doc.parse_document(doc).elements
        self.assertEqual(1, len(elements))
        self.assertEqual(script_doc_ddf_pb2.MESSAGE, elements[0].type)
        self.assertEqual(u'MY_DESC', elements[0].description)
        self.assertEqual('MY_MESSAGE', elements[0].name)
        self.assertEqual(u'example:\n<div class="codehilite"><pre><span></span><code>MY_EXAMPLE\n</code></pre></div>', elements[0].examples)
        p1 = elements[0].parameters[0]
        self.assertEqual('param_x', p1.name)
        self.assertEqual(u'DOC X', p1.doc)

    def test_examples2(self):
        doc= """
/*#
 * MY_DESC
 * @name MY_MESSAGE
 * @message
 * @examples example:
 */
"""
        elements = script_doc.parse_document(doc).elements
        self.assertEqual(1, len(elements))
        self.assertEqual(script_doc_ddf_pb2.MESSAGE, elements[0].type)
        self.assertEqual(u'MY_DESC', elements[0].description)
        self.assertEqual('MY_MESSAGE', elements[0].name)
        self.assertEqual(u'example:', elements[0].examples)

    def test_replaces(self):
        doc= """
/*#
 * MY_DESC
 * @name MY_MESSAGE
 * @replaces MY_REPLACEMENT
 */
"""
        elements = script_doc.parse_document(doc).elements
        self.assertEqual(1, len(elements))
        self.assertEqual(u'MY_DESC', elements[0].description)
        self.assertEqual('MY_MESSAGE', elements[0].name)
        self.assertEqual(u'MY_REPLACEMENT', elements[0].replaces)


    def test_ref(self):
        doc= """
/*#
 * MY_DESC
 * @name MY_MESSAGE
 * @examples example:
 * See [ref:some.func] for example or [ref:func]
 */
"""
        elements = script_doc.parse_document(doc).elements
        self.assertEqual(1, len(elements))
        self.assertEqual(u'MY_DESC', elements[0].description)
        self.assertEqual('MY_MESSAGE', elements[0].name)
        self.assertEqual(u'example:\nSee <a href="/ref/some#some.func">some.func</a> for example or <a href="#func">func</a>', elements[0].examples)

    def test_at(self):
        doc= """
/*#
 * MY_DESC @test
 * @name MY_MESSAGE
 * @examples example:
 * MY_EXAMPLE @test
 */
"""
        elements = script_doc.parse_document(doc).elements
        self.assertEqual(1, len(elements))
        self.assertEqual(u'MY_DESC @test', elements[0].description)
        self.assertEqual('MY_MESSAGE', elements[0].name)
        self.assertEqual(u'example:\nMY_EXAMPLE @test', elements[0].examples)

if __name__ == '__main__':
    unittest.main()
