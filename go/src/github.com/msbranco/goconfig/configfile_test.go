package goconfig_test

import (
	. "../goconfig"
	"bufio"
	"os"
	"strings"
	"testing"
)

func testGet(t *testing.T, c *ConfigFile, section string, option string, expected interface{}) {
	ok := false
	switch expected.(type) {
	case string:
		v, _ := c.GetString(section, option)
		if v == expected.(string) {
			ok = true
		}
	case int64:
		v, _ := c.GetInt64(section, option)
		if v == expected.(int64) {
			ok = true
		}
	case bool:
		v, _ := c.GetBool(section, option)
		if v == expected.(bool) {
			ok = true
		}
	default:
		t.Fatalf("Bad test case")
	}

	if !ok {
		t.Errorf("Get failure: expected different value for %s %s", section, option)
	}
}

// Create configuration representation and run multiple tests in-memory.
func TestInMemory(t *testing.T) {
	c := NewConfigFile()

	// test empty structure
	if len(c.GetSections()) != 1 { // should be empty
		t.Errorf("GetSections failure: invalid length")
	}

	if c.HasSection("no-section") { // test presence of missing section
		t.Errorf("HasSection failure: invalid section")
	}

	_, err := c.GetOptions("no-section") // get options for missing section
	if err == nil {
		t.Errorf("GetOptions failure: invalid section")
	}

	if c.HasOption("no-section", "no-option") {
		// test presence of option for missing section
		t.Errorf("HasSection failure: invalid/section/option")
	}

	// get value from missing section/option
	_, err = c.GetString("no-section", "no-option")
	if err == nil {
		t.Errorf("GetString failure: got value for missing section/option")
	}

	// get value from missing section/option
	_, err = c.GetInt64("no-section", "no-option")
	if err == nil {
		t.Errorf("GetInt failure: got value for missing section/option")
	}

	if c.RemoveSection("no-section") { // remove missing section
		t.Errorf("RemoveSection failure: removed missing section")
	}

	if c.RemoveOption("no-section", "no-option") {
		// remove missing section/option
		t.Errorf("RemoveOption failure: removed missing section/option")
	}

	// fill up structure
	if !c.AddSection("section1") { // add section
		t.Errorf("AddSection failure: false on first insert")
	}

	if c.AddSection("section1") { // re-add same section
		t.Errorf("AddSection failure: true on second insert")
	}

	if c.AddSection(DefaultSection) { // default section always exists
		t.Errorf("AddSection failure: true on default section insert")
	}

	if !c.AddOption("section1", "option1", "value1") { // add option/value
		t.Errorf("AddOption failure: false on first insert")
	}
	testGet(t, c, "section1", "option1", "value1") // read it back

	if c.AddOption("section1", "option1", "value2") { // overwrite value
		t.Errorf("AddOption failure: true on second insert")
	}
	testGet(t, c, "section1", "option1", "value2") // read it back again

	if !c.RemoveOption("section1", "option1") { // remove option/value
		t.Errorf("RemoveOption failure: false on first remove")
	}

	if c.RemoveOption("section1", "option1") { // remove again
		t.Errorf("RemoveOption failure: true on second remove")
	}

	_, err = c.GetString("section1", "option1") // read it back again
	if err == nil {
		t.Errorf("GetString failure: got value for removed section/option")
	}

	if !c.RemoveSection("section1") { // remove existing section
		t.Errorf("RemoveSection failure: false on first remove")
	}

	if c.RemoveSection("section1") { // remove again
		t.Errorf("RemoveSection failure: true on second remove")
	}

	// test types
	if !c.AddSection("section2") { // add section
		t.Errorf("AddSection failure: false on first insert")
	}

	if !c.AddOption("section2", "test-number", "666") { // add number
		t.Errorf("AddOption failure: false on first insert")
	}
	testGet(t, c, "section2", "test-number", int64(666)) // read it back

	if !c.AddOption("section2", "test-yes", "yes") { // add 'yes' (bool)
		t.Errorf("AddOption failure: false on first insert")
	}
	testGet(t, c, "section2", "test-yes", true) // read it back

	if !c.AddOption("section2", "test-false", "false") { // add 'false' (bool)
		t.Errorf("AddOption failure: false on first insert")
	}
	testGet(t, c, "section2", "test-false", false) // read it back

	// test cycle
	c.AddOption(DefaultSection, "opt1", "%(opt2)s")
	c.AddOption(DefaultSection, "opt2", "%(opt1)s")

	_, err = c.GetString(DefaultSection, "opt1")
	if err == nil {
		t.Errorf("GetString failure: no error for cycle")
	} else if strings.Index(err.Error(), "cycle") < 0 {
		t.Errorf("GetString failure: incorrect error for cycle")
	}
}

