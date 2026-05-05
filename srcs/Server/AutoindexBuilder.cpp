#include "AutoindexBuilder.hpp"

#include <dirent.h>

bool AutoindexBuilder::buildBody(const HttpRequest& request,
		const std::string& directoryPath,
		std::string& body) const
{
	DIR* dir = opendir(directoryPath.c_str());
	std::string base = request.path();

	if(dir == NULL)
	{
		return false;
	}
	if(base.empty() || base[base.size() - 1] != '/')
	{
		base += "/";
	}
	body = "<html><head><title>Index of " + htmlEscape(request.path())
		+ "</title></head><body><h1>Index of " + htmlEscape(request.path())
		+ "</h1><ul>";
	while(true)
	{
		struct dirent* entry = readdir(dir);
		std::string name;

		if(entry == NULL)
		{
			break;
		}
		name = entry->d_name;
		if(name == ".")
		{
			continue;
		}
		body += "<li><a href=\"" + htmlEscape(base + name) + "\">"
			+ htmlEscape(name) + "</a></li>";
	}
	closedir(dir);
	body += "</ul></body></html>\n";
	return true;
}

std::string AutoindexBuilder::htmlEscape(const std::string& value) const
{
	std::string result;

	for(std::size_t i = 0; i < value.size(); i++)
	{
		if(value[i] == '&')
			result += "&amp;";
		else if(value[i] == '<')
			result += "&lt;";
		else if(value[i] == '>')
			result += "&gt;";
		else if(value[i] == '"')
			result += "&quot;";
		else
			result += value[i];
	}
	return result;
}
