#include "LuaTestRunner.h"
#include "Framework/SysPrint.h"
#include <fstream>
#include <sstream>

extern void Quit();

void LuaTestRunner::finish(int pass, int fail, std::string failures) {
	const int total = pass + fail;
	sys_print(Info, "LuaTestRunner: %d/%d tests passed\n", pass, total);
	if (fail > 0)
		sys_print(Error, "LuaTestRunner failures:\n%s\n", failures.c_str());

	// XML escape helper
	auto xml_escape = [](const std::string& s) {
		std::string out;
		out.reserve(s.size());
		for (char c : s) {
			switch (c) {
			case '&':
				out += "&amp;";
				break;
			case '<':
				out += "&lt;";
				break;
			case '>':
				out += "&gt;";
				break;
			case '"':
				out += "&quot;";
				break;
			case '\'':
				out += "&apos;";
				break;
			default:
				out += c;
				break;
			}
		}
		return out;
	};

	// Write JUnit XML
	std::ofstream f("TestFiles/integration_lua_results.xml");
	f << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
	f << "<testsuite name=\"lua_integration\" tests=\"" << total << "\" failures=\"" << fail << "\">\n";

	// Parse failure list: each line is "testname: message"
	std::istringstream ss(failures);
	std::string line;
	while (std::getline(ss, line)) {
		if (line.empty())
			continue;
		auto colon = line.find(": ");
		std::string name = (colon != std::string::npos) ? line.substr(0, colon) : line;
		std::string msg = (colon != std::string::npos) ? line.substr(colon + 2) : "";
		f << "  <testcase name=\"" << xml_escape(name) << "\">\n";
		f << "    <failure message=\"" << xml_escape(msg) << "\"/>\n";
		f << "  </testcase>\n";
	}
	f << "</testsuite>\n";
	f.close();

	Quit();
}
