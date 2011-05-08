import unittest
import script_doc

class TestParse(unittest.TestCase):

    def test_empty1(self):
        doc= """
/*

 */
"""
        functions = script_doc.parseDocument(doc).functions
        self.assertEquals(0, len(functions))

    def test_empty2(self):
        doc= """
foobar
"""
        functions = script_doc.parseDocument(doc).functions
        self.assertEquals(0, len(functions))

    def test_missing_name(self):
        doc= """
/*#

 */
"""
        functions = script_doc.parseDocument(doc).functions
        self.assertEquals(0, len(functions))

    def test_simple(self):
        doc= """
/*# MY_DESC
 * @name MY_NAME
 */
"""
        functions = script_doc.parseDocument(doc).functions
        self.assertEquals(1, len(functions))
        self.assertEqual('MY_DESC', functions[0].description)
        self.assertEqual('MY_NAME', functions[0].name)

    def test_description(self):
        doc= """
/*#
 * MY_DESC
 * @name MY_NAME
 */
"""
        functions = script_doc.parseDocument(doc).functions
        self.assertEquals(1, len(functions))
        self.assertEqual('MY_DESC', functions[0].description)
        self.assertEqual('MY_NAME', functions[0].name)

    def test_multiple(self):
        doc= """
/*# MY_DESC1
 * @name MY_NAME1
 */

/*# MY_DESC2
 * @name MY_NAME2
 */

"""
        functions = script_doc.parseDocument(doc).functions
        self.assertEquals(2, len(functions))
        self.assertEqual('MY_DESC1', functions[0].description)
        self.assertEqual('MY_NAME1', functions[0].name)
        self.assertEqual('MY_DESC2', functions[1].description)
        self.assertEqual('MY_NAME2', functions[1].name)

    def test_param(self):
        doc= """
/*# MY_DESC
 * @name MY_NAME
 * @param param_x DOC X
 * @param param_y DOC Y
 * @return 123
 */
"""
        functions = script_doc.parseDocument(doc).functions
        self.assertEquals(1, len(functions))
        self.assertEqual('MY_DESC', functions[0].description)
        self.assertEqual('MY_NAME', functions[0].name)
        self.assertEqual('123', functions[0].return_)
        self.assertEqual(2, len(functions[0].parameters))
        p1 = functions[0].parameters[0]
        p2 = functions[0].parameters[1]
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
        functions = script_doc.parseDocument(doc).functions
        self.assertEquals(1, len(functions))
        self.assertEqual('MY_DESC', functions[0].description)
        self.assertEqual('MY_NAME', functions[0].name)
        self.assertEqual(2, len(functions[0].parameters))
        p1 = functions[0].parameters[0]
        p2 = functions[0].parameters[1]
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
        functions = script_doc.parseDocument(doc).functions
        print functions


if __name__ == '__main__':
    unittest.main()
