/*
 * this class holds the big xml structure that is referenced
 *
 * It contains some accessor functions (XPath?)
 *
 * For now we simply use an XML library
 * This class is the wrapper around that, so our own API
 * is as independent as possible from whatever library we use
 * 
 * This class can wrap around any level within the big
 * structure; it is assumed the classes that use this one
 * are aware of the part of the tree they are working with
 *
 * IDENTIFIER STRINGS:
 * specific parts of the configuration can be addressed in all
 * functions that take an identifier string, which is loosely based
 * on XPath
 *
 * Take the example xml format:
 * <?xml version="1.0"?>
 * <config>
 *     <module name="authoritative">
 *         <listen-port>53</listen-port>
 *         <zones>
 *             <zone name="tjeb.nl">
 *                 <type>master</type>
 *                 <file>/var/zones/tjeb.nl</file>
 *             </zone>
 *             <zone name="theo.com">
 *                 <type>master</type>
 *                 <file>/var/zones/theo.com</file>
 *             </zone>
 *         </zones>
 *     </module>
 * </config>
 *
 * Nodes are identifier by their XML name, subnodes are seperated with
 * a '/' character.
 *
 * For example, if we load the above xml file into a Config structure,
 * the identifier for the listen-port config part is
 * "/module/listen-port"
 *
 * Attributes can be specified with an '@' character:
 * "/module@name"
 * 
 * If multiple nodes with the same name are children of the current
 * node, specific ones may be addressed with a sub-identifier followed
 * by a value to match:
 * "/module[@name=authoritative]/zones/zone[@name=theo.com]"
 *
 * If the element or node is not found, usually a ConfigError exception
 * is thrown.
 */
#ifndef _CONFIG_OBJ_HH
#define _CONFIG_OBJ_HH 1

#include <string>
#include <iostream>

#include <xercesc/dom/DOM.hpp>
#include <xercesc/parsers/XercesDOMParser.hpp>

using namespace xercesc;

namespace ISC { namespace Config {

    /* current xml module requires an init and terminate*/
    void config_init();
    void config_cleanup();

    /* exception class */
    class ConfigError : public std::exception {
    public:
        ConfigError(std::string m="exception!") : msg(m) {}
        ~ConfigError() throw() {}
        const char* what() const throw() { return msg.c_str(); }

    private:
        std::string msg;
    };

    class Config {
        /* base node for this configuration of configuration part */;
        DOMNode *node;

        /* the parser needs to be retained outside of the parse
         * function */
        XercesDOMParser *parser;

    public:
        /* constructs an empty config element */
        Config() : node(NULL) { parser = new XercesDOMParser(); }
        /* constructs a config element with the xml data found in
         * the given file, throws ConfigError on error */
        Config(std::string filename);
        /* constructs a config element with the xml data found in
         * the given input stream. Throws ConfigError on error.
         * Stream functionality is not completely implemented yet
         */
        Config(std::istream &in);
        
        ~Config() { if (parser) { delete parser; } }

        /* returns the name of the base node */
        std::string getName();
        /* returns the value of the base node.
         * If the base node is not an attribute node or an
         * element node with only one text node child, a
         * ConfigError is thrown */
        std::string getValue();
        /* returns the value of a specific part of the configuration
         * See IDENTIFIER STRINGS above.
         * If not found, or if the node does not have a gettable value,
         * a ConfigError exception is thrown
         */
        std::string getValue(std::string identifier);

        /* sets the value of the base node.
         * If the base node is not an attribute node or an
         * element node with only one text node child, a
         * ConfigError is thrown */
        void setValue(std::string const &value);
        /* sets the value of a specific part of the configuration
         * See IDENTIFIER STRINGS above.
         * If not found, a ConfigError exception is thrown
         */
        void setValue(std::string identifier, std::string const &value);

        /*
         * Adds an empty element to the children of the current node
         */
        void addChild(std::string name);
        
        /*
         * Adds an empty element to the children of the node specified
         * by the identifier.
         * See IDENTIFIER STRINGS above.
         * If not found, a ConfigError exception is thrown
         */
        void addChild(std::string identifier, std::string name);

        /* returns a clone of a specific subtree of this configuration
         * part.
         * See IDENTIFIER STRINGS above.
         * If not found, a ConfigError exception is thrown
         */
        Config *getConfigPart(std::string const &identifier);
        /* replaces a specific subtree of this configuration
         * part by a clone of the given config part.
         * See IDENTIFIER STRINGS above.
         * If not found, a ConfigError exception is thrown
         */
        void setConfigPart(std::string const &identifier, Config *config);

        /* Read in an XML file
         * Throws a ConfigError if there is a problem reading or parsing
         * the file */
        void readFile(const std::string &filename);
        /* Write out this configuration (part) to an XML file
         * Throws a ConfigError if the file cannot be opened for
         * writing */
        void writeFile(const std::string &filename);

        /* Read in an XML stream
         * Throws a ConfigError if there is a problem reading or parsing
         * the file */
        void readStream(std::istream &in);
        /* Write out this configuration (part) to the given output
         * stream */
        void writeStream(std::ostream &out);

    private:
        /* serialize a specific DOMNode to the given stream with
         * the given prefix. Children of the node are also serialized
         * with a \t character added to the prefix */
        void serializeDOMNode(std::ostream &out, DOMNode *n, std::string prefix);
        /* Serialize the complete config part to the given stream,
         * prepended with <?xml version="1.0"?> */
        void serialize(std::ostream &out);

        /* Helper function to clear out empty text nodes which are
         * the result from parsing a file without a DTD */
        void removeEmptyTextNodes(DOMNode *n);

        /* Returns the name of a specific DOMNode */
        std::string getNodeName(const DOMNode *n);
        /* Returns the value of a specific DOMNode */
        std::string getNodeValue(const DOMNode *n);
        /* Sets the value of a specific DOMNode */
        void setNodeValue(DOMNode *n, std::string const &value);

        /* Add an empty element node to the children of the given node*/
        void addNodeChild(DOMNode *n, std::string const &name);
        
        /* Returns a DOMNode identifier by the given identifier */
        DOMNode *findSubNode(DOMNode *n, std::string const &identifier);
    };

}}

#endif
