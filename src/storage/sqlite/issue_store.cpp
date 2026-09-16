#include "issuetrace/issue_store.hpp"

#include <sqlite3.h>

#include <array>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <random>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace issuetrace {
namespace {

constexpr int kSchemaVersion = 8;

class Statement final {
public:
    Statement(sqlite3* db, const char* sql) {
        if (sqlite3_prepare_v2(db, sql, -1, &statement_, nullptr) != SQLITE_OK) {
            throw std::runtime_error(sqlite3_errmsg(db));
        }
    }
    ~Statement() { sqlite3_finalize(statement_); }
    Statement(const Statement&) = delete;
    Statement& operator=(const Statement&) = delete;
    [[nodiscard]] sqlite3_stmt* get() const { return statement_; }

private:
    sqlite3_stmt* statement_{};
};

void execute(sqlite3* db, const char* sql) {
    char* error = nullptr;
    if (sqlite3_exec(db, sql, nullptr, nullptr, &error) != SQLITE_OK) {
        const std::string message = error ? error : "SQLite operation failed";
        sqlite3_free(error);
        throw std::runtime_error(message);
    }
}

bool columnExists(sqlite3* db, const char* table, const char* column) {
    const std::string sql = "SELECT 1 FROM pragma_table_info(?) WHERE name=?";
    Statement statement(db, sql.c_str());
    sqlite3_bind_text(statement.get(), 1, table, -1, SQLITE_STATIC);
    sqlite3_bind_text(statement.get(), 2, column, -1, SQLITE_STATIC);
    return sqlite3_step(statement.get()) == SQLITE_ROW;
}

std::int64_t nowMilliseconds() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

std::string createUuid() {
    std::array<unsigned char, 16> bytes{};
    std::random_device source;
    for (auto& byte : bytes) byte = static_cast<unsigned char>(source());
    bytes[6] = static_cast<unsigned char>((bytes[6] & 0x0fU) | 0x40U);
    bytes[8] = static_cast<unsigned char>((bytes[8] & 0x3fU) | 0x80U);
    constexpr char hex[] = "0123456789abcdef";
    std::string result;
    result.reserve(36);
    for (std::size_t i = 0; i < bytes.size(); ++i) {
        if (i == 4 || i == 6 || i == 8 || i == 10) result.push_back('-');
        result.push_back(hex[bytes[i] >> 4U]);
        result.push_back(hex[bytes[i] & 0x0fU]);
    }
    return result;
}

std::string columnText(sqlite3_stmt* statement, const int column) {
    const auto* value = sqlite3_column_text(statement, column);
    return value ? reinterpret_cast<const char*>(value) : std::string{};
}

std::string safeExtension(const std::string& originalName) {
    const auto dot = originalName.find_last_of('.');
    if (dot == std::string::npos || dot + 1 == originalName.size()) return {};
    std::string extension;
    for (std::size_t i = dot + 1; i < originalName.size() && extension.size() < 12;
         ++i) {
        const auto value = static_cast<unsigned char>(originalName[i]);
        if ((value >= 'a' && value <= 'z') || (value >= 'A' && value <= 'Z') ||
            (value >= '0' && value <= '9')) {
            extension.push_back(static_cast<char>(value));
        } else {
            return {};
        }
    }
    return extension.empty() ? std::string{} : "." + extension;
}

StoredIssue readIssue(sqlite3_stmt* statement) {
    StoredIssue issue;
    issue.id = columnText(statement, 0);
    issue.title = columnText(statement, 1);
    issue.originalProblem = columnText(statement, 2);
    issue.reporter = columnText(statement, 3);
    issue.assignee = columnText(statement, 4);
    issue.service = columnText(statement, 5);
    issue.version = columnText(statement, 6);
    issue.ticket = columnText(statement, 7);
    issue.status = columnText(statement, 8);
    issue.priority = columnText(statement, 9);
    issue.conclusion = columnText(statement, 10);
    issue.reportedAt = sqlite3_column_int64(statement, 11);
    if (sqlite3_column_type(statement, 12) != SQLITE_NULL) {
        issue.resolvedAt = sqlite3_column_int64(statement, 12);
    }
    issue.createdAt = sqlite3_column_int64(statement, 13);
    issue.updatedAt = sqlite3_column_int64(statement, 14);
    issue.statusChangedAt = sqlite3_column_int64(statement, 15);
    if (sqlite3_column_type(statement, 16) != SQLITE_NULL) {
        issue.remindAt = sqlite3_column_int64(statement, 16);
    }
    issue.groupName = columnText(statement, 17);
    issue.tags = columnText(statement, 18);
    issue.trackedMilliseconds = sqlite3_column_int64(statement, 19);
    if (sqlite3_column_type(statement, 20) != SQLITE_NULL) {
        issue.timerStartedAt = sqlite3_column_int64(statement, 20);
    }
    return issue;
}

constexpr const char* issueColumns =
    "id,title,original_problem,reporter,assignee,service,version,ticket,status,"
    "priority,conclusion,reported_at,resolved_at,created_at,updated_at,"
    "status_changed_at,remind_at,group_name,tags,tracked_milliseconds,timer_started_at";

void bindText(sqlite3_stmt* statement, const int index,
              const std::string& value) {
    if (sqlite3_bind_text(statement, index, value.c_str(),
                          static_cast<int>(value.size()), SQLITE_TRANSIENT) !=
        SQLITE_OK) {
        throw std::runtime_error("Cannot bind SQLite text value");
    }
}

void refreshSearchDocument(sqlite3* db, const std::string& issueId) {
    Statement remove(db, "DELETE FROM issue_search WHERE issue_id=?");
    bindText(remove.get(), 1, issueId);
    if (sqlite3_step(remove.get()) != SQLITE_DONE) {
        throw std::runtime_error(sqlite3_errmsg(db));
    }
    Statement insert(db,
        "INSERT INTO issue_search(issue_id,content) "
        "SELECT i.id,i.title||' '||i.original_problem||' '||i.reporter||' '||"
        "i.assignee||' '||i.service||' '||i.version||' '||i.ticket||' '||"
        "i.group_name||' '||i.tags||' '||"
        "i.conclusion||' '||COALESCE((SELECT group_concat(t.content_markdown,' ') "
        "FROM timeline_entries t WHERE t.issue_id=i.id AND t.deleted_at IS NULL),'')||"
        "' '||COALESCE((SELECT group_concat(a.original_name,' ') FROM attachments a "
        "JOIN timeline_entries t ON t.id=a.timeline_entry_id WHERE t.issue_id=i.id "
        "AND t.deleted_at IS NULL AND a.deleted_at IS NULL),'') "
        "||' '||COALESCE((SELECT s.content_markdown FROM summary_drafts s "
        "WHERE s.issue_id=i.id),'') "
        "FROM issues i WHERE i.id=? AND i.deleted_at IS NULL");
    bindText(insert.get(), 1, issueId);
    if (sqlite3_step(insert.get()) != SQLITE_DONE) {
        throw std::runtime_error(sqlite3_errmsg(db));
    }
}

std::string quoteFtsQuery(const std::string& text) {
    std::string result{"\""};
    for (const char value : text) {
        if (value == '"') result += "\"\"";
        else result.push_back(value);
    }
    result.push_back('"');
    return result;
}

std::size_t utf8CharacterCount(const std::string& text) {
    std::size_t result = 0;
    for (const unsigned char value : text) {
        if ((value & 0xc0U) != 0x80U) ++result;
    }
    return result;
}

std::string issueIdForTimelineEntry(sqlite3* db, const std::string& entryId) {
    Statement statement(db, "SELECT issue_id FROM timeline_entries WHERE id=?");
    bindText(statement.get(), 1, entryId);
    if (sqlite3_step(statement.get()) != SQLITE_ROW) {
        throw std::runtime_error("Timeline entry does not exist");
    }
    return columnText(statement.get(), 0);
}

sqlite3* openDatabase(const std::filesystem::path& path, const int flags) {
    sqlite3* database = nullptr;
#ifdef _WIN32
    const auto native = path.wstring();
    const auto result = sqlite3_open16(native.c_str(), &database);
    if (result == SQLITE_OK && flags == SQLITE_OPEN_READONLY) {
        sqlite3_exec(database, "PRAGMA query_only=ON", nullptr, nullptr, nullptr);
    }
#else
    const auto result = sqlite3_open_v2(path.string().c_str(), &database, flags, nullptr);
#endif
    if (result != SQLITE_OK) {
        const std::string message = database ? sqlite3_errmsg(database)
                                             : "Cannot open SQLite database";
        if (database) sqlite3_close(database);
        throw std::runtime_error(message + ": " + path.generic_string());
    }
    return database;
}

std::int64_t scalarCount(sqlite3* database, const char* sql) {
    Statement statement(database, sql);
    if (sqlite3_step(statement.get()) != SQLITE_ROW) {
        throw std::runtime_error(sqlite3_errmsg(database));
    }
    return sqlite3_column_int64(statement.get(), 0);
}

bool tableExists(sqlite3* database, const char* name) {
    Statement statement(database,
        "SELECT 1 FROM sqlite_master WHERE type='table' AND name=?");
    bindText(statement.get(), 1, name);
    return sqlite3_step(statement.get()) == SQLITE_ROW;
}

std::optional<int> existingSchemaVersion(sqlite3* database) {
    if (!tableExists(database, "metadata")) return std::nullopt;
    Statement statement(database,
        "SELECT value FROM metadata WHERE key='schema_version'");
    if (sqlite3_step(statement.get()) != SQLITE_ROW) return 0;
    const auto value = columnText(statement.get(), 0);
    std::size_t consumed = 0;
    try {
        const auto version = std::stoi(value, &consumed);
        if (consumed != value.size() || version < 0) return 0;
        return version;
    } catch (...) {
        return 0;
    }
}

bool safeAttachmentPath(const std::filesystem::path& relative) {
    if (relative.empty() || relative.is_absolute()) return false;
    const auto normalized = relative.lexically_normal();
    if (normalized.empty() || *normalized.begin() != "attachments") return false;
    for (const auto& part : normalized) {
        if (part == "..") return false;
    }
    return true;
}

bool pathIsWithin(const std::filesystem::path& candidate,
                  const std::filesystem::path& parent) {
    const auto normalizedCandidate = std::filesystem::absolute(candidate).lexically_normal();
    const auto normalizedParent = std::filesystem::absolute(parent).lexically_normal();
    auto candidatePart = normalizedCandidate.begin();
    for (auto parentPart = normalizedParent.begin(); parentPart != normalizedParent.end();
         ++parentPart, ++candidatePart) {
        if (candidatePart == normalizedCandidate.end() || *candidatePart != *parentPart) {
            return false;
        }
    }
    return true;
}

WorkspaceVerification verifyWorkspaceFiles(const std::filesystem::path& root) {
    WorkspaceVerification result;
    const auto databasePath = root / "issuetrace.db";
    if (!std::filesystem::is_regular_file(databasePath)) {
        result.errors.push_back("缺少 issuetrace.db");
        return result;
    }
    sqlite3* database = nullptr;
    try {
        // The source database uses WAL mode. A standalone backup may need to
        // create transient -shm/-wal files while SQLite opens it, so validate
        // with read/write access but without SQLITE_OPEN_CREATE.
        database = openDatabase(databasePath, SQLITE_OPEN_READWRITE);
        Statement integrity(database, "PRAGMA integrity_check");
        if (sqlite3_step(integrity.get()) != SQLITE_ROW ||
            columnText(integrity.get(), 0) != "ok") {
            result.errors.push_back("SQLite 完整性检查失败");
        }
        const auto version = existingSchemaVersion(database);
        if (!version || *version != kSchemaVersion) {
            result.errors.push_back("不支持的工作区数据库版本");
        }
        result.issueCount = scalarCount(
            database, "SELECT count(*) FROM issues WHERE deleted_at IS NULL");
        result.timelineCount = scalarCount(
            database, "SELECT count(*) FROM timeline_entries WHERE deleted_at IS NULL");
        result.attachmentCount = scalarCount(
            database, "SELECT count(*) FROM attachments WHERE deleted_at IS NULL");

        Statement attachments(database,
            "SELECT relative_path,byte_size FROM attachments WHERE deleted_at IS NULL");
        int step = SQLITE_ROW;
        while ((step = sqlite3_step(attachments.get())) == SQLITE_ROW) {
            const auto relative = std::filesystem::path(columnText(attachments.get(), 0));
            if (!safeAttachmentPath(relative)) {
                result.errors.push_back("附件路径不安全：" + relative.generic_string());
                continue;
            }
            const auto file = root / relative;
            std::error_code error;
            const auto size = std::filesystem::file_size(file, error);
            if (error) {
                result.errors.push_back("附件缺失：" + relative.generic_string());
            } else if (size != static_cast<std::uintmax_t>(
                                   sqlite3_column_int64(attachments.get(), 1))) {
                result.errors.push_back("附件大小不匹配：" + relative.generic_string());
            }
        }
        if (step != SQLITE_DONE) throw std::runtime_error(sqlite3_errmsg(database));
        sqlite3_close(database);
        database = nullptr;
    } catch (const std::exception& error) {
        if (database) sqlite3_close(database);
        result.errors.push_back(std::string("无法验证数据库：") + error.what());
    }
    if (!std::filesystem::is_regular_file(root / "workspace.json")) {
        result.errors.push_back("缺少 workspace.json");
    }
    result.ok = result.errors.empty();
    return result;
}

void copyTree(const std::filesystem::path& source,
              const std::filesystem::path& destination) {
    std::filesystem::create_directories(destination);
    if (!std::filesystem::exists(source)) return;
    for (auto iterator = std::filesystem::recursive_directory_iterator(source);
         iterator != std::filesystem::recursive_directory_iterator(); ++iterator) {
        const auto& item = *iterator;
        if (item.is_symlink()) throw std::runtime_error("工作区附件不能包含符号链接");
        const auto relative = std::filesystem::relative(item.path(), source);
        if (!relative.empty() && *relative.begin() == ".tmp") {
            if (item.is_directory()) iterator.disable_recursion_pending();
            continue;
        }
        const auto target = destination / relative;
        if (item.is_directory()) std::filesystem::create_directories(target);
        else if (item.is_regular_file()) {
            std::filesystem::create_directories(target.parent_path());
            std::filesystem::copy_file(item.path(), target,
                                       std::filesystem::copy_options::overwrite_existing);
        }
    }
}

std::string backupTimestamp() {
    const auto time = std::chrono::system_clock::to_time_t(
        std::chrono::system_clock::now());
    std::tm local{};
#ifdef _WIN32
    localtime_s(&local, &time);
#else
    localtime_r(&time, &local);
#endif
    std::ostringstream result;
    result << std::put_time(&local, "%Y%m%d-%H%M%S");
    return result.str();
}

void backupDatabase(sqlite3* source, const std::filesystem::path& destination) {
    sqlite3* target = openDatabase(destination,
        SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE);
    auto* backup = sqlite3_backup_init(target, "main", source, "main");
    if (!backup) {
        const std::string message = sqlite3_errmsg(target);
        sqlite3_close(target);
        throw std::runtime_error(message);
    }
    const auto step = sqlite3_backup_step(backup, -1);
    const auto finish = sqlite3_backup_finish(backup);
    const std::string message = sqlite3_errmsg(target);
    sqlite3_close(target);
    if (step != SQLITE_DONE || finish != SQLITE_OK) {
        throw std::runtime_error("SQLite 备份失败：" + message);
    }
}

}  // namespace

