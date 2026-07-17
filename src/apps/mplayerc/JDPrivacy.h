#pragma once
// JDPrivacy.h - JD Privacy V28 filename codec for the MPC-BE fork.
// Matches the JDownloader "JD Privacy" V28 scripts exactly:
//   PRIVACY_KEY      = JD_PRIVACY_TEST_KEY_CHANGE_ME  (overridable, see below)
//   SCRAMBLE_VERSION = v16_actual_video_safe_cycle
//   marker           = U+00A7 at the end of the stem, no preceding space
// Basename codec: FNV-1a seeded per-token Caesar shifts, suffix peeling,
// cycle-walking to avoid reserved-token collisions. Extension codec: ROT+1
// over lowercase letters and digits, exact allow-list maps only.
// Key/version can be overridden without recompiling via a "privacy.key" file
// (line 1 = key, optional line 2 = version) placed next to the executable,
// or at %APPDATA%\MPC-BE\privacy.key (exe-dir wins).

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <algorithm>
#include <cstdint>
#include <cstdio>

namespace JDPrivacy {

// Fork version. Bumped with every change so the running build is identifiable
// (shown in Help > About).
inline const wchar_t* FORK_VERSION = L"2.8";

inline const wchar_t* DEFAULT_KEY     = L"JD_PRIVACY_TEST_KEY_CHANGE_ME";
inline const wchar_t* DEFAULT_VERSION = L"v16_actual_video_safe_cycle";
inline constexpr wchar_t MARKER       = L'\u00A7';
inline constexpr int    MAX_WALK      = 256;

struct ExtPair { const wchar_t* enc; const wchar_t* dec; };

inline const std::vector<ExtPair>& VideoExtMap()
{
	static const std::vector<ExtPair> v = {
		{ L"nq5", L"mp4" },
		{ L"n5w", L"m4v" },
		{ L"nlw", L"mkv" },
		{ L"npw", L"mov" },
		{ L"bwj", L"avi" },
		{ L"xnw", L"wmv" },
		{ L"xfcn", L"webm" },
		{ L"nqh", L"mpg" },
		{ L"nqfh", L"mpeg" },
		{ L"nqf", L"mpe" },
		{ L"nqw", L"mpv" },
		{ L"n2w", L"m1v" },
		{ L"n3w", L"m2v" },
		{ L"n3q", L"m2p" },
		{ L"ut", L"ts" },
		{ L"n3ut", L"m2ts" },
		{ L"nut", L"mts" },
		{ L"usq", L"trp" },
		{ L"wsp", L"vro" },
		{ L"upe", L"tod" },
		{ L"gmw", L"flv" },
		{ L"g5w", L"f4v" },
		{ L"4hq", L"3gp" },
		{ L"4h3", L"3g2" },
		{ L"phw", L"ogv" },
		{ L"phn", L"ogm" },
		{ L"i375", L"h264" },
		{ L"i376", L"h265" },
		{ L"375", L"264" },
		{ L"376", L"265" },
		{ L"ifwd", L"hevc" },
		{ L"bw2", L"av1" },
		{ L"jwg", L"ivf" },
		{ L"pcv", L"obu" },
		{ L"ew", L"dv" },
		{ L"wpc", L"vob" },
		{ L"ejwy", L"divx" },
		{ L"ywje", L"xvid" },
		{ L"sn", L"rm" },
		{ L"snwc", L"rmvb" },
		{ L"btg", L"asf" },
		{ L"ru", L"qt" },
		{ L"nyg", L"mxf" },
		{ L"hyg", L"gxf" },
		{ L"xuw", L"wtv" },
		{ L"ews-nt", L"dvr-ms" },
		{ L"bnw", L"amv" },
		{ L"z5n", L"y4m" },
		{ L"nkqh", L"mjpg" },
		{ L"nkqfh", L"mjpeg" },
		{ L"cjl", L"bik" },
		{ L"tnl", L"smk" },
		{ L"otw", L"nsv" },
		{ L"spr", L"roq" },
		{ L"gmj", L"fli" },
		{ L"gmd", L"flc" },
		{ L"ovu", L"nut" },
		{ L"sfd", L"rec" },
		{ L"npe", L"mod" },
	};
	return v;
}

inline const std::vector<ExtPair>& SubExtMap()
{
	static const std::vector<ExtPair> v = {
		{ L"tsu", L"srt" },
		{ L"wuu", L"vtt" },
		{ L"btt", L"ass" },
		{ L"ttb", L"ssa" },
		{ L"tvc", L"sub" },
		{ L"jey", L"idx" },
		{ L"tvq", L"sup" },
		{ L"tnj", L"smi" },
		{ L"tcw", L"sbv" },
		{ L"uunm", L"ttml" },
		{ L"egyq", L"dfxp" },
	};
	return v;
}

// space-separated encoded video extension list for CMediaFormats registration
inline const wchar_t* EncodedVideoExtSpaceList()
{
	return L"nq5 n5w nlw npw bwj xnw xfcn nqh nqfh nqf nqw n2w n3w n3q ut n3ut nut usq wsp upe gmw g5w 4hq 4h3 phw phn i375 i376 375 376 ifwd bw2 jwg pcv ew wpc ejwy ywje sn snwc btg ru nyg hyg xuw ews-nt bnw z5n nkqh nkqfh cjl tnl otw spr gmj gmd ovu sfd npe";
}

struct Config {
	std::wstring key;
	std::wstring version;
};

inline std::wstring GetExeDirPathForConfig()
{
#ifdef _WIN32
	wchar_t exe[1024] = {};
	if (::GetModuleFileNameW(nullptr, exe, 1024)) {
		std::wstring p(exe);
		const size_t sl = p.find_last_of(L"\\/");
		if (sl != std::wstring::npos) {
			return p.substr(0, sl + 1) + L"privacy.key";
		}
	}
#endif
	return {};
}

inline const Config& GetConfig()
{
	static const Config cfg = [] {
		Config c{ DEFAULT_KEY, DEFAULT_VERSION };
		auto tryRead = [&c](const std::wstring& path) -> bool {
			if (path.empty()) {
				return false;
			}
#ifdef _WIN32
			FILE* f = _wfopen(path.c_str(), L"rt, ccs=UTF-8");
#else
			FILE* f = nullptr;
#endif
			if (!f) {
				return false;
			}
			wchar_t line[512] = {};
			auto trim = [](std::wstring s) {
				const wchar_t* ws = L" \t\r\n";
				const auto b = s.find_first_not_of(ws);
				const auto e = s.find_last_not_of(ws);
				return (b == std::wstring::npos) ? std::wstring() : s.substr(b, e - b + 1);
			};
			if (fgetws(line, 512, f)) {
				std::wstring s = trim(line);
				if (!s.empty()) { c.key = s; }
			}
			if (fgetws(line, 512, f)) {
				std::wstring s = trim(line);
				if (!s.empty()) { c.version = s; }
			}
			fclose(f);
			return true;
		};
		if (!tryRead(GetExeDirPathForConfig())) {
#ifdef _WIN32
			wchar_t buf[1024] = {};
			if (::ExpandEnvironmentStringsW(L"%APPDATA%\\MPC-BE\\privacy.key", buf, 1024)) {
				tryRead(buf);
			}
#endif
		}
		return c;
	}();
	return cfg;
}

inline uint32_t Fnv1a32(const std::wstring& s)
{
	uint32_t h = 2166136261u;
	for (wchar_t c : s) {
		h ^= (uint32_t)(uint16_t)c;
		h *= 16777619u;
	}
	return h;
}

inline unsigned ShiftAt(const std::wstring& seedBase, int pos)
{
	return (Fnv1a32(seedBase + L"|" + std::to_wstring(pos)) % 25u) + 1u;
}

inline bool IsAlnumChar(wchar_t c)
{
	return (c >= L'0' && c <= L'9') || (c >= L'a' && c <= L'z') || (c >= L'A' && c <= L'Z');
}

inline bool IsDigitChar(wchar_t c)
{
	return c >= L'0' && c <= L'9';
}

inline std::wstring LowerAscii(std::wstring s)
{
	for (auto& c : s) {
		if (c >= L'A' && c <= L'Z') { c += 32; }
	}
	return s;
}

inline const std::unordered_set<std::wstring>& ReservedTokens()
{
	static const std::unordered_set<std::wstring> s = {
		L"2160p", L"1440p", L"1080p", L"720p", L"480p", L"360p", L"8k", L"4k", L"uhd", L"fhd", L"hd", L"sd", L"hdr", L"hdr10", L"hdr10plus", L"sdr", L"dv", L"dolby", L"vision", L"web", L"webrip", L"webdl", L"dl", L"bluray", L"blu", L"ray", L"bdrip", L"dvdrip", L"hdtv", L"x264", L"x265", L"h264", L"h265", L"hevc", L"av1", L"aac", L"ac3", L"eac3", L"ddp", L"dd", L"mp3", L"flac", L"opus", L"proper", L"repack", L"remux", L"part", L"cd", L"mp4", L"mkv", L"avi", L"mov", L"wmv", L"m4v", L"webm", L"mpg", L"mpeg", L"mpe", L"mpv", L"m1v", L"m2v", L"ts", L"m2ts", L"mts", L"m2p", L"flv", L"f4v", L"3gp", L"3g2", L"ogv", L"ogg", L"ogm", L"vob", L"ifo", L"vro", L"mod", L"tod", L"dat", L"264", L"265", L"divx", L"xvid", L"rm", L"rmvb", L"asf", L"qt", L"mxf", L"amv", L"y4m", L"bik", L"smk", L"trp", L"rec", L"nut", L"rar", L"zip", L"7z", L"iso"
	};
	return s;
}

inline const std::vector<std::wstring>& ReservedSuffixes()
{
	// longest-first
	static const std::vector<std::wstring> s = { L"hdr10plus", L"webrip", L"bluray", L"dvdrip", L"2160p", L"1440p", L"1080p", L"hdr10", L"webdl", L"bdrip", L"720p", L"480p", L"360p", L"x264", L"x265", L"h264", L"h265", L"hevc", L"hdtv", L"uhd", L"fhd", L"hdr", L"sdr", L"av1", L"4k", L"8k" };
	return s;
}

inline bool IsReservedToken(const std::wstring& t)
{
	if (t.empty()) {
		return true;
	}
	if (std::all_of(t.begin(), t.end(), IsDigitChar)) {
		return true;
	}
	return ReservedTokens().count(LowerAscii(t)) != 0;
}

inline bool EndsWithSuffixWithPrefix(const std::wstring& t)
{
	const std::wstring low = LowerAscii(t);
	for (const auto& s : ReservedSuffixes()) {
		if (low.size() > s.size() && low.compare(low.size() - s.size(), s.size(), s) == 0) {
			return true;
		}
	}
	return false;
}

inline bool IsInvalidCore(const std::wstring& t)
{
	return IsReservedToken(t) || EndsWithSuffixWithPrefix(t);
}

inline void PeelSuffixes(const std::wstring& t, std::wstring& core, std::wstring& suffix)
{
	core = t;
	suffix.clear();
	bool again = true;
	while (again) {
		again = false;
		const std::wstring low = LowerAscii(core);
		for (const auto& s : ReservedSuffixes()) {
			if (low.size() > s.size() && low.compare(low.size() - s.size(), s.size(), s) == 0) {
				suffix = core.substr(core.size() - s.size()) + suffix;
				core = core.substr(0, core.size() - s.size());
				again = true;
				break;
			}
		}
	}
}

inline wchar_t ShiftChar(wchar_t c, int s)
{
	auto pmod = [](int n, int m) { return ((n % m) + m) % m; };
	if (c >= L'a' && c <= L'z') { return (wchar_t)(L'a' + pmod(c - L'a' + s, 26)); }
	if (c >= L'A' && c <= L'Z') { return (wchar_t)(L'A' + pmod(c - L'A' + s, 26)); }
	if (c >= L'0' && c <= L'9') { return (wchar_t)(L'0' + pmod(c - L'0' + s, 10)); }
	return c;
}

inline std::wstring RawTransform(const std::wstring& token, bool decode)
{
	if (token.size() <= 1) {
		return token;
	}
	const Config& cfg = GetConfig();
	std::wstring seed = cfg.key + L"|" + cfg.version + L"|" + token[0] + L"|" + std::to_wstring((int)token.size());
	std::wstring out(1, token[0]);
	for (size_t i = 1; i < token.size(); i++) {
		const int s = (int)ShiftAt(seed, (int)i);
		out += ShiftChar(token[i], decode ? -s : s);
	}
	return out;
}

inline std::wstring WalkCore(const std::wstring& core, bool decode)
{
	if (core.size() <= 1 || IsReservedToken(core)) {
		return core;
	}
	std::wstring out = RawTransform(core, decode);
	int n = 0;
	while (IsInvalidCore(out) && n < MAX_WALK) {
		out = RawTransform(out, decode);
		n++;
	}
	return out;
}

inline std::wstring TransformToken(const std::wstring& t, bool decode)
{
	if (t.size() <= 1 || IsReservedToken(t)) {
		return t;
	}
	std::wstring core, suffix;
	PeelSuffixes(t, core, suffix);
	return WalkCore(core, decode) + suffix;
}

inline std::wstring TransformText(const std::wstring& text, bool decode)
{
	std::wstring out;
	size_t i = 0;
	while (i < text.size()) {
		if (IsAlnumChar(text[i])) {
			const size_t st = i;
			while (i < text.size() && IsAlnumChar(text[i])) { i++; }
			out += TransformToken(text.substr(st, i - st), decode);
		} else {
			out += text[i++];
		}
	}
	return out;
}

inline std::wstring RotExt(const std::wstring& ext, int dir)
{
	std::wstring out;
	for (wchar_t c : ext) {
		out += ShiftChar(c, dir);
	}
	return out;
}

// Returns decoded original extension (no dot) for an encoded one, empty if unknown.
inline std::wstring DecodeMediaExt(const std::wstring& encLower, bool* pIsSubtitle = nullptr)
{
	for (const auto& p : VideoExtMap()) {
		if (encLower == p.enc) {
			if (pIsSubtitle) { *pIsSubtitle = false; }
			return p.dec;
		}
	}
	for (const auto& p : SubExtMap()) {
		if (encLower == p.enc) {
			if (pIsSubtitle) { *pIsSubtitle = true; }
			return p.dec;
		}
	}
	return {};
}

// True if the filename (no path) is a JD Privacy marked media file:
// stem ends with the marker and the final extension is in the encoded maps.
inline bool IsPrivacyMarkedMedia(const std::wstring& fileName)
{
	const size_t dot = fileName.find_last_of(L'.');
	if (dot == std::wstring::npos || dot == 0 || dot + 1 >= fileName.size()) {
		return false;
	}
	if (fileName[dot - 1] != MARKER) {
		return false;
	}
	return !DecodeMediaExt(LowerAscii(fileName.substr(dot + 1))).empty();
}

// For a marked media file: the decoded logical extension (no dot), else empty.
inline std::wstring GetLogicalExtension(const std::wstring& fileName)
{
	const size_t dot = fileName.find_last_of(L'.');
	if (dot == std::wstring::npos || dot == 0 || dot + 1 >= fileName.size() || fileName[dot - 1] != MARKER) {
		return {};
	}
	return DecodeMediaExt(LowerAscii(fileName.substr(dot + 1)));
}

// Display decode for playlist labels etc.
// Rule: split at the LAST marker. Decode the part before it with the basename
// codec; keep the remainder verbatim except a directly-following ".encodedext"
// from the allow-list, which becomes the decoded extension. Unmarked names are
// returned unchanged.
inline std::wstring DecodeDisplayNameUncached(const std::wstring& name)
{
	const size_t m = name.find_last_of(MARKER);
	if (m == std::wstring::npos) {
		return name;
	}
	const std::wstring prefix = name.substr(0, m);
	std::wstring rest = name.substr(m + 1);
	std::wstring out = TransformText(prefix, true);
	if (rest.size() > 1 && rest[0] == L'.') {
		const std::wstring dec = DecodeMediaExt(LowerAscii(rest.substr(1)));
		if (!dec.empty()) {
			return out + L"." + dec;
		}
	}
	return out + rest;
}

inline std::wstring DecodeDisplayName(const std::wstring& name)
{
	static std::unordered_map<std::wstring, std::wstring> cache; // UI thread only
	auto it = cache.find(name);
	if (it != cache.end()) {
		return it->second;
	}
	if (cache.size() > 4096) {
		cache.clear();
	}
	std::wstring dec = DecodeDisplayNameUncached(name);
	cache.emplace(name, dec);
	return dec;
}

// Decode a display string that may be a bare filename OR a full path.
// Only the filename component is decoded - directory names are left alone,
// otherwise the basename codec would mangle the folders too.
inline std::wstring DecodeDisplayPathOrName(const std::wstring& s)
{
	const size_t sl = s.find_last_of(L"\\/");
	if (sl == std::wstring::npos) {
		return DecodeDisplayName(s);
	}
	return s.substr(0, sl + 1) + DecodeDisplayName(s.substr(sl + 1));
}

// Real (decoded) uppercase extension for the Type column.
inline std::wstring RealTypeUpper(const std::wstring& fileName)
{
	std::wstring ext;
	const std::wstring logical = GetLogicalExtension(fileName);
	if (!logical.empty()) {
		ext = logical;
	} else {
		const size_t dot = fileName.find_last_of(L'.');
		if (dot != std::wstring::npos && dot + 1 < fileName.size()) {
			ext = fileName.substr(dot + 1);
		}
	}
	for (auto& c : ext) {
		if (c >= L'a' && c <= L'z') { c -= 32; }
	}
	return ext;
}

} // namespace JDPrivacy
