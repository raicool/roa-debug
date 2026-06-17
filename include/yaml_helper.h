#pragma once

#include <yaml-cpp/yaml.h>

template<typename T>
YAML::Node get_nodeleaf_safe(YAML::Node node, std::string leaf, T placeholder)
{
	if (node[leaf].IsDefined())
	{
		return node[leaf];
	}
	return YAML::Node(placeholder);
}

inline bool file_exists(const std::string& name) 
{
	if (FILE* file = fopen(name.c_str(), "r")) 
	{
		fclose(file);
		return true;
	}
	else 
	{
		return false;
	}
}