class IssueStore::Impl final {
public:
    explicit Impl(std::filesystem::path root) : root_(std::move(root)) {
        if (root_.empty()) throw std::invalid_argument("Workspace path is empty");
        std::filesystem::create_directories(root_ / "attachments");
        std::filesystem::create_directories(root_ / "backups");
        std::filesystem::create_directories(root_ / "exports");
        const auto databasePath = root_ / "issuetrace.db";
#ifdef _WIN32
        const auto nativeDatabasePath = databasePath.wstring();
        const auto openResult = sqlite3_open16(nativeDatabasePath.c_str(), &db_);
#else
        const auto openResult = sqlite3_open_v2(
            databasePath.string().c_str(), &db_,
            SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr);
#endif
        if (openResult != SQLITE_OK) {
            const std::string message = db_ ? sqlite3_errmsg(db_) :
                                              "Cannot open SQLite database";
            if (db_) sqlite3_close(db_);
            db_ = nullptr;
            throw std::runtime_error(message);
        }
        std::optional<std::filesystem::path> preMigrationBackup;
        try {
            execute(db_, "PRAGMA foreign_keys=ON; PRAGMA journal_mode=WAL; "
                         "PRAGMA synchronous=NORMAL;");
            const auto existingVersion = existingSchemaVersion(db_);
            if (existingVersion && *existingVersion > kSchemaVersion) {
                throw std::runtime_error(
                    "工作区由更新版本的 IssueTrace 创建，当前版本不能安全打开");
            }
            if ((existingVersion && *existingVersion < kSchemaVersion) ||
                (!existingVersion && tableExists(db_, "issues"))) {
                const auto label = existingVersion ? std::to_string(*existingVersion)
                                                   : std::string("unknown");
                preMigrationBackup = root_ / "backups" /
                    ("PreMigration-v" + label + "-" + backupTimestamp() + "-" +
                     createUuid().substr(0, 8) + ".db");
                backupDatabase(db_, *preMigrationBackup);
            }
            execute(db_,
                    "CREATE TABLE IF NOT EXISTS metadata("
                    "key TEXT PRIMARY KEY,value TEXT NOT NULL);"
                    "INSERT OR IGNORE INTO metadata VALUES('schema_version','1');"
                    "CREATE TABLE IF NOT EXISTS issues("
                    "id TEXT PRIMARY KEY,title TEXT NOT NULL CHECK(length(trim(title))>0),"
                    "original_problem TEXT NOT NULL DEFAULT '',reporter TEXT NOT NULL DEFAULT '',"
                    "assignee TEXT NOT NULL DEFAULT '',service TEXT NOT NULL DEFAULT '',"
                    "version TEXT NOT NULL DEFAULT '',ticket TEXT NOT NULL DEFAULT '',"
                    "status TEXT NOT NULL DEFAULT 'pending',priority TEXT NOT NULL DEFAULT 'normal',"
                    "conclusion TEXT NOT NULL DEFAULT '',reported_at INTEGER NOT NULL,"
                    "resolved_at INTEGER,created_at INTEGER NOT NULL,updated_at INTEGER NOT NULL,"
                    "status_changed_at INTEGER NOT NULL,remind_at INTEGER,"
                    "group_name TEXT NOT NULL DEFAULT '',tags TEXT NOT NULL DEFAULT '',"
                    "tracked_milliseconds INTEGER NOT NULL DEFAULT 0,timer_started_at INTEGER,"
                    "deleted_at INTEGER);"
                    "CREATE INDEX IF NOT EXISTS issues_active_updated "
                    "ON issues(deleted_at,updated_at DESC);"
                    "CREATE TABLE IF NOT EXISTS timeline_entries("
                    "id TEXT PRIMARY KEY,issue_id TEXT NOT NULL REFERENCES issues(id),"
                    "type TEXT NOT NULL,content_markdown TEXT NOT NULL,"
                    "occurred_at INTEGER NOT NULL,created_at INTEGER NOT NULL,"
                    "updated_at INTEGER NOT NULL,deleted_at INTEGER);"
                    "CREATE INDEX IF NOT EXISTS timeline_issue_occurred "
                    "ON timeline_entries(issue_id,deleted_at,occurred_at DESC);"
                    "CREATE TABLE IF NOT EXISTS attachments("
                    "id TEXT PRIMARY KEY,timeline_entry_id TEXT NOT NULL "
                    "REFERENCES timeline_entries(id),relative_path TEXT NOT NULL UNIQUE,"
                    "original_name TEXT NOT NULL,mime_type TEXT NOT NULL,"
                    "byte_size INTEGER NOT NULL,sha256 TEXT NOT NULL,"
                    "created_at INTEGER NOT NULL,deleted_at INTEGER);"
                    "CREATE INDEX IF NOT EXISTS attachment_entry_active "
                    "ON attachments(timeline_entry_id,deleted_at,created_at);"
                    "CREATE TABLE IF NOT EXISTS form_template_versions("
                    "template_id TEXT NOT NULL,version INTEGER NOT NULL,"
                    "content_json TEXT NOT NULL,created_at INTEGER NOT NULL,"
                    "is_active INTEGER NOT NULL DEFAULT 0 CHECK(is_active IN(0,1)),"
                    "PRIMARY KEY(template_id,version));"
                    "CREATE UNIQUE INDEX IF NOT EXISTS one_active_form_template "
                    "ON form_template_versions(is_active) WHERE is_active=1;"
                    "CREATE TABLE IF NOT EXISTS summary_drafts("
                    "id TEXT PRIMARY KEY,issue_id TEXT NOT NULL UNIQUE REFERENCES issues(id),"
                    "content_markdown TEXT NOT NULL,created_at INTEGER NOT NULL,"
                    "updated_at INTEGER NOT NULL);");
            {
                if (!columnExists(db_, "issues", "status_changed_at")) {
                    execute(db_,
                        "ALTER TABLE issues ADD COLUMN status_changed_at INTEGER;");
                }
                if (!columnExists(db_, "issues", "remind_at")) {
                    execute(db_, "ALTER TABLE issues ADD COLUMN remind_at INTEGER;");
                }
                if (!columnExists(db_, "issues", "group_name")) {
                    execute(db_, "ALTER TABLE issues ADD COLUMN group_name TEXT NOT NULL DEFAULT '';");
                }
                if (!columnExists(db_, "issues", "tags")) {
                    execute(db_, "ALTER TABLE issues ADD COLUMN tags TEXT NOT NULL DEFAULT '';");
                }
                if (!columnExists(db_, "issues", "tracked_milliseconds")) {
                    execute(db_, "ALTER TABLE issues ADD COLUMN tracked_milliseconds INTEGER NOT NULL DEFAULT 0;");
                }
                if (!columnExists(db_, "issues", "timer_started_at")) {
                    execute(db_, "ALTER TABLE issues ADD COLUMN timer_started_at INTEGER;");
                }
                execute(db_,
                    "UPDATE issues SET status_changed_at=updated_at "
                    "WHERE status_changed_at IS NULL;"
                    "UPDATE issues SET status='investigating' "
                    "WHERE status='verifying';"
                    "UPDATE issues SET status='completed' "
                    "WHERE status IN('resolved','closed');");
            }
            execute(db_, "UPDATE metadata SET value='8' WHERE key='schema_version';");
            execute(db_, "CREATE VIRTUAL TABLE IF NOT EXISTS issue_search USING "
                         "fts5(issue_id UNINDEXED,content,tokenize='trigram');");
            bool rebuildSearch = true;
            {
                Statement tokenizer(db_,
                    "SELECT value FROM metadata WHERE key='search_tokenizer'");
                if (sqlite3_step(tokenizer.get()) == SQLITE_ROW) {
                    rebuildSearch = columnText(tokenizer.get(), 0) != "trigram-v1";
                }
            }
            if (rebuildSearch) {
                execute(db_, "DROP TABLE issue_search;"
                             "CREATE VIRTUAL TABLE issue_search USING "
                             "fts5(issue_id UNINDEXED,content,tokenize='trigram');"
                             "INSERT INTO metadata(key,value) VALUES"
                             "('search_tokenizer','trigram-v1') ON CONFLICT(key) "
                             "DO UPDATE SET value=excluded.value;");
                Statement issueIds(db_,
                                   "SELECT id FROM issues WHERE deleted_at IS NULL");
                std::vector<std::string> ids;
                while (sqlite3_step(issueIds.get()) == SQLITE_ROW) {
                    ids.push_back(columnText(issueIds.get(), 0));
                }
                for (const auto& id : ids) refreshSearchDocument(db_, id);
            }
            if (!std::filesystem::exists(root_ / "workspace.json")) {
                std::ofstream metadata(root_ / "workspace.json");
                metadata << "{\n  \"formatVersion\": 1,\n  \"application\": "
                            "\"IssueTrace\"\n}\n";
            }
        } catch (...) {
            sqlite3_close(db_);
            db_ = nullptr;
            if (preMigrationBackup) {
                std::error_code ignored;
                std::filesystem::remove(databasePath, ignored);
                std::filesystem::remove(databasePath.string() + "-wal", ignored);
                std::filesystem::remove(databasePath.string() + "-shm", ignored);
                std::filesystem::copy_file(*preMigrationBackup, databasePath,
                    std::filesystem::copy_options::overwrite_existing, ignored);
            }
            throw;
        }
    }

