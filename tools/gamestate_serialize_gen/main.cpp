#include "dependencies/pugixml/src/pugixml.hpp"

// Disable automatic #pragma linking for boost - only enabled in msvc and that should provide boost
// symbols as part of the module that uses it
#define BOOST_ALL_NO_LIB
#include <boost/program_options.hpp>
#include <fstream>
#include <iostream>
#include <list>
#include <map>
#include <string>

namespace po = boost::program_options;

enum class NodeType
{
	Normal,
	Section,
	SectionMap,
};

class SerializeNode
{
  public:
	std::string name;
	NodeType type = NodeType::Normal;
	SerializeNode(std::string name) : name(name) {}
};

class SerializeObject
{
  public:
	std::string name;
	std::list<std::pair<std::string, SerializeNode>> members;
	SerializeObject(std::string name) : name(name) {}
};

class SerializeEnum
{
  public:
	std::string name;
	std::list<std::string> values;
};

class StateDefinition
{
  public:
	std::list<SerializeObject> objects;
	std::list<SerializeEnum> enums;
};

bool readXml(std::istream &in, StateDefinition &state)
{
	pugi::xml_document doc;

	auto ret = doc.load(in);
	if (!ret)
	{
		std::cerr << "Failed to parse file:" << ret.description() << "\n";
		return false;
	}

	auto rootNode = doc.first_child();
	while (rootNode)
	{
		if (std::string(rootNode.name()) == "openapoc_gamestate")
		{
			auto objectNode = rootNode.first_child();
			while (objectNode)
			{
				if (std::string(objectNode.name()) == "object")
				{
					SerializeObject obj("");
					auto memberNode = objectNode.first_child();
					while (memberNode)
					{
						if (std::string(memberNode.name()) == "member")
						{
							std::string memberName = memberNode.text().as_string();
							NodeType type = NodeType::Normal;
							auto typeAttr = memberNode.attribute("type");
							if (!typeAttr.empty())
							{
								std::string typeName = typeAttr.as_string();
								if (typeName == "Normal")
									type = NodeType::Normal;
								else if (typeName == "Section")
									type = NodeType::Section;
								else if (typeName == "SectionMap")
									type = NodeType::SectionMap;
								else
									std::cerr << "Unknown member type attrribute \"" << typeName
									          << "\"\n";
							}
							SerializeNode member(memberName);
							member.type = type;
							obj.members.push_back(std::make_pair(member.name, member));
						}
						else if (std::string(memberNode.name()) == "name")
						{
							obj.name = memberNode.text().as_string();
						}
						memberNode = memberNode.next_sibling();
					}
					state.objects.push_back(obj);
				}
				else if (std::string(objectNode.name()) == "enum")
				{
					SerializeEnum sEnum;
					auto valueNode = objectNode.first_child();
					while (valueNode)
					{
						if (std::string(valueNode.name()) == "value")
						{
							sEnum.values.push_back(valueNode.text().as_string());
						}
						else if (std::string(valueNode.name()) == "name")
						{
							sEnum.name = valueNode.text().as_string();
						}
						valueNode = valueNode.next_sibling();
					}
					state.enums.push_back(sEnum);
				}
				objectNode = objectNode.next_sibling();
			}
		}
		rootNode = rootNode.next_sibling();
	}

	return true;
}

void writeHeader(std::ofstream &out, const StateDefinition &state)
{
	out << "// GENERATED HEADER - generated by GamestateSerializeGen - do not modify directly\n\n";
	out << "#pragma once\n\n";
	out << "#include \"game/state/gamestate.h\"\n\n";
	out << "#include \"game/state/gamestate_serialize.h\"\n\n";
	out << "namespace OpenApoc {\n\n";
	out << "class SerializationNode;\n\n";
	for (auto &object : state.objects)
	{
		out << "void serializeIn(const GameState *, sp<SerializationNode> node, " << object.name
		    << " &obj);\n";
		out << "void serializeOut(sp<SerializationNode> node, const " << object.name
		    << " &obj, const " << object.name << " &ref);\n";
		out << "bool operator==(const " << object.name << " &a, const " << object.name << " &b);\n";
		out << "bool operator!=(const " << object.name << " &a, const " << object.name
		    << " &b);\n\n";
	}

	for (auto &e : state.enums)
	{
		out << "void serializeIn(const GameState *, sp<SerializationNode> node, " << e.name
		    << " &val);\n";
		out << "void serializeOut(sp<SerializationNode> node, const " << e.name << " &val, const "
		    << e.name << " &ref);\n";
	}

	out << "\n} // namespace OpenApoc\n";
}

