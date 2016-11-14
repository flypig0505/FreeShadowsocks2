#include <iostream>
#include <regex>
#include <unordered_map>
#include <cstdio>
#include <tuple>
#include <unordered_set>

#include <cpr/cpr.h>
#include <rapidjson/document.h>
#include <rapidjson/filereadstream.h>
#include <rapidjson/filewritestream.h>
#include <rapidjson/prettywriter.h>

typedef rapidjson::Document::AllocatorType Allocator;

struct page_t {
	page_t(std::string url, std::string regex, std::initializer_list<std::string> fields) : url(url), regex(regex), fields(fields) {
		
	}
	cpr::Url url;
	std::regex regex;
	std::vector<std::string> fields;
};

page_t pages[] = {
	{
		"http://www.ishadowsocks.org/",
		"<h4>[^<:]+:([^<]+)</h4>\\s+<h4>[^<:]+:([0-9]+)</h4>\\s+<h4>[^<:]+:([^<]*)</h4>\\s+<h4>[^<:]+:([^<]*)</h4>",
		{ "server", "server_port", "password", "method" }
	}
};

bool is_essential(const std::string& field) {
	static const std::unordered_set<std::string> essential_fields{ "server", "server_port", "password", "method" };
	return essential_fields.find(field) != essential_fields.end();
}


void fillAccount(const std::smatch& mr, const std::vector<std::string>& fields, rapidjson::Value& configs, rapidjson::Document::AllocatorType& allocator) {
	
	auto conf = rapidjson::Value(rapidjson::kObjectType);

	auto i = ++mr.cbegin(); // skip the whole match
	for (const auto& f : fields) {
		if (i == mr.cend())
			return;
		auto v = (i++)->str();
		if (v.empty() && is_essential(f))
			return;
		conf.AddMember(rapidjson::Value(f, allocator), rapidjson::Value(v, allocator), allocator);
	}
	configs.PushBack(std::move(conf), allocator);
}

rapidjson::Value parsePage(const std::string& s, const std::regex& r, const std::vector<std::string>& fields, rapidjson::Document::AllocatorType& allocator) {
	auto configs = rapidjson::Value(rapidjson::kArrayType);
	try {
		auto it = std::sregex_iterator(s.begin(), s.end(), r);
		auto end = std::sregex_iterator();
		for (; it != end; ++it) {
			fillAccount(*it, fields, configs, allocator);
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
	static auto filename = "gui-config.json";
	auto doc = load(filename);

	auto& allocator = doc.GetAllocator();


	for (const auto& p : pages) {
		auto r = cpr::Get(p.url);
		if (r.status_code != 200)
			continue;


		auto v = parsePage(r.text, p.regex, p.fields, allocator);

		merge(doc["configs"], std::move(v), allocator);
	}

	save(doc, filename);
	return 0;
}
