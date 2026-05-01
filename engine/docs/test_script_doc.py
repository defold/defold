# Copyright 2020-2026 The Defold Foundation
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
        doc_msg = script_doc.parse_document(doc)
        doc_dict = script_doc.message_to_json_dict(doc_msg)
        elements = doc_dict["elements"]
        self.assertEqual(1, len(elements))
        self.assertEqual(u'<em>EMPHASIS</em>\n<ul>\n<li>MY_DESC</li>\n<li>MY_DESC</li>\n</ul>', elements[0].get("description"))

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
 * @param param_x [type:string] DOC X
 * @param param_y [type:number|boolean|nil] DOC Y
 * @param [param_z] [type:string] DOC Y
 */
"""
        elements = script_doc.parse_document(doc).elements
        self.assertEqual(1, len(elements))
        self.assertEqual(u'MY_DESC', elements[0].description)
        self.assertEqual('MY_NAME', elements[0].name)
        self.assertEqual(3, len(elements[0].parameters))
        p1 = elements[0].parameters[0]
        self.assertEqual('param_x', p1.name)
        self.assertEqual(u'DOC X', p1.doc)
        self.assertEqual('string', p1.types[0])
        p2 = elements[0].parameters[1]
        self.assertEqual('param_y', p2.name)
        self.assertEqual(u'DOC Y', p2.doc)
        self.assertEqual('number', p2.types[0])
        self.assertEqual('boolean', p2.types[1])
        self.assertEqual('nil', p2.types[2])
        p3 = elements[0].parameters[2]
        self.assertEqual(True, p3.is_optional)

    def test_return(self):
        doc= """
/*#
 * MY_DESC
 * @name MY_NAME
 * @return what_a [type:string|boolean] DOC ZX
 * @return what_b [type:number|nil] DOC ZY
 */
"""
        elements = script_doc.parse_document(doc).elements
        self.assertEqual(1, len(elements))
        self.assertEqual(2, len(elements[0].returnvalues))
        r1 = elements[0].returnvalues[0]
        r2 = elements[0].returnvalues[1]
        self.assertEqual('what_a', r1.name)
        self.assertEqual(u'DOC ZX', r1.doc)
        self.assertEqual('string', r1.types[0])
        self.assertEqual('boolean', r1.types[1])
        self.assertEqual('what_b', r2.name)
        self.assertEqual(u'DOC ZY', r2.doc)
        self.assertEqual('number', r2.types[0])
        self.assertEqual('nil', r2.types[1])


    def test_all_lua_types(self):
        doc= """
/*#
 * MY_DESC
 * @name MY_NAME
 * @param param_x [type:string|number|boolean|function|nil|userdata|thread|file] DOCX
 * @param param_y [type:vector|vector3|vector4|matrix4|quaternion|hash|url|node|resource|buffer] DOCY
 * @param param_z [type:constant|any] DOCZ
 */
"""
        elements = script_doc.parse_document(doc).elements
        self.assertEqual(True, True) # make sure it doesn't crash


    def test_wrong_type(self):
        doc= """
/*#
 * MY_DESC
 * @name MY_NAME
 * @param param_x [type:foobar] DOCX
 */
"""
        exception = False
        try:
            script_doc.parse_document(doc)
        except Exception:
            exception = True

        self.assertEqual(exception, True)


    def test_document(self):
        doc= """
/*#
 * MY_DESC
 * @name MY_DOC
 * @document
 * @namespace NS
 * @path path/to/mydoc
 */
"""
        info = script_doc.parse_document(doc).info
        self.assertEqual(u'MY_DESC', info.description)
        self.assertEqual('MY_DOC', info.name)
        self.assertEqual('NS', info.namespace)
        self.assertEqual('path/to/mydoc', info.path)

    def test_message(self):
        doc= """
/*#
 * MY_DESC
 * @name MY_MESSAGE
 * @message
 * @param param_x [type:string] DOC X
 * @param param_y [type:boolean] DOC Y
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
        self.assertEqual('string', p1.types[0])
        p2 = elements[0].parameters[1]
        self.assertEqual('param_y', p2.name)
        self.assertEqual(u'DOC Y', p2.doc)
        self.assertEqual('boolean', p2.types[0])

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

    def test_examples_code_markdown(self):
        doc= """
/*#
 * @name MY_MESSAGE
 * @examples example:
 *
 * ```language
 * MY_EXAMPLE
 * ```
 */
"""
        doc_msg = script_doc.parse_document(doc)
        doc_dict = script_doc.message_to_json_dict(doc_msg)
        elements = doc_dict["elements"]
        self.assertEqual(1, len(elements))
        self.assertEqual(u'example:\n<div class="codehilite"><pre><span></span><code>MY_EXAMPLE\n</code></pre></div>', elements[0].get("examples"))

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
        doc_msg = script_doc.parse_document(doc)
        doc_dict = script_doc.message_to_json_dict(doc_msg)
        elements = doc_dict["elements"]
        self.assertEqual(1, len(elements))
        self.assertEqual(u'example:\nSee <a href="/ref/some#some.func">some.func</a> for example or <a href="#func">func</a>', elements[0].get("examples"))

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
