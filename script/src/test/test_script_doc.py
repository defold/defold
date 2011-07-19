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

    def test_missing_name(self):
        doc= """
/*#

 */
"""
        elements = script_doc.parse_document(doc).elements
        self.assertEquals(0, len(elements))

    def test_simple(self):
        doc= """
/*# MY_DESC
 * @name MY_NAME
 */
"""
        elements = script_doc.parse_document(doc).elements
        self.assertEquals(1, len(elements))
        self.assertEqual('MY_DESC', elements[0].description)
        self.assertEqual('MY_NAME', elements[0].name)
        self.assertEqual('MY_NAME', elements[0].name)
        self.assertEqual(script_doc_ddf_pb2.FUNCTION, elements[0].type)

    def test_simple_variable(self):
        doc= """
/*# MY_DESC
 * @name MY_NAME
 * @variable
 */
"""
        elements = script_doc.parse_document(doc).elements
        self.assertEquals(1, len(elements))
        self.assertEqual('MY_DESC', elements[0].description)
        self.assertEqual('MY_NAME', elements[0].name)
        self.assertEqual('MY_NAME', elements[0].name)
        self.assertEqual(script_doc_ddf_pb2.VARIABLE, elements[0].type)

    def test_description(self):
        doc= """
/*#
 * MY_DESC
 * @name MY_NAME
 */
"""
        elements = script_doc.parse_document(doc).elements
        self.assertEquals(1, len(elements))
        self.assertEqual('MY_DESC', elements[0].description)
        self.assertEqual('MY_NAME', elements[0].name)

    def test_multiple(self):
        doc= """
/*# MY_DESC1
 * @name MY_NAME1
 */

/*# MY_DESC2
 * @name MY_NAME2
 */

"""
        elements = script_doc.parse_document(doc).elements
        self.assertEquals(2, len(elements))
        self.assertEqual('MY_DESC1', elements[0].description)
        self.assertEqual('MY_NAME1', elements[0].name)
        self.assertEqual('MY_DESC2', elements[1].description)
        self.assertEqual('MY_NAME2', elements[1].name)

    def test_param(self):
        doc= """
/*# MY_DESC
 * @name MY_NAME
 * @param param_x DOC X
 * @param param_y DOC Y
 * @return 123
 */
"""
        elements = script_doc.parse_document(doc).elements
        self.assertEquals(1, len(elements))
        self.assertEqual('MY_DESC', elements[0].description)
        self.assertEqual('MY_NAME', elements[0].name)
        self.assertEqual('123', elements[0].return_)
        self.assertEqual(2, len(elements[0].parameters))
        p1 = elements[0].parameters[0]
        p2 = elements[0].parameters[1]
        self.assertEqual('param_x', p1.name)
        self.assertEqual('DOC X', p1.doc)
        self.assertEqual('param_y', p2.name)
        self.assertEqual('DOC Y', p2.doc)

    def test_optional(self):
        doc= """
/*# MY_DESC
 * @name MY_NAME
 * @param param_x DOCX
 * @param [param_y] DOCY
 */
"""
        elements = script_doc.parse_document(doc).elements
        self.assertEquals(1, len(elements))
        self.assertEqual('MY_DESC', elements[0].description)
        self.assertEqual('MY_NAME', elements[0].name)
        self.assertEqual(2, len(elements[0].parameters))
        p1 = elements[0].parameters[0]
        p2 = elements[0].parameters[1]
        self.assertEqual('param_x', p1.name)
        self.assertEqual('DOCX', p1.doc)
        self.assertEqual('[param_y]', p2.name)
        self.assertEqual('DOCY', p2.doc)

    def test_bug1(self):
        doc = """
    /*#
     * Create a new URL. The function has two general forms.
     * Either you pass a single argument containing the URI to be parsed as text
     * or you pass individual parts of the URI. The two forms are illustrated in the following examples:
     * <br><br>
     * msg.url("socket:/path#fragment) <br>
     * msg.url("socket", "/path", "fragment") <br>
     * In the second form you could leave out arguments, eg: <br><br>
     * msg.url("socket", "/path")
     *
     * @name msg.url
     * @param [uristring] uri string to create uri from
     * @param [socket] socket name from the URI
     * @param [path] path of the URI
     * @param [fragment] fragment of the URI
     * @return a new URI
     */
"""
        elements = script_doc.parse_document(doc).elements
        print elements


if __name__ == '__main__':
    unittest.main()