    ~Impl() {
        if (db_) sqlite3_close(db_);
    }

    std::filesystem::path root_;
    sqlite3* db_{};
};

IssueStore::IssueStore(const std::filesystem::path& workspaceRoot)
    : impl_(std::make_unique<Impl>(workspaceRoot)) {}
IssueStore::~IssueStore() = default;
IssueStore::IssueStore(IssueStore&&) noexcept = default;
IssueStore& IssueStore::operator=(IssueStore&&) noexcept = default;

const std::filesystem::path& IssueStore::workspaceRoot() const {
    return impl_->root_;
}

StoredIssue IssueStore::createIssue(std::string title, std::string reporter) {
    if (title.find_first_not_of(" \t\r\n") == std::string::npos) {
        throw std::invalid_argument("事件内容不能为空");
    }
    const auto now = nowMilliseconds();
    StoredIssue issue;
    issue.id = createUuid();
    issue.title = std::move(title);
    issue.reporter = std::move(reporter);
    issue.reportedAt = now;
    issue.createdAt = now;
    issue.updatedAt = now;
    issue.statusChangedAt = now;
    execute(impl_->db_, "BEGIN IMMEDIATE");
    try {
        Statement statement(impl_->db_,
            "INSERT INTO issues(id,title,reporter,status,priority,reported_at,created_at,"
            "updated_at,status_changed_at) VALUES(?,?,?,?,?,?,?,?,?)");
        bindText(statement.get(), 1, issue.id);
        bindText(statement.get(), 2, issue.title);
        bindText(statement.get(), 3, issue.reporter);
        bindText(statement.get(), 4, issue.status);
        bindText(statement.get(), 5, issue.priority);
        sqlite3_bind_int64(statement.get(), 6, issue.reportedAt);
        sqlite3_bind_int64(statement.get(), 7, issue.createdAt);
        sqlite3_bind_int64(statement.get(), 8, issue.updatedAt);
        sqlite3_bind_int64(statement.get(), 9, issue.statusChangedAt);
        if (sqlite3_step(statement.get()) != SQLITE_DONE) {
            throw std::runtime_error(sqlite3_errmsg(impl_->db_));
        }
        refreshSearchDocument(impl_->db_, issue.id);
        execute(impl_->db_, "COMMIT");
    } catch (...) {
        execute(impl_->db_, "ROLLBACK");
        throw;
    }
    return issue;
}

