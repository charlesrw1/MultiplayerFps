#include "EnumDefReflection.h"
#include "GlobalEnumMgr.h"

AutoEnumDef::AutoEnumDef(const char* name, int count, const char** strs)
{
	id = GlobalEnumDefMgr::get().add_enum({ name,count,strs });
}