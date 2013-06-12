// This package implements a parser for configuration files.
// This allows easy reading and writing of structured configuration files.
//
// Given a sample configuration file:
//
//	[default]
//	host=www.example.com
//	protocol=http://
//	base-url=%(protocol)s%(host)s
//
//	[service-1]
//	url=%(base-url)s/some/path
//	delegation : on
//	maxclients=200 # do not set this higher
//	comments=This is a multi-line
//		entry	; And this is a comment
//
// To read this configuration file, do:
//
//	c, err := configfile.ReadConfigFile("config.cfg");
//
//  // result is string :http://www.example.com/some/path"
//	c.GetString("service-1", "url");
//
//	c.GetInt64("service-1", "maxclients"); // result is int 200
//	c.GetBool("service-1", "delegation"); // result is bool true
//
//  // result is string "This is a multi-line\nentry"
//	c.GetString("service-1", "comments");
//
// Note the support for unfolding variables (such as %(base-url)s), which are
// read from the special (reserved) section name [default].
//
// A new configuration file can also be created with:
//
//	c := configfile.NewConfigFile();
//	c.AddSection("section");
//	c.AddOption("section", "option", "value");
//  // use 0644 as file permission
//	c.WriteConfigFile("config.cfg", 0644, "A header for this file");
//
// This results in the file:
//
//	# A header for this file
//	[section]
//	option=value
//
// Note that sections and options are case-insensitive (values are
// case-sensitive) and are converted to lowercase when saved to a file.
//
// The functionality and workflow is loosely based on the configparser.py
// package of the Python Standard Library.
package goconfig

import (
	"bufio"
	"errors"
	"fmt"
	"io"
	"os"
	"regexp"
	"strconv"
	"strings"
)

// ConfigFile is the representation of configuration settings.
// The public interface is entirely through methods.
type ConfigFile struct {
	data map[string]map[string]string // Maps sections to options to values.
}

var (
	DefaultSection = "default" // Default section name (must be lower-case).

	// Maximum allowed depth when recursively substituing variable names.
	DepthValues = 200

	// Strings accepted as bool.
	BoolStrings = map[string]bool{
		"0":     false,
		"1":     true,
		"f":     false,
		"false": false,
		"n":     false,
		"no":    false,
		"off":   false,
		"on":    true,
		"t":     true,
		"true":  true,
		"y":     true,
		"yes":   true,
	}

	varRegExp = regexp.MustCompile(`%\(([a-zA-Z0-9_.\-]+)\)s`)
)

// AddSection adds a new section to the configuration.
// It returns true if the new section was inserted, and false if the section
// already existed.
func (c *ConfigFile) AddSection(section string) bool {

	section = strings.ToLower(section)

	if _, ok := c.data[section]; ok {
		return false
	}
	c.data[section] = make(map[string]string)

	return true
}

// RemoveSection removes a section from the configuration.
// It returns true if the section was removed, and false if section did not
// exist.
func (c *ConfigFile) RemoveSection(section string) bool {

	section = strings.ToLower(section)

	switch _, ok := c.data[section]; {
	case !ok:
		return false
	case section == DefaultSection:
		return false // default section cannot be removed
	default:
		for o, _ := range c.data[section] {
			delete(c.data[section], o)
		}
		delete(c.data, section)
	}

	return true
}

// AddOption adds a new option and value to the configuration.
// It returns true if the option and value were inserted, and false if the
// value was overwritten.
// If the section does not exist in advance, it is created.
func (c *ConfigFile) AddOption(section string, option string, value string) bool {

	c.AddSection(section) // make sure section exists

	section = strings.ToLower(section)
	option = strings.ToLower(option)

	_, ok := c.data[section][option]
	c.data[section][option] = value

	return !ok
}

// RemoveOption removes a option and value from the configuration.
// It returns true if the option and value were removed, and false otherwise,
// including if the section did not exist.
func (c *ConfigFile) RemoveOption(section string, option string) bool {

	section = strings.ToLower(section)
	option = strings.ToLower(option)

	if _, ok := c.data[section]; !ok {
		return false
	}

	_, ok := c.data[section][option]
	delete(c.data[section], option)

	return ok
}

// NewConfigFile creates an empty configuration representation.
// This representation can be filled with AddSection and AddOption and then
// saved to a file using WriteConfigFile.
func NewConfigFile() *ConfigFile {

	c := new(ConfigFile)
	c.data = make(map[string]map[string]string)

	c.AddSection(DefaultSection) // default section always exists

	return c
}