std::vector<StoredIssue> IssueStore::listIssues() const {
    const std::string sql = std::string("SELECT ") + issueColumns +
                            " FROM issues WHERE deleted_at IS NULL "
                            "ORDER BY updated_at DESC,id ASC";
    Statement statement(impl_->db_, sql.c_str());
    std::vector<StoredIssue> result;
    while (sqlite3_step(statement.get()) == SQLITE_ROW) {
        result.push_back(readIssue(statement.get()));
    }
    return result;
}

std::vector<StoredIssue> IssueStore::listDeletedIssues() const {
    const std::string sql = std::string("SELECT ") + issueColumns +
                            " FROM issues WHERE deleted_at IS NOT NULL "
                            "ORDER BY deleted_at DESC,id ASC";
    Statement statement(impl_->db_, sql.c_str());
    std::vector<StoredIssue> result;
    while (sqlite3_step(statement.get()) == SQLITE_ROW) {
        result.push_back(readIssue(statement.get()));
    }
    return result;
}

std::vector<StoredIssue> IssueStore::searchIssues(const IssueQuery& query) const {
    std::string orderBy = "i.updated_at DESC,i.id ASC";
    if (query.sort == "reported_desc") orderBy = "i.reported_at DESC,i.id ASC";
    else if (query.sort == "created_desc") orderBy = "i.created_at DESC,i.id ASC";
    else if (query.sort == "priority_desc") {
        orderBy = "CASE i.priority WHEN 'urgent' THEN 0 WHEN 'high' THEN 1 "
                  "WHEN 'normal' THEN 2 ELSE 3 END,i.updated_at DESC,i.id ASC";
    } else if (query.sort == "title_asc") {
        orderBy = "i.title COLLATE NOCASE ASC,i.updated_at DESC,i.id ASC";
    } else if (query.sort == "tracked_desc") {
        orderBy = "i.tracked_milliseconds+CASE WHEN i.timer_started_at IS NULL "
                  "THEN 0 ELSE MAX(0," + std::to_string(nowMilliseconds()) +
                  "-i.timer_started_at) END DESC,i.updated_at DESC,i.id ASC";
    } else if (query.sort == "status") orderBy = "i.status ASC,i.updated_at DESC,i.id ASC";
    else if (query.sort == "service") orderBy = "i.service ASC,i.updated_at DESC,i.id ASC";
    else if (query.sort == "assignee") orderBy = "i.assignee ASC,i.updated_at DESC,i.id ASC";
    else if (query.sort == "group") {
        orderBy = "CASE WHEN trim(i.group_name)='' THEN 0 ELSE 1 END,"
                  "i.group_name COLLATE NOCASE,i.updated_at DESC,i.id ASC";
    }
    const std::string sql = std::string("SELECT ") + issueColumns +
        " FROM issues i WHERE i.deleted_at IS NULL "
        "AND (?1='' OR (?6=1 AND i.id IN(SELECT issue_id FROM issue_search "
        "WHERE issue_search MATCH ?2)) OR (?6=0 AND i.id IN(SELECT issue_id "
        "FROM issue_search WHERE content LIKE '%'||?1||'%'))) "
        "AND (?3='' OR i.status=?3) "
        "AND (?4='' OR i.service LIKE '%'||?4||'%') "
        "AND (?5='' OR i.assignee LIKE '%'||?5||'%') "
        "AND (?7=0 OR i.updated_at<=?7) "
        "AND (?8='' OR i.priority=?8) "
        "AND (?9='' OR 1=1) "
        "AND (?10='' OR COALESCE((SELECT t.content_markdown FROM timeline_entries t "
        "WHERE t.issue_id=i.id AND t.deleted_at IS NULL AND t.type='progress' "
        "ORDER BY t.occurred_at DESC LIMIT 1),'') LIKE '%'||?10||'%') "
        "AND (?11='' OR i.title LIKE '%'||?11||'%') "
        "AND (?12=0 OR i.tracked_milliseconds+CASE WHEN i.timer_started_at IS NULL "
        "THEN 0 ELSE MAX(0,?14-i.timer_started_at) END>=?12) "
        "AND (?13=0 OR i.tracked_milliseconds+CASE WHEN i.timer_started_at IS NULL "
        "THEN 0 ELSE MAX(0,?14-i.timer_started_at) END<=?13) "
        "AND (?15='' OR (?15='__default__' AND trim(i.group_name)='') OR "
        "i.group_name=?15 OR i.group_name LIKE ?15||'/%') "
        "AND (?16='' OR i.version LIKE '%'||?16||'%') "
        "AND (?17='' OR i.ticket LIKE '%'||?17||'%') ORDER BY " + orderBy;
    Statement statement(impl_->db_, sql.c_str());
    bindText(statement.get(), 1, query.text);
    bindText(statement.get(), 2, quoteFtsQuery(query.text));
    bindText(statement.get(), 3, query.status);
    bindText(statement.get(), 4, query.service);
    bindText(statement.get(), 5, query.assignee);
    sqlite3_bind_int(statement.get(), 6,
                     utf8CharacterCount(query.text) >= 3 ? 1 : 0);
    const auto staleBefore = query.staleDays > 0
        ? nowMilliseconds() - static_cast<std::int64_t>(query.staleDays) * 86400000
        : 0;
    sqlite3_bind_int64(statement.get(), 7, staleBefore);
    bindText(statement.get(), 8, query.priority);
    bindText(statement.get(), 9, query.tag);
    bindText(statement.get(), 10, query.progress);
    bindText(statement.get(), 11, query.titleText);
    sqlite3_bind_int64(statement.get(), 12,
        static_cast<std::int64_t>(query.minimumTrackedMinutes) * 60000);
    sqlite3_bind_int64(statement.get(), 13,
        static_cast<std::int64_t>(query.maximumTrackedMinutes) * 60000);
    sqlite3_bind_int64(statement.get(), 14, nowMilliseconds());
    bindText(statement.get(), 15, query.groupPath);
    bindText(statement.get(), 16, query.version);
    bindText(statement.get(), 17, query.ticket);
    const auto selectedTags = [&query] {
        std::vector<std::string> values;
        std::size_t start = 0;
        while (start <= query.tag.size()) {
            const auto end = query.tag.find(',', start);
            auto value = query.tag.substr(start, end == std::string::npos
                ? std::string::npos : end - start);
            const auto first = value.find_first_not_of(" \t\r\n");
            const auto last = value.find_last_not_of(" \t\r\n");
            if (first != std::string::npos) values.push_back(value.substr(first, last - first + 1));
            if (end == std::string::npos) break;
            start = end + 1;
        }
        return values;
    }();
    std::vector<StoredIssue> result;
    int step = SQLITE_ROW;
    while ((step = sqlite3_step(statement.get())) == SQLITE_ROW) {
        auto issue = readIssue(statement.get());
        bool includesAllTags = true;
        for (const auto& tag : selectedTags) {
            const auto haystack = "," + issue.tags + ",";
            if (haystack.find("," + tag + ",") == std::string::npos) {
                includesAllTags = false;
                break;
            }
        }
        if (includesAllTags) result.push_back(std::move(issue));
    }
    if (step != SQLITE_DONE) throw std::runtime_error(sqlite3_errmsg(impl_->db_));
    return result;
}

void IssueStore::renameIssueGroupPrefix(const std::string& sourcePrefix,
                                        const std::string& destinationPrefix) {
    if (sourcePrefix.empty() || destinationPrefix.empty()) {
        throw std::invalid_argument("分组路径不能为空");
    }
    if (destinationPrefix == sourcePrefix ||
        destinationPrefix.starts_with(sourcePrefix + "/")) {
        throw std::invalid_argument("不能把分组移动到自身或其子分组");
    }
    execute(impl_->db_, "BEGIN IMMEDIATE");
    try {
        Statement ids(impl_->db_,
            "SELECT id FROM issues WHERE deleted_at IS NULL AND "
            "(group_name=? OR group_name LIKE ?||'/%')");
        bindText(ids.get(), 1, sourcePrefix);
        bindText(ids.get(), 2, sourcePrefix);
        std::vector<std::string> affected;
        while (sqlite3_step(ids.get()) == SQLITE_ROW) {
            affected.push_back(columnText(ids.get(), 0));
        }
        Statement rename(impl_->db_,
            "UPDATE issues SET group_name=?||substr(group_name,length(?)+1) "
            "WHERE deleted_at IS NULL AND (group_name=? OR group_name LIKE ?||'/%')");
        bindText(rename.get(), 1, destinationPrefix);
        bindText(rename.get(), 2, sourcePrefix);
        bindText(rename.get(), 3, sourcePrefix);
        bindText(rename.get(), 4, sourcePrefix);
        if (sqlite3_step(rename.get()) != SQLITE_DONE) {
            throw std::runtime_error(sqlite3_errmsg(impl_->db_));
        }
        for (const auto& id : affected) refreshSearchDocument(impl_->db_, id);
        execute(impl_->db_, "COMMIT");
    } catch (...) {
        execute(impl_->db_, "ROLLBACK");
        throw;
    }
}