// Create a 'tough' configuration file and test (read) parsing.
func TestReadFile(t *testing.T) {
	const tmp = "/tmp/__config_test.go__garbage"
	defer os.Remove(tmp)

	file, err := os.Create(tmp)
	if err != nil {
		t.Fatalf("Test cannot run because cannot write temporary file: " + tmp)
	}

	buf := bufio.NewWriter(file)
	buf.WriteString("[section-1]\n")
	buf.WriteString("  option1=value1 ; This is a comment\n")
	buf.WriteString(" option2 : 2#Not a comment\t#Now this is a comment after a TAB\n")
	buf.WriteString("  # Let me put another comment\n")
	buf.WriteString("    option3= line1\nline2 \n\tline3 # Comment\n")
	buf.WriteString("; Another comment\n")
	buf.WriteString("[" + DefaultSection + "]\n")
	buf.WriteString("variable1=small\n")
	buf.WriteString("variable2=a_part_of_a_%(variable1)s_test\n")
	buf.WriteString("[secTION-2]\n")
	buf.WriteString("IS-flag-TRUE=Yes\n")
	buf.WriteString("[section-1]\n") // continue again [section-1]
	buf.WriteString("option4=this_is_%(variable2)s.\n")
	buf.Flush()
	file.Close()

	c, err := ReadConfigFile(tmp)
	if err != nil {
		t.Fatalf("ReadConfigFile failure: " + err.Error())
	}

	if len(c.GetSections()) != 3 { // check number of sections
		t.Errorf("GetSections failure: wrong number of sections")
	}

	opts, err := c.GetOptions("section-1") // check number of options
	if len(opts) != 6 {                    // 4 of [section-1] plus 2 of [default]
		t.Errorf("GetOptions failure: wrong number of options")
	}

	testGet(t, c, "section-1", "option1", "value1")
	testGet(t, c, "section-1", "option2", "2#Not a comment")
	testGet(t, c, "section-1", "option3", "line1\nline2\nline3")
	testGet(t, c, "section-1", "option4", "this_is_a_part_of_a_small_test.")
	testGet(t, c, "SECtion-2", "is-FLAG-true", true) // case-insensitive
}

// Test writing and reading back a configuration file.
func TestWriteReadFile(t *testing.T) {
	const tmp = "/tmp/__config_test.go__garbage"
	defer os.Remove(tmp)

	cw := NewConfigFile()

	// write file; will test only read later on
	cw.AddSection("First-Section")
	cw.AddOption("First-Section", "option1", "value option1")
	cw.AddOption("First-Section", "option2", "2")

	cw.AddOption(DefaultSection, "host", "www.example.com")
	cw.AddOption(DefaultSection, "protocol", "https://")
	cw.AddOption(DefaultSection, "base-url", "%(protocol)s%(host)s")

	cw.AddOption("Another-Section", "useHTTPS", "y")
	cw.AddOption("Another-Section", "url", "%(base-url)s/some/path")

	cw.WriteConfigFile(tmp, 0644, "Test file for test-case")

	// read back file and test
	cr, err := ReadConfigFile(tmp)
	if err != nil {
		t.Fatalf("ReadConfigFile failure: " + err.Error())
	}

	testGet(t, cr, "first-section", "option1", "value option1")
	testGet(t, cr, "first-section", "option2", int64(2))
	testGet(t, cr, "Another-SECTION", "usehttps", true)
	testGet(t, cr, "another-section", "url", "https://www.example.com/some/path")
}
