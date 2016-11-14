#include <iostream>
#include <regex>
#include <unordered_map>
#include <cstdio>
#include <tuple>
#include <unordered_set>
#include <cctype>

#include <cpr/cpr.h>
#include <rapidjson/document.h>
#include <rapidjson/filereadstream.h>
#include <rapidjson/filewritestream.h>
#include <rapidjson/prettywriter.h>

typedef rapidjson::Document::AllocatorType Allocator;

std::string toString(const rapidjson::Value& v) {
	return v.IsString() ? std::string(v.GetString(), v.GetStringLength()) : std::string();
}


struct Page {
	Page(std::string url, std::string regex, std::initializer_list<std::string> fields) : url(url), regex(regex), fields(fields) {
		
	}
	Page(std::string url, std::string regex, std::vector<std::string> fields) : url(url), regex(regex), fields(fields) {

	}
	cpr::Url url;
	std::regex regex;
	std::vector<std::string> fields;
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
	auto s = toString(conf["method"]);
	if (s.empty())
		return;

	std::transform(s.begin(), s.end(), s.begin(), tolower);
	conf["method"].SetString(s, allocator);

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

rapidjson::Document load(const char* filename, rapidjson::Type type) {
	auto fp = openf(filename, "rt");
	rapidjson::Document d(type);

	if (fp) {
		char readBuffer[65536];
		rapidjson::FileReadStream is(fp.get(), readBuffer, sizeof(readBuffer));
		d.ParseStream(is);
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


std::vector<Page> loadPagesConfig(const char* filename) {
	auto d = load(filename, rapidjson::kArrayType);
	if (!d.IsArray())
		return std::vector<Page>();

	std::vector<Page> pages;
	for (auto& p : d.GetArray()) {
		if (!p.IsObject())
			continue;
		auto url = toString(p["url"]);
		auto regex = toString(p["regex"]);

		if (url.empty() || regex.empty())
			continue;

		std::vector<std::string> fields;
		if (!p.HasMember("fields")) {
			fields = {
				"server",
				"server_port",
				"password",
				"method"
			};
		}
		else {
			for (auto& field : p["fields"].GetArray()) {
				if (field.IsString()) {
					fields.emplace_back(field.GetString(), field.GetStringLength());
				}
			}
			if (fields.size() < 4)
				continue;
		}
		pages.emplace_back(url, regex, fields);
	}
	return pages;
}

int main()
{
	static auto filename = "gui-config.json";
	auto doc = load(filename, rapidjson::kObjectType);
	auto& allocator = doc.GetAllocator();

	if (!doc.HasMember("configs")) {
		doc.AddMember("configs", rapidjson::Value(rapidjson::kArrayType), allocator);
	}

	for (const auto& p : loadPagesConfig("FreeShadowsocks.json")) {
		auto r = cpr::Get(p.url);
		if (r.status_code != 200) {
			std::cerr << r.status_code << " " << p.url << std::endl;
			continue;
		}

		auto v = parsePage(r.text, p.regex, p.fields, allocator);
		std::cout << v.Size() << " accounts got form " << p.url << std::endl;
		merge(doc["configs"], std::move(v), allocator);
	}

	save(doc, filename);
	return 0;
}