std::optional<StoredIssue> IssueStore::findIssue(const std::string& id) const {
    const std::string sql = std::string("SELECT ") + issueColumns +
                            " FROM issues WHERE id=? AND deleted_at IS NULL";
    Statement statement(impl_->db_, sql.c_str());
    bindText(statement.get(), 1, id);
    if (sqlite3_step(statement.get()) == SQLITE_ROW) {
        return readIssue(statement.get());
    }
    return std::nullopt;
}

void IssueStore::updateIssue(const StoredIssue& issue) {
    if (issue.title.find_first_not_of(" \t\r\n") == std::string::npos) {
        throw std::invalid_argument("事件内容不能为空");
    }
    execute(impl_->db_, "BEGIN IMMEDIATE");
    try {
    Statement statement(impl_->db_,
        "UPDATE issues SET title=?,original_problem=?,reporter=?,assignee=?,service=?,"
        "version=?,ticket=?,status=?,priority=?,group_name=?,tags=?,conclusion=?,reported_at=?,resolved_at=?,"
        "updated_at=?,status_changed_at=?,remind_at=? WHERE id=? AND deleted_at IS NULL");
    const std::array<const std::string*, 12> values{
        &issue.title, &issue.originalProblem, &issue.reporter, &issue.assignee,
        &issue.service, &issue.version, &issue.ticket, &issue.status,
        &issue.priority, &issue.groupName, &issue.tags, &issue.conclusion};
    int index = 1;
    for (const auto* value : values) bindText(statement.get(), index++, *value);
    sqlite3_bind_int64(statement.get(), index++, issue.reportedAt);
    if (issue.resolvedAt) sqlite3_bind_int64(statement.get(), index++, *issue.resolvedAt);
    else sqlite3_bind_null(statement.get(), index++);
    sqlite3_bind_int64(statement.get(), index++, nowMilliseconds());
    sqlite3_bind_int64(statement.get(), index++, issue.statusChangedAt);
    if (issue.remindAt) sqlite3_bind_int64(statement.get(), index++, *issue.remindAt);
    else sqlite3_bind_null(statement.get(), index++);
    bindText(statement.get(), index, issue.id);
    if (sqlite3_step(statement.get()) != SQLITE_DONE) {
        throw std::runtime_error(sqlite3_errmsg(impl_->db_));
    }
    if (sqlite3_changes(impl_->db_) != 1) {
        throw std::runtime_error("Issue does not exist");
    }
    refreshSearchDocument(impl_->db_, issue.id);
    execute(impl_->db_, "COMMIT");
    } catch (...) {
        execute(impl_->db_, "ROLLBACK");
        throw;
    }
}

void IssueStore::setIssueReminder(const std::string& id,
                                  const std::optional<std::int64_t> remindAt) {
    Statement statement(impl_->db_,
                        "UPDATE issues SET remind_at=? WHERE id=? AND deleted_at IS NULL");
    if (remindAt) sqlite3_bind_int64(statement.get(), 1, *remindAt);
    else sqlite3_bind_null(statement.get(), 1);
    bindText(statement.get(), 2, id);
    if (sqlite3_step(statement.get()) != SQLITE_DONE) {
        throw std::runtime_error(sqlite3_errmsg(impl_->db_));
    }
    if (sqlite3_changes(impl_->db_) != 1) {
        throw std::runtime_error("Issue does not exist");
    }
}

void IssueStore::setIssueTrackedMilliseconds(
    const std::string& id, const std::int64_t trackedMilliseconds) {
    if (trackedMilliseconds < 0) {
        throw std::invalid_argument("累计处理时间不能为负数");
    }
    Statement statement(
        impl_->db_,
        "UPDATE issues SET tracked_milliseconds=? WHERE id=? AND deleted_at IS NULL "
        "AND timer_started_at IS NULL");
    sqlite3_bind_int64(statement.get(), 1, trackedMilliseconds);
    bindText(statement.get(), 2, id);
    if (sqlite3_step(statement.get()) != SQLITE_DONE) {
        throw std::runtime_error(sqlite3_errmsg(impl_->db_));
    }
    if (sqlite3_changes(impl_->db_) != 1) {
        const auto issue = findIssue(id);
        if (!issue) throw std::runtime_error("Issue does not exist");
        if (issue->timerStartedAt) {
            throw std::runtime_error("请先暂停计时再修改累计处理时间");
        }
        throw std::runtime_error("累计处理时间保存失败");
    }
}

void IssueStore::startIssueTimer(const std::string& id) {
    Statement statement(impl_->db_,
        "UPDATE issues SET timer_started_at=? WHERE id=? AND deleted_at IS NULL "
        "AND timer_started_at IS NULL");
    sqlite3_bind_int64(statement.get(), 1, nowMilliseconds());
    bindText(statement.get(), 2, id);
    if (sqlite3_step(statement.get()) != SQLITE_DONE) {
        throw std::runtime_error(sqlite3_errmsg(impl_->db_));
    }
}

void IssueStore::pauseIssueTimer(const std::string& id) {
    const auto now = nowMilliseconds();
    Statement statement(impl_->db_,
        "UPDATE issues SET tracked_milliseconds=tracked_milliseconds+MAX(0,?-timer_started_at),"
        "timer_started_at=NULL WHERE id=? AND deleted_at IS NULL AND timer_started_at IS NOT NULL");
    sqlite3_bind_int64(statement.get(), 1, now);
    bindText(statement.get(), 2, id);
    if (sqlite3_step(statement.get()) != SQLITE_DONE) {
        throw std::runtime_error(sqlite3_errmsg(impl_->db_));
    }
}

namespace {
std::vector<std::string> distinctIssueValues(sqlite3* db, const char* column) {
    const std::string sql = std::string("SELECT DISTINCT trim(") + column +
        ") FROM issues WHERE deleted_at IS NULL AND trim(" + column +
        ")<>'' ORDER BY trim(" + column + ") COLLATE NOCASE";
    Statement statement(db, sql.c_str());
    std::vector<std::string> values;
    while (sqlite3_step(statement.get()) == SQLITE_ROW) values.push_back(columnText(statement.get(), 0));
    return values;
}
}

std::vector<std::string> IssueStore::distinctServices() const {
    return distinctIssueValues(impl_->db_, "service");
}

std::vector<std::string> IssueStore::distinctVersions() const {
    return distinctIssueValues(impl_->db_, "version");
}

std::vector<std::string> IssueStore::distinctReporters() const {
    return distinctIssueValues(impl_->db_, "reporter");
}

std::vector<std::string> IssueStore::distinctAssignees() const {
    return distinctIssueValues(impl_->db_, "assignee");
}

std::vector<std::string> IssueStore::distinctGroups() const {
    return distinctIssueValues(impl_->db_, "group_name");
}

void IssueStore::softDeleteIssue(const std::string& id) {
    execute(impl_->db_, "BEGIN IMMEDIATE");
    try {
    Statement statement(impl_->db_,
                        "UPDATE issues SET deleted_at=?,updated_at=? "
                        "WHERE id=? AND deleted_at IS NULL");
    const auto now = nowMilliseconds();
    sqlite3_bind_int64(statement.get(), 1, now);
    sqlite3_bind_int64(statement.get(), 2, now);
    bindText(statement.get(), 3, id);
    if (sqlite3_step(statement.get()) != SQLITE_DONE) {
        throw std::runtime_error(sqlite3_errmsg(impl_->db_));
    }
    if (sqlite3_changes(impl_->db_) != 1) {
        throw std::runtime_error("Issue does not exist");
    }
    refreshSearchDocument(impl_->db_, id);
    execute(impl_->db_, "COMMIT");
    } catch (...) {
        execute(impl_->db_, "ROLLBACK");
        throw;
    }
}