func stripComments(l string) string {

	// comments are preceded by space or TAB
	for _, c := range []string{" ;", "\t;", " #", "\t#"} {
		if i := strings.Index(l, c); i != -1 {
			l = l[0:i]
		}
	}

	return l
}

func firstIndex(s string, delim []byte) int {

	for i := 0; i < len(s); i++ {
		for j := 0; j < len(delim); j++ {
			if s[i] == delim[j] {
				return i
			}
		}
	}

	return -1
}

func (c *ConfigFile) read(buf *bufio.Reader) error {

	var section, option string

	for {
		l, err := buf.ReadString('\n') // parse line-by-line
		if err == io.EOF {
			break
		} else if err != nil {
			return err
		}

		l = strings.TrimSpace(l)
		// switch written for readability (not performance)
		switch {
		case len(l) == 0: // empty line
			continue

		case l[0] == '#': // comment
			continue

		case l[0] == ';': // comment
			continue

		case len(l) >= 3 && strings.ToLower(l[0:3]) == "rem":
			// comment (for windows users)
			continue

		case l[0] == '[' && l[len(l)-1] == ']': // new section
			option = "" // reset multi-line value
			section = strings.TrimSpace(l[1 : len(l)-1])
			c.AddSection(section)

		case section == "": // not new section and no section defined so far
			return errors.New("Section not found: must start with section")

		default: // other alternatives
			i := firstIndex(l, []byte{'=', ':'})
			switch {
			case i > 0: // option and value
				i := firstIndex(l, []byte{'=', ':'})
				option = strings.TrimSpace(l[0:i])
				value := strings.TrimSpace(stripComments(l[i+1:]))
				c.AddOption(section, option, value)

			case section != "" && option != "":
				// continuation of multi-line value
				prev, _ := c.GetRawString(section, option)
				value := strings.TrimSpace(stripComments(l))
				c.AddOption(section, option, prev+"\n"+value)

			default:
				return errors.New(fmt.Sprintf("Could not parse line: %s", l))
			}
		}
	}

	return nil
}

// ReadConfigFile reads a file and returns a new configuration representation.
// This representation can be queried with GetString, etc.
func ReadConfigFile(fname string) (*ConfigFile, error) {

	file, err := os.Open(fname)
	if err != nil {
		return nil, err
	}

	c := NewConfigFile()
	if err := c.read(bufio.NewReader(file)); err != nil {
		return nil, err
	}

	if err := file.Close(); err != nil {
		return nil, err
	}

	return c, nil
}

func (c *ConfigFile) write(buf *bufio.Writer, header string) error {

	if header != "" {
		_, err := buf.WriteString(fmt.Sprintf("# %s\n", header))
		if err != nil {
			return err
		}
	}

	for section, sectionmap := range c.data {

		if section == DefaultSection && len(sectionmap) == 0 {
			continue // skip default section if empty
		}

		_, err := buf.WriteString(fmt.Sprintf("[%s]\n", section))
		if err != nil {
			return err
		}

		for option, value := range sectionmap {
			_, err := buf.WriteString(fmt.Sprintf("%s = %s\n", option, value))
			if err != nil {
				return err
			}
		}

		if _, err := buf.WriteString("\n"); err != nil {
			return err
		}
	}

	return nil
}

// WriteConfigFile saves the configuration representation to a file.
// The desired file permissions must be passed as in os.Open.
// The header is a string that is saved as a comment in the first line of the file.
func (c *ConfigFile) WriteConfigFile(fname string, perm uint32, header string) error {

	var file *os.File

	file, err := os.OpenFile(fname, os.O_WRONLY|os.O_CREATE|os.O_TRUNC, os.FileMode(perm))
	if err != nil {
		return err
	}

	buf := bufio.NewWriter(file)
	if err := c.write(buf, header); err != nil {
		return err
	}
	buf.Flush()

	return file.Close()
}

// GetSections returns the list of sections in the configuration.
// (The default section always exists.)
func (c *ConfigFile) GetSections() (sections []string) {

	sections = make([]string, len(c.data))

	i := 0
	for s, _ := range c.data {
		sections[i] = s
		i++
	}

	return sections
}

// HasSection checks if the configuration has the given section.
// (The default section always exists.)
func (c *ConfigFile) HasSection(section string) bool {

	_, ok := c.data[strings.ToLower(section)]

	return ok
}

