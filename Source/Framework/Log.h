#pragma once
#include "Framework/Util.h"

#define LOG_MSG(msg) sys_print(Info, "[%s:%d] %s\n", __FUNCTION__, __LINE__, msg)
#define LOG_ERR(msg) sys_print(Error, "[%s:%d] %s\n", __FUNCTION__, __LINE__, msg)
#define LOG_WARN(msg) sys_print(Warning, "[%s:%d] %s\n", __FUNCTION__, __LINE__, msg)