void IssueStore::restoreIssue(const std::string& id) {
    execute(impl_->db_, "BEGIN IMMEDIATE");
    try {
        Statement statement(impl_->db_,
            "UPDATE issues SET deleted_at=NULL,updated_at=? "
            "WHERE id=? AND deleted_at IS NOT NULL");
        sqlite3_bind_int64(statement.get(), 1, nowMilliseconds());
        bindText(statement.get(), 2, id);
        if (sqlite3_step(statement.get()) != SQLITE_DONE) {
            throw std::runtime_error(sqlite3_errmsg(impl_->db_));
        }
        if (sqlite3_changes(impl_->db_) != 1) {
            throw std::runtime_error("Deleted issue does not exist");
        }
        refreshSearchDocument(impl_->db_, id);
        execute(impl_->db_, "COMMIT");
    } catch (...) {
        execute(impl_->db_, "ROLLBACK");
        throw;
    }
}

TimelineEntry IssueStore::createTimelineEntry(const std::string& issueId,
                                              std::string type,
                                              std::string contentMarkdown) {
    if (contentMarkdown.find_first_not_of(" \t\r\n") == std::string::npos) {
        throw std::invalid_argument("记录内容不能为空");
    }
    if (type.empty()) type = "note";
    const auto now = nowMilliseconds();
    TimelineEntry entry{createUuid(), issueId, std::move(type),
                        std::move(contentMarkdown), now, now, now};
    execute(impl_->db_, "BEGIN IMMEDIATE");
    try {
        Statement insert(impl_->db_,
            "INSERT INTO timeline_entries(id,issue_id,type,content_markdown,"
            "occurred_at,created_at,updated_at) VALUES(?,?,?,?,?,?,?)");
        bindText(insert.get(), 1, entry.id);
        bindText(insert.get(), 2, entry.issueId);
        bindText(insert.get(), 3, entry.type);
        bindText(insert.get(), 4, entry.contentMarkdown);
        sqlite3_bind_int64(insert.get(), 5, now);
        sqlite3_bind_int64(insert.get(), 6, now);
        sqlite3_bind_int64(insert.get(), 7, now);
        if (sqlite3_step(insert.get()) != SQLITE_DONE) {
            throw std::runtime_error(sqlite3_errmsg(impl_->db_));
        }
        Statement touch(impl_->db_,
                        "UPDATE issues SET updated_at=? WHERE id=? AND deleted_at IS NULL");
        sqlite3_bind_int64(touch.get(), 1, now);
        bindText(touch.get(), 2, issueId);
        if (sqlite3_step(touch.get()) != SQLITE_DONE ||
            sqlite3_changes(impl_->db_) != 1) {
            throw std::runtime_error("Issue does not exist");
        }
        refreshSearchDocument(impl_->db_, issueId);
        execute(impl_->db_, "COMMIT");
    } catch (...) {
        execute(impl_->db_, "ROLLBACK");
        throw;
    }
    return entry;
}

std::vector<TimelineEntry> IssueStore::listTimelineEntries(
    const std::string& issueId) const {
    Statement statement(impl_->db_,
        "SELECT id,issue_id,type,content_markdown,occurred_at,created_at,updated_at "
                            "FROM timeline_entries WHERE issue_id=? AND deleted_at IS NULL "
                            "AND type<>'_description_attachment' "
        "ORDER BY occurred_at DESC,rowid DESC");
    bindText(statement.get(), 1, issueId);
    std::vector<TimelineEntry> result;
    while (sqlite3_step(statement.get()) == SQLITE_ROW) {
        TimelineEntry entry;
        entry.id = columnText(statement.get(), 0);
        entry.issueId = columnText(statement.get(), 1);
        entry.type = columnText(statement.get(), 2);
        entry.contentMarkdown = columnText(statement.get(), 3);
        entry.occurredAt = sqlite3_column_int64(statement.get(), 4);
        entry.createdAt = sqlite3_column_int64(statement.get(), 5);
        entry.updatedAt = sqlite3_column_int64(statement.get(), 6);
        result.push_back(std::move(entry));
    }
    return result;
}

void IssueStore::updateTimelineEntry(const std::string& id, std::string type,
                                     std::string contentMarkdown) {
    if (contentMarkdown.find_first_not_of(" \t\r\n") == std::string::npos) {
        throw std::invalid_argument("记录内容不能为空");
    }
    if (type.empty()) type = "note";
    const auto issueId = issueIdForTimelineEntry(impl_->db_, id);
    const auto now = nowMilliseconds();
    execute(impl_->db_, "BEGIN IMMEDIATE");
    try {
        Statement statement(impl_->db_,
            "UPDATE timeline_entries SET type=?,content_markdown=?,updated_at=? "
            "WHERE id=? AND deleted_at IS NULL");
        bindText(statement.get(), 1, type);
        bindText(statement.get(), 2, contentMarkdown);
        sqlite3_bind_int64(statement.get(), 3, now);
        bindText(statement.get(), 4, id);
        if (sqlite3_step(statement.get()) != SQLITE_DONE ||
            sqlite3_changes(impl_->db_) != 1) {
            throw std::runtime_error("Timeline entry does not exist");
        }
        Statement touch(impl_->db_,
            "UPDATE issues SET updated_at=? WHERE deleted_at IS NULL AND id=("
            "SELECT issue_id FROM timeline_entries WHERE id=?)");
        sqlite3_bind_int64(touch.get(), 1, now);
        bindText(touch.get(), 2, id);
        if (sqlite3_step(touch.get()) != SQLITE_DONE ||
            sqlite3_changes(impl_->db_) != 1) {
            throw std::runtime_error("Issue does not exist");
        }
        refreshSearchDocument(impl_->db_, issueId);
        execute(impl_->db_, "COMMIT");
    } catch (...) {
        execute(impl_->db_, "ROLLBACK");
        throw;
    }
}

void IssueStore::softDeleteTimelineEntry(const std::string& id) {
    const auto issueId = issueIdForTimelineEntry(impl_->db_, id);
    const auto now = nowMilliseconds();
    execute(impl_->db_, "BEGIN IMMEDIATE");
    try {
        Statement statement(impl_->db_,
            "UPDATE timeline_entries SET deleted_at=?,updated_at=? "
            "WHERE id=? AND deleted_at IS NULL");
        sqlite3_bind_int64(statement.get(), 1, now);
        sqlite3_bind_int64(statement.get(), 2, now);
        bindText(statement.get(), 3, id);
        if (sqlite3_step(statement.get()) != SQLITE_DONE ||
            sqlite3_changes(impl_->db_) != 1) {
            throw std::runtime_error("Timeline entry does not exist");
        }
        Statement touch(impl_->db_,
            "UPDATE issues SET updated_at=? WHERE deleted_at IS NULL AND id=("
            "SELECT issue_id FROM timeline_entries WHERE id=?)");
        sqlite3_bind_int64(touch.get(), 1, now);
        bindText(touch.get(), 2, id);
        if (sqlite3_step(touch.get()) != SQLITE_DONE ||
            sqlite3_changes(impl_->db_) != 1) {
            throw std::runtime_error("Issue does not exist");
        }
        refreshSearchDocument(impl_->db_, issueId);
        execute(impl_->db_, "COMMIT");
    } catch (...) {
        execute(impl_->db_, "ROLLBACK");
        throw;
    }
}

std::string IssueStore::currentProgress(const std::string& issueId) const {
    Statement statement(impl_->db_,
        "SELECT content_markdown FROM timeline_entries "
        "WHERE issue_id=? AND type='progress' AND deleted_at IS NULL "
        "ORDER BY occurred_at DESC,rowid DESC LIMIT 1");
    bindText(statement.get(), 1, issueId);
    return sqlite3_step(statement.get()) == SQLITE_ROW
               ? columnText(statement.get(), 0)
               : std::string{};
}