void writeSource(std::ofstream &out, const StateDefinition &state)
{
	out << "// GENERATED SOURCE - generated by GamestateSerializeGen - do not modify directly\n\n";
	out << "#include \"framework/serialization/serialize.h\"\n";
	out << "#include \"game/state/gamestate_serialize.h\"\n";
	out << "#include \"game/state/gamestate_serialize_generated.h\"\n\n";

	out << "namespace OpenApoc {\n\n";
	for (auto &object : state.objects)
	{
		out << "void serializeIn(const GameState *state, sp<SerializationNode> node, "
		    << object.name << " &obj)\n{\n";

		out << "\tif (!node) return;\n";

		for (auto &member : object.members)
		{
			std::string serializeFn;
			std::string newNodeFn;
			switch (member.second.type)
			{
				case NodeType::Normal:
					serializeFn = "serializeIn";
					newNodeFn = "getNode";
					break;
				case NodeType::Section:
					serializeFn = "serializeIn";
					newNodeFn = "getSection";
					break;
				case NodeType::SectionMap:
					serializeFn = "serializeInSectionMap";
					newNodeFn = "getSection";
					break;
			}
			out << "\t" << serializeFn << "(state, node->" << newNodeFn << "(\"" << member.first
			    << "\"), obj." << member.first << ");\n";
		}

		out << "}\n";

		out << "void serializeOut(sp<SerializationNode> node, const " << object.name
		    << " &obj, const " << object.name << " &ref)\n{\n";

		for (auto &member : object.members)
		{
			std::string serializeFn;
			std::string newNodeFn;
			switch (member.second.type)
			{
				case NodeType::Normal:
					serializeFn = "serializeOut";
					newNodeFn = "addNode";
					break;
				case NodeType::Section:
					serializeFn = "serializeOut";
					newNodeFn = "addSection";
					break;
				case NodeType::SectionMap:
					serializeFn = "serializeOutSectionMap";
					newNodeFn = "addSection";
					break;
			}
			out << "\tif (obj." << member.first << " != ref." << member.first << ")" << serializeFn
			    << "(node->" << newNodeFn << "(\"" << member.first << "\"), obj." << member.first
			    << ", ref." << member.first << ");\n";
		}

		out << "}\n";

		out << "bool operator==(const " << object.name << " &a, const " << object.name
		    << " &b)\n{\n";

		for (auto &member : object.members)
		{
			out << "\tif (a." << member.first << " != b." << member.first << ") { return false;}\n";
		}

		out << "\treturn true;\n}\n";

		out << "bool operator!=(const " << object.name << " &a, const " << object.name
		    << " &b)\n{\n\treturn !(a == b);\n}\n";
	}

	for (auto &e : state.enums)
	{
		out << "void serializeIn(const GameState *state, sp<SerializationNode> node, " << e.name
		    << " &val)\n{\n"
		    << "\tstatic const std::map<" << e.name << ", UString> valueMap = {\n";
		for (auto &value : e.values)
		{
			out << "\t\t{" << e.name << "::" << value << ", \"" << value << "\"},\n";
		}
		out << "\t};\n";

		out << "\tserializeIn(state, node, val, valueMap);\n"
		    << "}\n";

		out << "void serializeOut(sp<SerializationNode> node, const " << e.name << " &val, const "
		    << e.name << " &ref)\n{\n"
		    << "\tstatic const std::map<" << e.name << ", UString> valueMap = {\n";
		for (auto &value : e.values)
		{
			out << "\t\t{" << e.name << "::" << value << ", \"" << value << "\"},\n";
		}
		out << "\t};\n";

		out << "\tserializeOut(node, val, ref, valueMap);\n"
		    << "}\n";
	}
	out << "\n} // namespace OpenApoc\n";
}

int main(int argc, char **argv)
{

	po::options_description desc("Allowed options");
	// These operator() really screw up clang-format
	// clang-format off
	desc.add_options()
		("help", "Show help message")
		("xml,x", po::value<std::string>(),"Input XML file")
		("output-header,h", po::value<std::string>(), "Path to output generated header file")
		("output-source,o", po::value<std::string>(), "Path to output generated source file")
	;
	// clang-format on

	po::variables_map vm;
	po::store(po::parse_command_line(argc, argv, desc), vm);
	po::notify(vm);

	std::string inputXmlPath;
	std::string outputHeaderPath;
	std::string outputSourcePath;

	if (vm.count("help"))
	{
		std::cout << desc << "\n";
		return 1;
	}
	if (!vm.count("xml"))
	{
		std::cerr << "Must specify input XML file\n" << desc << "\n";
		return 1;
	}
	if (!vm.count("output-header"))
	{
		std::cerr << "Must specify output header file\n" << desc << "\n";
		return 1;
	}
	if (!vm.count("output-source"))
	{
		std::cerr << "Must specify output source file\n" << desc << "\n";
		return 1;
	}

	inputXmlPath = vm["xml"].as<std::string>();
	outputHeaderPath = vm["output-header"].as<std::string>();
	outputSourcePath = vm["output-source"].as<std::string>();

	std::ifstream inputXmlFile(inputXmlPath);
	if (!inputXmlFile)
	{
		std::cerr << "Failed to open input XML file \"" << inputXmlPath << "\"\n";
		return 1;
	}

	std::ofstream outputSourceFile(outputSourcePath);
	if (!outputSourceFile)
	{
		std::cerr << "Failed to open output source file at \"" << outputSourcePath << "\"\n";
		return 1;
	}

	std::ofstream outputHeaderFile(outputHeaderPath);
	if (!outputHeaderFile)
	{
		std::cerr << "Failed to open output header file at \"" << outputHeaderPath << "\"\n";
		return 1;
	}

	StateDefinition state;
	if (!readXml(inputXmlFile, state))
	{
		std::cerr << "Reading input XML file \"" << inputXmlPath << "\" failed\n";
		return 1;
	}

	writeHeader(outputHeaderFile, state);
	writeSource(outputSourceFile, state);

	return 0;
}
