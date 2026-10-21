#pragma once
// In-game settings pages for the CS Water Mod SKSE plugins (WaterWind, WaterWade) in SKSE Menu Framework.
//
// The framework is found by name at run time (the same exports its public consumer header uses: AddSectionItem plus the cimgui "ig*"
// functions), so the plugins have no link-time dependency on it and keep working without it: the ini file stays the source of truth.
// Every change is written straight into the plugin's ini (a value is replaced in place, comments and order are kept). The plugins re-read
// their ini about once a second, so a change shows within a second or two. Under Mod Organizer 2 the written file lands in the Overwrite
// folder, like any other file a game plugin writes.
//
// Used by E:/WorkSpace/WaterWind and E:/WorkSpace/WaterWade (INCLUDES "E:/WorkSpace/shared").

#include <Windows.h>

#include <algorithm>
#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace CswMenu
{
	namespace Smf
	{
		struct ImVec2
		{
			float x, y;
		};
		using RenderFunction = void(__stdcall*)();

		inline HMODULE Module()
		{
			static HMODULE module = nullptr;
			if (!module) module = ::GetModuleHandleW(L"SKSEMenuFramework");
			return module;
		}

		template <class T>
		T Fn(const char* a_name)
		{
			auto m = Module();
			return m ? reinterpret_cast<T>(::GetProcAddress(m, a_name)) : nullptr;
		}

		inline bool AddSectionItem(const char* a_path, RenderFunction a_render)
		{
			static auto f = Fn<void (*)(const char*, RenderFunction)>("AddSectionItem");
			if (!f) return false;
			f(a_path, a_render);
			return true;
		}

		inline float Version()
		{
			static auto f = Fn<float (*)()>("GetMenuFrameworkVersion");
			return f ? f() : 0.0f;
		}

		inline void TextV(const char* a_name, const char* a_fmt, std::va_list a_args)
		{
			auto f = Fn<void (*)(const char*, std::va_list)>(a_name);
			if (f) f(a_fmt, a_args);
		}
		inline void Text(const char* a_fmt, ...)
		{
			std::va_list args;
			va_start(args, a_fmt);
			TextV("igTextV", a_fmt, args);
			va_end(args);
		}
		inline void TextWrapped(const char* a_fmt, ...)
		{
			std::va_list args;
			va_start(args, a_fmt);
			TextV("igTextWrappedV", a_fmt, args);
			va_end(args);
		}
		inline void TextDisabled(const char* a_fmt, ...)
		{
			std::va_list args;
			va_start(args, a_fmt);
			TextV("igTextDisabledV", a_fmt, args);
			va_end(args);
		}

		inline bool SliderFloat(const char* a_label, float* a_v, float a_min, float a_max, const char* a_format)
		{
			static auto f = Fn<bool (*)(const char*, float*, float, float, const char*, int)>("igSliderFloat");
			return f && f(a_label, a_v, a_min, a_max, a_format, 0);
		}
		inline bool SliderInt(const char* a_label, int* a_v, int a_min, int a_max)
		{
			static auto f = Fn<bool (*)(const char*, int*, int, int, const char*, int)>("igSliderInt");
			return f && f(a_label, a_v, a_min, a_max, "%d", 0);
		}
		inline bool Checkbox(const char* a_label, bool* a_v)
		{
			static auto f = Fn<bool (*)(const char*, bool*)>("igCheckbox");
			return f && f(a_label, a_v);
		}
		inline bool Button(const char* a_label)
		{
			static auto f = Fn<bool (*)(const char*, ImVec2)>("igButton");
			return f && f(a_label, ImVec2{ 0.0f, 0.0f });
		}
		/// True once, right after the item drawn just before was let go after being edited (a slider drag ending). Null when the
		/// framework does not export it; callers then write on every change.
		inline bool HasDeactivatedAfterEdit()
		{
			static auto f = Fn<bool (*)()>("igIsItemDeactivatedAfterEdit");
			return f != nullptr;
		}
		inline bool ItemDeactivatedAfterEdit()
		{
			static auto f = Fn<bool (*)()>("igIsItemDeactivatedAfterEdit");
			return f && f();
		}
		inline void SameLine()
		{
			static auto f = Fn<void (*)(float, float)>("igSameLine");
			if (f) f(0.0f, -1.0f);
		}
		inline void Separator()
		{
			static auto f = Fn<void (*)()>("igSeparator");
			if (f) f();
		}
		inline void SeparatorText(const char* a_label)
		{
			static auto f = Fn<void (*)(const char*)>("igSeparatorText");
			if (f) f(a_label);
			else Text("%s", a_label);
		}
	}

	// ---- minimal ini access: sections, key=value, ';' comments; keys compared without case --------------------------------------

	inline std::string Lower(std::string s)
	{
		std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
		return s;
	}

	inline std::string Trim(const std::string& s)
	{
		const auto a = s.find_first_not_of(" \t\r\n");
		if (a == std::string::npos) return {};
		return s.substr(a, s.find_last_not_of(" \t\r\n") - a + 1);
	}

	/// "section/key" (both lower case) -> value, for every setting in the file.
	inline std::map<std::string, std::string> IniRead(const std::filesystem::path& a_path)
	{
		std::map<std::string, std::string> out;
		std::ifstream                      in(a_path);
		std::string                        line, section;
		while (std::getline(in, line)) {
			const auto t = Trim(line);
			if (t.empty() || t[0] == ';' || t[0] == '#') continue;
			if (t[0] == '[') {
				const auto e = t.find(']');
				section = Lower(t.substr(1, e == std::string::npos ? std::string::npos : e - 1));
				continue;
			}
			const auto eq = t.find('=');
			if (eq == std::string::npos) continue;
			out[section + "/" + Lower(Trim(t.substr(0, eq)))] = Trim(t.substr(eq + 1));
		}
		return out;
	}

	/// Replaces the value of key in section in place (comments and order kept). A missing key is added at the end of its section, a
	/// missing section at the end of the file. Written to a temporary file first, then moved over the ini.
	inline bool IniSet(const std::filesystem::path& a_path, const std::string& a_section, const std::string& a_key, const std::string& a_value)
	{
		std::vector<std::string> lines;
		{
			std::ifstream in(a_path, std::ios::binary);
			std::string   l;
			while (std::getline(in, l)) {
				if (!l.empty() && l.back() == '\r') l.pop_back();
				lines.push_back(l);
			}
		}
		const auto wantSection = Lower(a_section);
		const auto wantKey = Lower(a_key);
		std::string section;
		int         sectionStart = -1, sectionEnd = -1, found = -1;
		for (int i = 0; i < static_cast<int>(lines.size()); ++i) {
			const auto t = Trim(lines[i]);
			if (!t.empty() && t[0] == '[') {
				const auto e = t.find(']');
				section = Lower(t.substr(1, e == std::string::npos ? std::string::npos : e - 1));
				if (section == wantSection && sectionStart < 0) sectionStart = i;
				continue;
			}
			if (section == wantSection) {
				if (!t.empty() && t[0] != ';' && t[0] != '#') {
					const auto eq = t.find('=');
					if (eq != std::string::npos && Lower(Trim(t.substr(0, eq))) == wantKey && found < 0) found = i;
					sectionEnd = i;
				}
			}
		}
		const auto entry = a_key + "=" + a_value;
		if (found >= 0) {
			lines[found] = entry;
		} else if (sectionStart >= 0) {
			lines.insert(lines.begin() + (sectionEnd >= 0 ? sectionEnd + 1 : sectionStart + 1), entry);
		} else {
			if (!lines.empty() && !Trim(lines.back()).empty()) lines.emplace_back();
			lines.push_back("[" + a_section + "]");
			lines.push_back(entry);
		}
		auto tmp = a_path;
		tmp += ".tmp";
		{
			std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
			if (!out) return false;
			for (const auto& l : lines) out << l << "\r\n";
			if (!out.good()) return false;
		}
		std::error_code ec;
		std::filesystem::rename(tmp, a_path, ec);
		if (ec) {
			// rename over an existing file can fail while something has it open: fall back to a plain overwrite
			std::ofstream out(a_path, std::ios::binary | std::ios::trunc);
			if (!out) return false;
			for (const auto& l : lines) out << l << "\r\n";
			std::filesystem::remove(tmp, ec);
			return out.good();
		}
		return true;
	}

	// ---- a page of settings ---------------------------------------------------------------------------------------------------

	struct Entry
	{
		enum class Kind { Bool, Float, Int };
		Kind        kind{ Kind::Float };
		const char* section{};
		const char* key{};
		const char* label{};
		const char* tip{};       // one line of help under the control (may be null)
		float       min{ 0 }, max{ 1 };
		const char* format{ "%.2f" };
		float       def{ 0 };    // value of "Restore defaults"
		float       value{ 0 };  // current value (bool: 0 or 1)
	};

	class Page
	{
	public:
		Page(std::filesystem::path a_ini, std::vector<Entry> a_entries, std::string a_intro) :
			_ini(std::move(a_ini)), _entries(std::move(a_entries)), _intro(std::move(a_intro)) {}

		void Render()
		{
			using clock = std::chrono::steady_clock;
			const auto now = clock::now();
			if (!_loaded || (now - _lastLoad > std::chrono::seconds(2) && now - _lastEdit > std::chrono::seconds(3))) Load();

			Smf::TextWrapped("%s", _intro.c_str());
			if (!std::filesystem::exists(_ini)) Smf::TextDisabled("%s was not found; the plugin runs on its built-in values until a setting is changed here.", _ini.filename().string().c_str());
			const char* last = nullptr;
			for (auto& e : _entries) {
				if (!last || std::string_view(last) != e.section) {
					Smf::SeparatorText(e.section);
					last = e.section;
				}
				const auto label = std::string(e.label) + "##" + e.section + "_" + e.key;
				bool       changed = false, release = false;
				if (e.kind == Entry::Kind::Bool) {
					bool b = e.value >= 0.5f;
					if (Smf::Checkbox(label.c_str(), &b)) {
						e.value = b ? 1.0f : 0.0f;
						changed = release = true;
					}
				} else if (e.kind == Entry::Kind::Int) {
					int i = static_cast<int>(e.value + (e.value >= 0 ? 0.5f : -0.5f));
					if (Smf::SliderInt(label.c_str(), &i, static_cast<int>(e.min), static_cast<int>(e.max))) {
						e.value = static_cast<float>(i);
						changed = true;
					}
					release = Smf::ItemDeactivatedAfterEdit();
				} else {
					float v = e.value;
					if (Smf::SliderFloat(label.c_str(), &v, e.min, e.max, e.format)) {
						e.value = v;
						changed = true;
					}
					release = Smf::ItemDeactivatedAfterEdit();
				}
				if (changed) {
					_lastEdit = now;
					_dirty.insert(&e - _entries.data());
				}
				// write when the drag is let go, or at once when the framework cannot tell (or for checkboxes)
				if ((release || (changed && !Smf::HasDeactivatedAfterEdit())) && !_dirty.empty()) Flush();
				if (e.tip && *e.tip) Smf::TextDisabled("    %s", e.tip);
			}
			Smf::Separator();
			if (Smf::Button("Restore defaults")) {
				for (auto& e : _entries) {
					e.value = e.def;
					WriteEntry(e);
				}
				_status = "Defaults written.";
				_lastEdit = now;
			}
			if (!_status.empty()) Smf::Text("%s", _status.c_str());
			Smf::TextDisabled("Changes are saved to %s and applied within a second or two.", _ini.filename().string().c_str());
		}

	private:
		static std::string Format(const Entry& e)
		{
			if (e.kind == Entry::Kind::Bool) return e.value >= 0.5f ? "1" : "0";
			if (e.kind == Entry::Kind::Int) return std::to_string(static_cast<int>(e.value + (e.value >= 0 ? 0.5f : -0.5f)));
			char buf[48];
			std::snprintf(buf, sizeof buf, "%.4g", static_cast<double>(e.value));
			return buf;
		}

		void WriteEntry(const Entry& e)
		{
			if (!IniSet(_ini, e.section, e.key, Format(e))) _status = "Could not write the ini file (see the plugin log).";
			else _status.clear();
		}

		void Flush()
		{
			for (auto idx : _dirty) WriteEntry(_entries[idx]);
			_dirty.clear();
		}

		void Load()
		{
			const auto values = IniRead(_ini);
			for (auto& e : _entries) {
				const auto it = values.find(Lower(e.section) + "/" + Lower(e.key));
				if (it == values.end()) continue;
				const auto& s = it->second;
				if (e.kind == Entry::Kind::Bool) e.value = (s != "0" && Lower(s) != "false") ? 1.0f : 0.0f;
				else e.value = static_cast<float>(std::atof(s.c_str()));
			}
			_loaded = true;
			_lastLoad = std::chrono::steady_clock::now();
		}

		std::filesystem::path                 _ini;
		std::vector<Entry>                    _entries;
		std::string                           _intro;
		std::string                           _status;
		std::set<std::ptrdiff_t>              _dirty;
		bool                                  _loaded{ false };
		std::chrono::steady_clock::time_point _lastLoad{}, _lastEdit{};
	};
}