Attachment IssueStore::addAttachment(
    const std::string& timelineEntryId, const std::string& originalName,
    const std::string& mimeType, const std::string& sha256,
    const std::span<const unsigned char> content) {
    if (content.empty()) throw std::invalid_argument("附件内容不能为空");
    if (originalName.empty()) throw std::invalid_argument("附件名称不能为空");

    Statement owner(impl_->db_,
        "SELECT issue_id FROM timeline_entries WHERE id=? AND deleted_at IS NULL");
    bindText(owner.get(), 1, timelineEntryId);
    if (sqlite3_step(owner.get()) != SQLITE_ROW) {
        throw std::runtime_error("Timeline entry does not exist");
    }
    const auto issueId = columnText(owner.get(), 0);
    const auto id = createUuid();
    const auto extension = safeExtension(originalName);
    const auto relative = std::filesystem::path("attachments") / issueId /
                          (id + extension);
    const auto finalPath = impl_->root_ / relative;
    const auto temporaryDirectory = impl_->root_ / "attachments" / ".tmp";
    const auto temporaryPath = temporaryDirectory / (id + ".tmp");
    std::filesystem::create_directories(finalPath.parent_path());
    std::filesystem::create_directories(temporaryDirectory);
    {
        std::ofstream output(temporaryPath, std::ios::binary | std::ios::trunc);
        if (!output) throw std::runtime_error("无法创建附件临时文件");
        output.write(reinterpret_cast<const char*>(content.data()),
                     static_cast<std::streamsize>(content.size()));
        if (!output) {
            output.close();
            std::filesystem::remove(temporaryPath);
            throw std::runtime_error("附件写入失败");
        }
    }

    const auto now = nowMilliseconds();
    Attachment attachment{id, timelineEntryId, relative.generic_string(),
                          originalName, mimeType, static_cast<std::int64_t>(content.size()),
                          sha256, now};
    bool moved = false;
    execute(impl_->db_, "BEGIN IMMEDIATE");
    try {
        Statement insert(impl_->db_,
            "INSERT INTO attachments(id,timeline_entry_id,relative_path,original_name,"
            "mime_type,byte_size,sha256,created_at) VALUES(?,?,?,?,?,?,?,?)");
        bindText(insert.get(), 1, attachment.id);
        bindText(insert.get(), 2, attachment.timelineEntryId);
        bindText(insert.get(), 3, attachment.relativePath);
        bindText(insert.get(), 4, attachment.originalName);
        bindText(insert.get(), 5, attachment.mimeType);
        sqlite3_bind_int64(insert.get(), 6, attachment.byteSize);
        bindText(insert.get(), 7, attachment.sha256);
        sqlite3_bind_int64(insert.get(), 8, attachment.createdAt);
        if (sqlite3_step(insert.get()) != SQLITE_DONE) {
            throw std::runtime_error(sqlite3_errmsg(impl_->db_));
        }
        std::filesystem::rename(temporaryPath, finalPath);
        moved = true;
        Statement touch(impl_->db_,
            "UPDATE issues SET updated_at=? WHERE deleted_at IS NULL AND id=?");
        sqlite3_bind_int64(touch.get(), 1, now);
        bindText(touch.get(), 2, issueId);
        if (sqlite3_step(touch.get()) != SQLITE_DONE ||
            sqlite3_changes(impl_->db_) != 1) {
            throw std::runtime_error("Issue does not exist");
        }
        refreshSearchDocument(impl_->db_, issueId);
        execute(impl_->db_, "COMMIT");
    } catch (...) {
        execute(impl_->db_, "ROLLBACK");
        std::error_code ignored;
        std::filesystem::remove(moved ? finalPath : temporaryPath, ignored);
        throw;
    }
    return attachment;
}

std::vector<Attachment> IssueStore::listAttachments(
    const std::string& timelineEntryId) const {
    Statement statement(impl_->db_,
        "SELECT id,timeline_entry_id,relative_path,original_name,mime_type,"
        "byte_size,sha256,created_at FROM attachments "
        "WHERE timeline_entry_id=? AND deleted_at IS NULL ORDER BY created_at,id");
    bindText(statement.get(), 1, timelineEntryId);
    std::vector<Attachment> result;
    while (sqlite3_step(statement.get()) == SQLITE_ROW) {
        Attachment attachment;
        attachment.id = columnText(statement.get(), 0);
        attachment.timelineEntryId = columnText(statement.get(), 1);
        attachment.relativePath = columnText(statement.get(), 2);
        attachment.originalName = columnText(statement.get(), 3);
        attachment.mimeType = columnText(statement.get(), 4);
        attachment.byteSize = sqlite3_column_int64(statement.get(), 5);
        attachment.sha256 = columnText(statement.get(), 6);
        attachment.createdAt = sqlite3_column_int64(statement.get(), 7);
        result.push_back(std::move(attachment));
    }
    return result;
}

std::vector<Attachment> IssueStore::listDescriptionAttachments(
    const std::string& issueId) const {
    Statement statement(impl_->db_,
        "SELECT a.id,a.timeline_entry_id,a.relative_path,a.original_name,a.mime_type,"
        "a.byte_size,a.sha256,a.created_at FROM attachments a JOIN timeline_entries t "
        "ON t.id=a.timeline_entry_id WHERE t.issue_id=? AND t.type='_description_attachment' "
        "AND t.deleted_at IS NULL AND a.deleted_at IS NULL ORDER BY a.created_at,a.id");
    bindText(statement.get(), 1, issueId);
    std::vector<Attachment> result;
    while (sqlite3_step(statement.get()) == SQLITE_ROW) {
        Attachment value;
        value.id = columnText(statement.get(), 0);
        value.timelineEntryId = columnText(statement.get(), 1);
        value.relativePath = columnText(statement.get(), 2);
        value.originalName = columnText(statement.get(), 3);
        value.mimeType = columnText(statement.get(), 4);
        value.byteSize = sqlite3_column_int64(statement.get(), 5);
        value.sha256 = columnText(statement.get(), 6);
        value.createdAt = sqlite3_column_int64(statement.get(), 7);
        result.push_back(std::move(value));
    }
    return result;
}

void IssueStore::softDeleteAttachment(const std::string& id) {
    Statement owner(impl_->db_,
        "SELECT t.issue_id FROM attachments a JOIN timeline_entries t "
        "ON t.id=a.timeline_entry_id WHERE a.id=? AND a.deleted_at IS NULL");
    bindText(owner.get(), 1, id);
    if (sqlite3_step(owner.get()) != SQLITE_ROW) {
        throw std::runtime_error("Attachment does not exist");
    }
    const auto issueId = columnText(owner.get(), 0);
    const auto now = nowMilliseconds();
    execute(impl_->db_, "BEGIN IMMEDIATE");
    try {
        Statement statement(impl_->db_,
            "UPDATE attachments SET deleted_at=? WHERE id=? AND deleted_at IS NULL");
        sqlite3_bind_int64(statement.get(), 1, now);
        bindText(statement.get(), 2, id);
        if (sqlite3_step(statement.get()) != SQLITE_DONE ||
            sqlite3_changes(impl_->db_) != 1) {
            throw std::runtime_error("Attachment does not exist");
        }
        Statement touch(impl_->db_,
            "UPDATE issues SET updated_at=? WHERE id=? AND deleted_at IS NULL");
        sqlite3_bind_int64(touch.get(), 1, now);
        bindText(touch.get(), 2, issueId);
        if (sqlite3_step(touch.get()) != SQLITE_DONE ||
            sqlite3_changes(impl_->db_) != 1) {
            throw std::runtime_error("Issue does not exist");
        }
        refreshSearchDocument(impl_->db_, issueId);
        execute(impl_->db_, "COMMIT");
    } catch (...) {
        execute(impl_->db_, "ROLLBACK");
        throw;
    }
}

std::optional<std::string> IssueStore::workspaceValue(
    const std::string& key) const {
    Statement statement(impl_->db_, "SELECT value FROM metadata WHERE key=?");
    bindText(statement.get(), 1, key);
    if (sqlite3_step(statement.get()) == SQLITE_ROW) {
        return columnText(statement.get(), 0);
    }
    return std::nullopt;
}

void IssueStore::setWorkspaceValue(const std::string& key,
                                   const std::string& value) {
    Statement statement(impl_->db_,
        "INSERT INTO metadata(key,value) VALUES(?,?) "
        "ON CONFLICT(key) DO UPDATE SET value=excluded.value");
    bindText(statement.get(), 1, key);
    bindText(statement.get(), 2, value);
    if (sqlite3_step(statement.get()) != SQLITE_DONE) {
        throw std::runtime_error(sqlite3_errmsg(impl_->db_));
    }
}

std::optional<std::string> IssueStore::activeFormTemplateJson() const {
    Statement statement(impl_->db_,
        "SELECT content_json FROM form_template_versions WHERE is_active=1 LIMIT 1");
    if (sqlite3_step(statement.get()) == SQLITE_ROW) {
        return columnText(statement.get(), 0);
    }
    return std::nullopt;
}

void IssueStore::publishFormTemplate(const std::string& templateId,
                                     const int version,
                                     const std::string& contentJson) {
    if (templateId.empty() || version < 1 || contentJson.empty()) {
        throw std::invalid_argument("Invalid form template version");
    }
    execute(impl_->db_, "BEGIN IMMEDIATE");
    try {
        execute(impl_->db_, "UPDATE form_template_versions SET is_active=0 "
                            "WHERE is_active=1");
        Statement insert(impl_->db_,
            "INSERT INTO form_template_versions(template_id,version,content_json,"
            "created_at,is_active) VALUES(?,?,?,?,1)");
        bindText(insert.get(), 1, templateId);
        sqlite3_bind_int(insert.get(), 2, version);
        bindText(insert.get(), 3, contentJson);
        sqlite3_bind_int64(insert.get(), 4, nowMilliseconds());
        if (sqlite3_step(insert.get()) != SQLITE_DONE) {
            throw std::runtime_error(sqlite3_errmsg(impl_->db_));
        }
        execute(impl_->db_, "COMMIT");
    } catch (...) {
        execute(impl_->db_, "ROLLBACK");
        throw;
    }
}

