#include <iostream>
#include <regex>
#include <unordered_map>
#include <cstdio>
#include <cpr/cpr.h>
#include <rapidjson/document.h>
#include <rapidjson/filereadstream.h>
#include <rapidjson/filewritestream.h>
#include <rapidjson/prettywriter.h>

template<rapidjson::SizeType N>
void addMember(rapidjson::Value& object, const char(&key)[N], const std::string& s, rapidjson::Document::AllocatorType& allocator) {
	object.AddMember(key, rapidjson::Value(s, allocator), allocator);
}

rapidjson::Value parse(const std::string& s, rapidjson::Document::AllocatorType& allocator) {
	auto configs = rapidjson::Value(rapidjson::kArrayType);
	try {
		std::regex r("<h4>[^<:]+:([^<]+)</h4>\\s+<h4>[^<:]+:([0-9]+)</h4>\\s+<h4>[^<:]+:([^<]*)</h4>\\s+<h4>[^<:]+:([^<]*)</h4>");

		auto it = std::sregex_iterator(s.begin(), s.end(), r);
		auto end = std::sregex_iterator();
		for (; it != end; ++it) {
			auto conf = rapidjson::Value(rapidjson::kObjectType);

			auto i = it->cbegin();
			auto server = (++i)->str();
			conf.AddMember("server", rapidjson::Value(server, allocator), allocator);
			conf.AddMember("server_port", rapidjson::Value(std::stoi((++i)->str())), allocator);
			auto password = (++i)->str();
			if (password.empty())
				continue;
			conf.AddMember("password", rapidjson::Value(password, allocator), allocator);
			conf.AddMember("method", rapidjson::Value((++i)->str(), allocator), allocator);
			conf.AddMember("auth", rapidjson::Value(false), allocator);

			configs.PushBack(std::move(conf), allocator);
		}
	}
	catch (std::regex_error& e) {
		std::cerr << e.what() << std::endl;
	}

	return configs;
}

void merge(rapidjson::Value& local, rapidjson::Value&& remote, rapidjson::Document::AllocatorType& allocator) {
	if (!local.IsArray() || local.Empty()) {
		local.Swap(remote);
		return;
	}
	std::unordered_map<std::string, rapidjson::Value*> servers;
	for (auto& v : local.GetArray()) {
		if (!v.HasMember("server")) continue;
		servers[v["server"].GetString()] = &v;
	}

	for (auto& v : remote.GetArray()) {
		if (!v.HasMember("server")) continue;

		auto it = servers.find(v["server"].GetString());
		if (it != servers.end()) {
			it->second->Swap(v);
		}
		else {
			local.PushBack(v, allocator);
		}
	}
}

std::unique_ptr<FILE, int(*)(FILE*)> openf(const char* filename, const char* mode) {
	FILE* f = nullptr;
#ifdef WIN32
	fopen_s(&f, filename, mode);
#else
	f = fopen(filename, mode);
#endif
	return std::unique_ptr<FILE, int(*)(FILE*)>(f, fclose);
}

rapidjson::Document load(const char* filename) {
	auto fp = openf(filename, "rt");
	rapidjson::Document d(rapidjson::kObjectType);

	if (fp) {
		char readBuffer[65536];
		rapidjson::FileReadStream is(fp.get(), readBuffer, sizeof(readBuffer));
		d.ParseStream(is);
	}

	if (!d.HasMember("configs")) {
		d.AddMember("configs", rapidjson::Value(rapidjson::kArrayType), d.GetAllocator());
	}

	return std::move(d);
}

void save(const rapidjson::Document& doc, const char* filename) {
	auto fp = openf(filename, "wt");
	char writeBuffer[65536];
	rapidjson::FileWriteStream os(fp.get(), writeBuffer, sizeof(writeBuffer));
	rapidjson::PrettyWriter<rapidjson::FileWriteStream> writer(os);
	writer.SetIndent(' ', 2);
	doc.Accept(writer);
}

int main()
{
	auto r = cpr::Get(cpr::Url{ "http://www.ishadowsocks.org/" });
	if (r.status_code != 200)
		return -1;

	static auto filename = "gui-config.json";
	auto doc = load(filename);

	auto& allocator = doc.GetAllocator();
	
	auto v = parse(r.text, allocator);

	merge(doc["configs"], std::move(v), allocator);

	save(doc, filename);
	return 0;
}
