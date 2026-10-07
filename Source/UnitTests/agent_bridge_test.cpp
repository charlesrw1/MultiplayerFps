#include <gtest/gtest.h>
#include "AgentBridge/AgentBridge.h"

TEST(AgentBridgeTest, DescriptorSerializesMachineReadableMetadata) {
	AgentBridgeCommandDescriptor descriptor;
	descriptor.name = "test_descriptor";
	descriptor.description = "test command";
	descriptor.availability = "editor";
	descriptor.effects = "mutating";
	descriptor.input_schema = {{"type", "object"}, {"required", {"value"}}};

	auto json = descriptor.to_json();
	EXPECT_EQ(json["name"], "test_descriptor");
	EXPECT_EQ(json["description"], "test command");
	EXPECT_EQ(json["availability"], "editor");
	EXPECT_EQ(json["effects"], "mutating");
	EXPECT_EQ(json["input_schema"]["required"][0], "value");
}

TEST(AgentBridgeTest, CatalogIsSortedAndDescribesRegisteredCommand) {
	AgentBridgeCommandDescriptor descriptor;
	descriptor.name = "unit_test_typed_command";
	descriptor.description = "registered during the unit test";
	agent_bridge_register_command(descriptor, [](const nlohmann::json&) { return nlohmann::json{{"ok", true}}; });

	auto described = agent_bridge_describe_command("unit_test_typed_command");
	EXPECT_EQ(described["description"], descriptor.description);

	auto catalog = agent_bridge_list_commands();
	std::string previous;
	bool found = false;
	for (const auto& command : catalog) {
		const std::string name = command["name"].get<std::string>();
		EXPECT_LE(previous, name);
		previous = name;
		found |= name == descriptor.name;
	}
	EXPECT_TRUE(found);
}
