#ifndef SQLITE_REPO_H
#define SQLITE_REPO_H

#include "domain/entities/object.h"
#include "domain/ports/repo.h"

#include <SQLiteCpp/SQLiteCpp.h>

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

class SqliteObjectRepo : public IObjectRepo {
private:
    std::mutex _mutex;
    SQLite::Database _db;
    std::unique_ptr<SQLite::Statement> _insert_stmt;

    void _init_schema() {
        // Tối ưu hiệu năng SQLite cho real-time pipeline
        this->_db.exec("PRAGMA journal_mode = WAL;");
        this->_db.exec("PRAGMA synchronous = NORMAL;");
        this->_db.exec("PRAGMA temp_store = MEMORY;");

        // Tạo bảng objects
        this->_db.exec(R"(
            CREATE TABLE IF NOT EXISTS objects (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                camera_id INTEGER NOT NULL,
                frame_num INTEGER NOT NULL,
                track_id INTEGER NOT NULL,
                x1n REAL NOT NULL,
                y1n REAL NOT NULL,
                x2n REAL NOT NULL,
                y2n REAL NOT NULL,
                class_type INTEGER NOT NULL,
                view_type INTEGER,
                view_score REAL,
                action_state_type INTEGER,
                action_state_score REAL
            );
        )");

        // Tạo index cho truy vấn get_objects_by_frame
        this->_db.exec("CREATE INDEX IF NOT EXISTS idx_objects_cam_frame ON objects(camera_id, frame_num);");

        // Chuẩn bị trước Prepared Statement
        this->_insert_stmt = std::make_unique<SQLite::Statement>(this->_db, R"(
            INSERT INTO objects (
                camera_id, frame_num, track_id,
                x1n, y1n, x2n, y2n, class_type,
                view_type, view_score,
                action_state_type, action_state_score
            ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);
        )");
    }

public:
    explicit SqliteObjectRepo(const std::string& db_path)
        : _db([&]() {
            std::filesystem::path p(db_path);
            if (p.has_parent_path()) {
                std::filesystem::create_directories(p.parent_path());
            }
            return SQLite::Database(db_path, SQLite::OPEN_READWRITE | SQLite::OPEN_CREATE);
          }()) 
    {
        this->_init_schema();
    }

    void add(Object obj) override {
        std::lock_guard<std::mutex> lock(this->_mutex);

        this->_insert_stmt->reset();
        this->_insert_stmt->clearBindings();

        // 1. Bind các trường bắt buộc
        this->_insert_stmt->bind(1, static_cast<int32_t>(obj.camera_id));
        this->_insert_stmt->bind(2, static_cast<int32_t>(obj.frame_num));
        this->_insert_stmt->bind(3, static_cast<int64_t>(obj.track_id));
        this->_insert_stmt->bind(4, static_cast<double>(obj.x1n));
        this->_insert_stmt->bind(5, static_cast<double>(obj.y1n));
        this->_insert_stmt->bind(6, static_cast<double>(obj.x2n));
        this->_insert_stmt->bind(7, static_cast<double>(obj.y2n));
        this->_insert_stmt->bind(8, static_cast<int32_t>(obj.class_type));

        // 2. Bind View (NULL nếu nullopt)
        if (obj.view.has_value()) {
            this->_insert_stmt->bind(9, static_cast<int32_t>(obj.view->type));
            this->_insert_stmt->bind(10, static_cast<double>(obj.view->score));
        } else {
            this->_insert_stmt->bind(9);
            this->_insert_stmt->bind(10);
        }

        // 3. Bind ActionState (NULL nếu nullopt)
        if (obj.action_state.has_value()) {
            this->_insert_stmt->bind(11, static_cast<int32_t>(obj.action_state->type));
            this->_insert_stmt->bind(12, static_cast<double>(obj.action_state->score));
        } else {
            this->_insert_stmt->bind(11);
            this->_insert_stmt->bind(12);
        }

        this->_insert_stmt->exec();
    }

    std::vector<Object> get_objects_by_frame(
        guint camera_id, 
        gint frame_num, 
        std::optional<std::vector<DetectionClassType>> class_filter = std::nullopt
    ) override {
        std::lock_guard<std::mutex> lock(this->_mutex);

        std::vector<Object> results;

        SQLite::Statement query(this->_db, R"(
            SELECT 
                camera_id, frame_num, track_id,
                x1n, y1n, x2n, y2n, class_type,
                view_type, view_score,
                action_state_type, action_state_score
            FROM objects
            WHERE camera_id = ? AND frame_num = ?;
        )");

        query.bind(1, static_cast<int32_t>(camera_id));
        query.bind(2, static_cast<int32_t>(frame_num));

        while (query.executeStep()) {
            DetectionClassType c_type = static_cast<DetectionClassType>(query.getColumn(7).getInt());

            if (class_filter.has_value()) {
                const auto& filters = class_filter.value();
                if (std::find(filters.begin(), filters.end(), c_type) == filters.end()) {
                    continue;
                }
            }

            Object obj;
            obj.camera_id = static_cast<guint>(query.getColumn(0).getInt());
            obj.frame_num = static_cast<gint>(query.getColumn(1).getInt());
            obj.track_id = static_cast<guint64>(query.getColumn(2).getInt64());
            obj.x1n = static_cast<float>(query.getColumn(3).getDouble());
            obj.y1n = static_cast<float>(query.getColumn(4).getDouble());
            obj.x2n = static_cast<float>(query.getColumn(5).getDouble());
            obj.y2n = static_cast<float>(query.getColumn(6).getDouble());
            obj.class_type = c_type;

            if (!query.getColumn(8).isNull()) {
                View view;
                view.type = static_cast<ViewType>(query.getColumn(8).getInt());
                view.score = static_cast<float>(query.getColumn(9).getDouble());
                obj.view = view;
            }

            if (!query.getColumn(10).isNull()) {
                ActionState action;
                action.type = static_cast<ActionStateType>(query.getColumn(10).getInt());
                action.score = static_cast<float>(query.getColumn(11).getDouble());
                obj.action_state = action;
            }

            results.push_back(obj);
        }

        return results;
    }
};

#endif // SQLITE_REPO_H