// GetOptions returns the list of options available in the given section.
// It returns an error if the section does not exist and an empty list if the
// section is empty.
// Options within the default section are also included.
func (c *ConfigFile) GetOptions(section string) ([]string, error) {

	section = strings.ToLower(section)

	if _, ok := c.data[section]; !ok {
		return nil, errors.New(
			fmt.Sprintf("Section not found: %s", section),
		)
	}

	options := make([]string, len(c.data[DefaultSection])+len(c.data[section]))

	i := 0
	for s, _ := range c.data[DefaultSection] {
		options[i] = s
		i++
	}

	for s, _ := range c.data[section] {
		options[i] = s
		i++
	}

	return options, nil
}

// HasOption checks if the configuration has the given option in the section.
// It returns false if either the option or section do not exist.
func (c *ConfigFile) HasOption(section string, option string) bool {

	section = strings.ToLower(section)
	option = strings.ToLower(option)

	if _, ok := c.data[section]; !ok {
		return false
	}

	_, okd := c.data[DefaultSection][option]
	_, oknd := c.data[section][option]

	return okd || oknd
}

// GetRawString gets the (raw) string value for the given option in the
// section.
// The raw string value is not subjected to unfolding, which was illustrated
// in the beginning of this documentation.
// It returns an error if either the section or the option do not exist.
func (c *ConfigFile) GetRawString(section string, option string) (string, error) {

	section = strings.ToLower(section)
	option = strings.ToLower(option)

	if _, ok := c.data[section]; ok {

		if value, ok := c.data[section][option]; ok {
			return value, nil
		}

		return "", errors.New(fmt.Sprintf("Option not found: %s", option))
	}

	return "", errors.New(fmt.Sprintf("Section not found: %s", section))
}

// GetString gets the string value for the given option in the section.
// If the value needs to be unfolded (see e.g. %(host)s example in the
// beginning of this documentation),
// then GetString does this unfolding automatically, up to DepthValues number
// of iterations.
// It returns an error if either the section or the option do not exist, or
// the unfolding cycled.
func (c *ConfigFile) GetString(section string, option string) (string, error) {

	value, err := c.GetRawString(section, option)
	if err != nil {
		return "", err
	}

	section = strings.ToLower(section)

	var i int

	for i = 0; i < DepthValues; i++ { // keep a sane depth

		vr := varRegExp.FindStringSubmatchIndex(value)
		if len(vr) == 0 {
			break
		}

		noption := value[vr[2]:vr[3]]
		noption = strings.ToLower(noption)

		// search variable in default section
		nvalue, _ := c.data[DefaultSection][noption]
		if _, ok := c.data[section][noption]; ok {
			nvalue = c.data[section][noption]
		}

		if nvalue == "" {
			return "", errors.New(fmt.Sprintf("Option not found: %s", noption))
		}

		// substitute by new value and take off leading '%(' and trailing ')s'
		value = value[0:vr[2]-2] + nvalue + value[vr[3]+2:]
	}

	if i == DepthValues {
		return "",
			errors.New(
				fmt.Sprintf(
					"Possible cycle while unfolding variables: max depth of %d reached",
					strconv.Itoa(DepthValues),
				),
			)
	}

	return value, nil
}

// GetInt has the same behaviour as GetString but converts the response to int.
func (c *ConfigFile) GetInt64(section string, option string) (int64, error) {

	sv, err := c.GetString(section, option)
	if err != nil {
		return 0, err
	}

	value, err := strconv.ParseInt(sv, 10, 64)
	if err != nil {
		return 0, err
	}

	return value, nil
}

// GetFloat has the same behaviour as GetString but converts the response to
// float.
func (c *ConfigFile) GetFloat(section string, option string) (float64, error) {

	sv, err := c.GetString(section, option)
	if err != nil {
		return float64(0), err
	}

	value, err := strconv.ParseFloat(sv, 64)
	if err != nil {
		return float64(0), err
	}

	return value, nil
}

// GetBool has the same behaviour as GetString but converts the response to
// bool.
// See constant BoolStrings for string values converted to bool.
func (c *ConfigFile) GetBool(section string, option string) (bool, error) {

	sv, err := c.GetString(section, option)
	if err != nil {
		return false, err
	}

	value, ok := BoolStrings[strings.ToLower(sv)]
	if ok == false {
		return false, errors.New(
			fmt.Sprintf("Could not parse bool value: %s", sv),
		)
	}

	return value, nil
}
