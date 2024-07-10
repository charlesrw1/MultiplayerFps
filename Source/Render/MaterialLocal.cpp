#include "MaterialLocal.h"
#include "Framework/Files.h"
inline std::string remove_filename_from_path(std::string& path)
{
	auto find = path.rfind("/");
	if (find == std::string::npos) {
		return "";
	}
	else {
		std::string wo_filename = path.substr(0, find);
		path = path.substr(find);
		return wo_filename;
	}
}

const MaterialType* MaterialManagerLocal::find_master_material(const char* master_name)
{
	std::string material_name = master_name;
	auto wo_path = remove_filename_from_path(material_name);

	wo_path += "/.output/" + material_name + "_generated.txt";

	//FileSys::open_read()
	return {};
}
