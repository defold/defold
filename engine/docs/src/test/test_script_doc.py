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
        self.assertEquals(0, len(elements))

    def test_empty2(self):
        doc= """
foobar
"""
        elements = script_doc.parse_document(doc).elements
        self.assertEquals(0, len(elements))

    def test_simple(self):
        doc= """
/*#
 * MY_DESC
 *
 * @name MY_NAME
 */
"""
        elements = script_doc.parse_document(doc).elements
        self.assertEquals(1, len(elements))
        self.assertEqual('', elements[0].brief)
        self.assertEqual(u'<p>MY_DESC</p>', elements[0].description)
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
        self.assertEquals(1, len(elements))
        self.assertEqual(u'<p>MY_BRIEF</p>', elements[0].brief)
        self.assertEqual('', elements[0].description)
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
        self.assertEquals(1, len(elements))
        self.assertEqual(u'<p>MY_DESC</p>', elements[0].description)
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
        self.assertEquals(1, len(elements))
        self.assertEqual(u'<p><em>EMPHASIS</em></p>\n<ul>\n<li>MY_DESC</li>\n<li>MY_DESC</li>\n</ul>', elements[0].description)
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
        self.assertEquals(2, len(elements))
        self.assertEqual(u'<p>MY_DESC1</p>', elements[0].description)
        self.assertEqual('MY_NAME1', elements[0].name)
        self.assertEqual(u'<p>MY_DESC2</p>', elements[1].description)
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
        self.assertEquals(1, len(elements))
        self.assertEqual(u'<p>MY_DESC</p>', elements[0].description)
        self.assertEqual('MY_NAME', elements[0].name)
        self.assertEqual(2, len(elements[0].parameters))
        p1 = elements[0].parameters[0]
        p2 = elements[0].parameters[1]
        self.assertEqual('param_x', p1.name)
        self.assertEqual(u'<p>DOC X</p>', p1.doc)
        self.assertEqual('param_y', p2.name)
        self.assertEqual(u'<p>DOC Y</p>', p2.doc)
        self.assertEqual(2, len(elements[0].returnvalues))
        r1 = elements[0].returnvalues[0]
        r2 = elements[0].returnvalues[1]
        self.assertEqual('what_a', r1.name)
        self.assertEqual(u'<p>DOC ZX</p>', r1.doc)
        self.assertEqual('what_b', r2.name)
        self.assertEqual(u'<p>DOC ZY</p>', r2.doc)

    def test_types_optional(self):
        doc= """
/*#
 * MY_DESC
 * @name MY_NAME
 * @param param_x [type: type_a] DOCX
 * @param [param_y] [type:type_b] DOCY
 * @param [param_z] [type:*type_c*] DOCZ
 */
"""
        elements = script_doc.parse_document(doc).elements
        self.assertEquals(1, len(elements))
        self.assertEqual(u'<p>MY_DESC</p>', elements[0].description)
        self.assertEqual('MY_NAME', elements[0].name)
        self.assertEqual(3, len(elements[0].parameters))
        p1 = elements[0].parameters[0]
        p2 = elements[0].parameters[1]
        p3 = elements[0].parameters[2]
        self.assertEqual('param_x', p1.name)
        self.assertEqual(u'<p><span class="type"> type_a</span> DOCX</p>', p1.doc)
        self.assertEqual('[param_y]', p2.name)
        self.assertEqual(u'<p><span class="type">type_b</span> DOCY</p>', p2.doc)
        self.assertEqual('[param_z]', p3.name)
        self.assertEqual(u'<p><span class="type">*type_c*</span> DOCZ</p>', p3.doc)


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
        self.assertEqual(u'<p>MY_DESC</p>', info.description)
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
        self.assertEquals(1, len(elements))
        self.assertEqual(script_doc_ddf_pb2.MESSAGE, elements[0].type)
        self.assertEqual(u'<p>MY_DESC</p>', elements[0].description)
        self.assertEqual('MY_MESSAGE', elements[0].name)
        p1 = elements[0].parameters[0]
        self.assertEqual('param_x', p1.name)
        self.assertEqual(u'<p>DOC X</p>', p1.doc)

    def test_message2(self):
        doc= """
/*#
 * MY_DESC
 * @name MY_MESSAGE
 * @message
 */
"""
        elements = script_doc.parse_document(doc).elements
        self.assertEquals(1, len(elements))
        self.assertEqual(script_doc_ddf_pb2.MESSAGE, elements[0].type)
        self.assertEqual(u'<p>MY_DESC</p>', elements[0].description)
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
        self.assertEquals(1, len(elements))
        self.assertEqual(script_doc_ddf_pb2.MESSAGE, elements[0].type)
        self.assertEqual(u'<p>MY_DESC</p>', elements[0].description)
        self.assertEqual('MY_MESSAGE', elements[0].name)
        self.assertEqual(u'<p>example:</p>\n<div class="codehilite"><pre><span></span>MY_EXAMPLE\n</pre></div>', elements[0].examples)
        p1 = elements[0].parameters[0]
        self.assertEqual('param_x', p1.name)
        self.assertEqual(u'<p>DOC X</p>', p1.doc)

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
        self.assertEquals(1, len(elements))
        self.assertEqual(script_doc_ddf_pb2.MESSAGE, elements[0].type)
        self.assertEqual(u'<p>MY_DESC</p>', elements[0].description)
        self.assertEqual('MY_MESSAGE', elements[0].name)
        self.assertEqual(u'<p>example:</p>', elements[0].examples)

    def test_replaces(self):
        doc= """
/*#
 * MY_DESC
 * @name MY_MESSAGE
 * @replaces MY_REPLACEMENT
 */
"""
        elements = script_doc.parse_document(doc).elements
        self.assertEquals(1, len(elements))
        self.assertEqual(u'<p>MY_DESC</p>', elements[0].description)
        self.assertEqual('MY_MESSAGE', elements[0].name)
        self.assertEqual(u'<p>MY_REPLACEMENT</p>', elements[0].replaces)


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
        self.assertEquals(1, len(elements))
        self.assertEqual(u'<p>MY_DESC</p>', elements[0].description)
        self.assertEqual('MY_MESSAGE', elements[0].name)
        self.assertEqual(u'<p>example:\nSee <a href="/ref/some#some.func">some.func</a> for example or <a href="#func">func</a></p>', elements[0].examples)

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
        self.assertEquals(1, len(elements))
        self.assertEqual(u'<p>MY_DESC @test</p>', elements[0].description)
        self.assertEqual('MY_MESSAGE', elements[0].name)
        self.assertEqual(u'<p>example:\nMY_EXAMPLE @test</p>', elements[0].examples)

if __name__ == '__main__':
    unittest.main()
