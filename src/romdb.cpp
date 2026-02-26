#include "romdb.h"
#include <string>
#include <iostream>
#include <vector>
#include <tuple>
#include <algorithm>
#include <cctype>
#include <regex>

#include <curl/curl.h>
#include <sqlite3.h>
#include <tinyxml2.h>

// curl write callback
static size_t writeToString(void* ptr, size_t size, size_t nmemb, void* userdata) {
    size_t total = size * nmemb;
    std::string* s = static_cast<std::string*>(userdata);
    s->append(static_cast<char*>(ptr), total);
    return total;
}

static bool downloadToString(const std::string& url, std::string& out) {
    CURL* curl = curl_easy_init();
    if (!curl) return false;
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeToString);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &out);
    curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);
    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);
    return res == CURLE_OK;
}

static std::string trim(const std::string& s) {
    size_t a = 0;
    while (a < s.size() && std::isspace((unsigned char)s[a])) ++a;
    size_t b = s.size();
    while (b > a && std::isspace((unsigned char)s[b-1])) --b;
    return s.substr(a, b - a);
}

static bool saveEntriesToSqlite(const std::string& dbPath, const std::vector<std::tuple<std::string,std::string,std::string>>& rows) {
    sqlite3* db = nullptr;
    if (sqlite3_open(dbPath.c_str(), &db) != SQLITE_OK) {
        std::cerr << "sqlite3_open failed: " << (db ? sqlite3_errmsg(db) : "(null)") << "\n";
        if (db) sqlite3_close(db);
        return false;
    }

    const char* createSql =
        "CREATE TABLE IF NOT EXISTS roms ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "game TEXT,"
        "description TEXT,"
        "rom_crc TEXT UNIQUE"
        ");";
    char* err = nullptr;
    if (sqlite3_exec(db, createSql, nullptr, nullptr, &err) != SQLITE_OK) {
        std::cerr << "Failed to create table: " << (err ? err : "(unknown)") << "\n";
        sqlite3_free(err);
        sqlite3_close(db);
        return false;
    }

    const char* insertSql = "INSERT OR IGNORE INTO roms (game, description, rom_crc) VALUES (?, ?, ?);";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, insertSql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "sqlite3_prepare_v2 failed: " << sqlite3_errmsg(db) << "\n";
        sqlite3_close(db);
        return false;
    }

    for (const auto& r : rows) {
        const std::string& game = std::get<0>(r);
        const std::string& desc = std::get<1>(r);
        const std::string& crc = std::get<2>(r);
        sqlite3_bind_text(stmt, 1, game.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, desc.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 3, crc.c_str(), -1, SQLITE_TRANSIENT);
        if (sqlite3_step(stmt) != SQLITE_DONE) {
            std::cerr << "Failed to insert row: " << sqlite3_errmsg(db) << "\n";
        }
        sqlite3_reset(stmt);
        sqlite3_clear_bindings(stmt);
    }

    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return true;
}

static std::vector<std::tuple<std::string,std::string,std::string>> parseDatCustom(const std::string& dat) {
    std::vector<std::tuple<std::string,std::string,std::string>> rows;
    std::regex gameRegex(R"DELIM(\bgame\s*\(\s*name\s*"([^"]*)"\s*[\s\S]*?description\s*"([^"]*)"\s*[\s\S]*?rom\s*\([\s\S]*?crc\s*([0-9A-Fa-f]{8})[\s\S]*?\))DELIM", std::regex::icase);
    std::smatch match;
    auto searchStart = dat.cbegin();
    
    while (std::regex_search(searchStart, dat.cend(), match, gameRegex)) {
        std::string name = match[1].str();
        std::string desc = match[2].str();
        std::string crc = match[3].str();
        std::transform(crc.begin(), crc.end(), crc.begin(), [](unsigned char c){ return std::toupper(c); });
        rows.emplace_back(name, desc, crc);
        searchStart = match.suffix().first;
    }
    
    return rows;
}

bool RomDB::init(const std::string& url, const std::string& dbPath) {
    std::cout << "RomDB: downloading DAT from " << url << "\n";
    std::string content;
    if (!downloadToString(url, content)) {
        std::cerr << "RomDB: failed to download DAT\n";
        return false;
    }
    std::cout << "RomDB: parse DAT content...\n";

    auto rows = parseDatCustom(content); // new custom DAT parser

    std::cout << "RomDB: parsed " << rows.size() << " rom entries\n";
    if (!saveEntriesToSqlite(dbPath, rows)) {
        std::cerr << "RomDB: failed to save to sqlite\n";
        return false;
    }
    std::cout << "RomDB: saved DB to " << dbPath << "\n";
    return true;
}