std::optional<SummaryDraft> IssueStore::findSummaryDraft(
    const std::string& issueId) const {
    Statement statement(impl_->db_,
        "SELECT id,issue_id,content_markdown,created_at,updated_at "
        "FROM summary_drafts WHERE issue_id=?");
    bindText(statement.get(), 1, issueId);
    if (sqlite3_step(statement.get()) != SQLITE_ROW) return std::nullopt;
    SummaryDraft draft;
    draft.id = columnText(statement.get(), 0);
    draft.issueId = columnText(statement.get(), 1);
    draft.contentMarkdown = columnText(statement.get(), 2);
    draft.createdAt = sqlite3_column_int64(statement.get(), 3);
    draft.updatedAt = sqlite3_column_int64(statement.get(), 4);
    return draft;
}

SummaryDraft IssueStore::saveSummaryDraft(const std::string& issueId,
                                          const std::string& contentMarkdown) {
    if (contentMarkdown.empty()) throw std::invalid_argument("总结内容不能为空");
    const auto now = nowMilliseconds();
    execute(impl_->db_, "BEGIN IMMEDIATE");
    try {
        Statement statement(impl_->db_,
            "INSERT INTO summary_drafts(id,issue_id,content_markdown,created_at,updated_at) "
            "SELECT ?,?,?,?,? WHERE EXISTS(SELECT 1 FROM issues WHERE id=? "
            "AND deleted_at IS NULL) ON CONFLICT(issue_id) DO UPDATE SET "
            "content_markdown=excluded.content_markdown,updated_at=excluded.updated_at");
        bindText(statement.get(), 1, createUuid());
        bindText(statement.get(), 2, issueId);
        bindText(statement.get(), 3, contentMarkdown);
        sqlite3_bind_int64(statement.get(), 4, now);
        sqlite3_bind_int64(statement.get(), 5, now);
        bindText(statement.get(), 6, issueId);
        if (sqlite3_step(statement.get()) != SQLITE_DONE ||
            sqlite3_changes(impl_->db_) != 1) {
            throw std::runtime_error("Issue does not exist");
        }
        refreshSearchDocument(impl_->db_, issueId);
        execute(impl_->db_, "COMMIT");
    } catch (...) {
        execute(impl_->db_, "ROLLBACK");
        throw;
    }
    const auto saved = findSummaryDraft(issueId);
    if (!saved) throw std::runtime_error("Cannot reload saved summary");
    return *saved;
}

WorkspaceVerification IssueStore::verifyWorkspace() const {
    return verifyWorkspaceFiles(impl_->root_);
}

std::filesystem::path IssueStore::createBackup(
    const std::filesystem::path& destinationRoot) const {
    if (destinationRoot.empty()) throw std::invalid_argument("备份位置不能为空");
    if (pathIsWithin(destinationRoot, impl_->root_ / "attachments")) {
        throw std::invalid_argument("备份位置不能位于工作区附件目录中");
    }
    std::filesystem::create_directories(destinationRoot);
    const auto suffix = createUuid();
    const auto target = destinationRoot /
        ("IssueTraceBackup-" + backupTimestamp() + "-" + suffix.substr(0, 8));
    const auto temporary = destinationRoot / (".issuetrace-backup-" + suffix);
    if (std::filesystem::exists(target) || std::filesystem::exists(temporary)) {
        throw std::runtime_error("备份目录已存在");
    }
    try {
        std::filesystem::create_directories(temporary);
        backupDatabase(impl_->db_, temporary / "issuetrace.db");
        copyTree(impl_->root_ / "attachments", temporary / "attachments");
        std::filesystem::copy_file(
            impl_->root_ / "workspace.json", temporary / "workspace.json");
        const auto verification = verifyWorkspaceFiles(temporary);
        if (!verification.ok) {
            throw std::runtime_error("备份验证失败：" + verification.errors.front());
        }
        std::ofstream manifest(temporary / "backup.json", std::ios::trunc);
        manifest << "{\n"
                 << "  \"formatVersion\": 1,\n"
                 << "  \"application\": \"IssueTrace\",\n"
                 << "  \"createdAt\": " << nowMilliseconds() << ",\n"
                 << "  \"issues\": " << verification.issueCount << ",\n"
                 << "  \"timelineEntries\": " << verification.timelineCount << ",\n"
                 << "  \"attachments\": " << verification.attachmentCount << "\n"
                 << "}\n";
        if (!manifest) throw std::runtime_error("无法写入备份清单");
        manifest.close();
        std::filesystem::rename(temporary, target);
        return target;
    } catch (...) {
        std::error_code ignored;
        std::filesystem::remove_all(temporary, ignored);
        throw;
    }
}

WorkspaceVerification IssueStore::verifyBackup(
    const std::filesystem::path& backupRoot) {
    auto result = verifyWorkspaceFiles(backupRoot);
    const auto manifestPath = backupRoot / "backup.json";
    if (!std::filesystem::is_regular_file(manifestPath)) {
        result.errors.push_back("缺少 backup.json");
        result.ok = false;
    } else {
        std::ifstream manifest(manifestPath);
        const std::string content((std::istreambuf_iterator<char>(manifest)),
                                  std::istreambuf_iterator<char>());
        if (!manifest || content.find("\"application\": \"IssueTrace\"") ==
                             std::string::npos ||
            content.find("\"formatVersion\": 1") == std::string::npos) {
            result.errors.push_back("backup.json 不是受支持的 IssueTrace 备份清单");
            result.ok = false;
        }
    }
    return result;
}

void IssueStore::restoreBackup(const std::filesystem::path& backupRoot,
                               const std::filesystem::path& workspaceRoot) {
    if (backupRoot.empty() || workspaceRoot.empty()) {
        throw std::invalid_argument("备份或工作区位置不能为空");
    }
    const auto verification = verifyBackup(backupRoot);
    if (!verification.ok) {
        throw std::runtime_error("备份不可恢复：" + verification.errors.front());
    }
    std::filesystem::create_directories(workspaceRoot / "backups");
    const auto suffix = createUuid();
    const auto staging = workspaceRoot / (".issuetrace-restore-" + suffix);
    const auto rollback = workspaceRoot / "backups" /
                          (".restore-rollback-" + suffix);
    bool switchStarted = false;
    try {
        std::filesystem::create_directories(staging);
        std::filesystem::copy_file(backupRoot / "issuetrace.db",
                                   staging / "issuetrace.db");
        std::filesystem::copy_file(backupRoot / "workspace.json",
                                   staging / "workspace.json");
        copyTree(backupRoot / "attachments", staging / "attachments");
        const auto staged = verifyWorkspaceFiles(staging);
        if (!staged.ok) {
            throw std::runtime_error("恢复暂存验证失败：" + staged.errors.front());
        }

        std::filesystem::create_directories(rollback);
        switchStarted = true;
        const auto moveExisting = [&](const std::filesystem::path& relative) {
            const auto source = workspaceRoot / relative;
            if (!std::filesystem::exists(source)) return;
            const auto destination = rollback / relative;
            std::filesystem::create_directories(destination.parent_path());
            std::filesystem::rename(source, destination);
        };
        moveExisting("issuetrace.db");
        moveExisting("issuetrace.db-wal");
        moveExisting("issuetrace.db-shm");
        moveExisting("workspace.json");
        moveExisting("attachments");

        std::filesystem::rename(staging / "issuetrace.db", workspaceRoot / "issuetrace.db");
        std::filesystem::rename(staging / "workspace.json", workspaceRoot / "workspace.json");
        std::filesystem::rename(staging / "attachments", workspaceRoot / "attachments");
        std::filesystem::remove_all(staging);
        std::filesystem::remove_all(rollback);
        std::filesystem::create_directories(workspaceRoot / "exports");
    } catch (...) {
        std::error_code ignored;
        if (switchStarted) {
            std::filesystem::remove(workspaceRoot / "issuetrace.db", ignored);
            std::filesystem::remove(workspaceRoot / "issuetrace.db-wal", ignored);
            std::filesystem::remove(workspaceRoot / "issuetrace.db-shm", ignored);
            std::filesystem::remove(workspaceRoot / "workspace.json", ignored);
            std::filesystem::remove_all(workspaceRoot / "attachments", ignored);
            const auto restoreExisting = [&](const std::filesystem::path& relative) {
                const auto source = rollback / relative;
                if (!std::filesystem::exists(source)) return;
                const auto destination = workspaceRoot / relative;
                std::filesystem::create_directories(destination.parent_path());
                std::filesystem::rename(source, destination, ignored);
            };
            restoreExisting("issuetrace.db");
            restoreExisting("issuetrace.db-wal");
            restoreExisting("issuetrace.db-shm");
            restoreExisting("workspace.json");
            restoreExisting("attachments");
        }
        std::filesystem::remove_all(staging, ignored);
        std::filesystem::remove_all(rollback, ignored);
        throw;
    }
}

}  // namespace issuetrace
