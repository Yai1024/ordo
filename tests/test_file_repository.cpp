// M1：CSV 仓库的往返与损坏文件处理（OUTLINE §11：C4 必须有往返测试）。

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "ordo/io/file_repository.h"
#include "test_framework.h"

namespace {

const ordo::TimePoint kNow{std::chrono::hours{24 * 365}};

std::filesystem::path temp_file(const std::string& name) {
    return std::filesystem::temp_directory_path() / name;
}

void write_text(const std::filesystem::path& path, const std::string& text) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output << text;
}

ordo::Task make_task(ordo::TaskId id, const std::string& title) {
    ordo::Task task;
    task.id = id;
    task.title = title;
    task.course = "高等数学";
    task.importance = 4;
    task.estimate = 90;
    task.deadline = ordo::add_minutes(kNow, 500);
    return task;
}

const std::string kHeader = "id,title,course,importance,estimate,deadline,state,review_stage\n";
const std::string kHeaderV2 =
    "id,title,course,importance,estimate,deadline,state,review_stage,kind,fixed_start\n";

}  // namespace

ORDO_TEST(repository_round_trips_all_fields) {
    const std::filesystem::path path = temp_file("ordo_repo_roundtrip.csv");
    std::filesystem::remove(path);

    ordo::CsvTaskRepository repo(path);
    ordo::Task first = make_task(1, "读书,第3章");
    first.course = "他说\"好\"";
    first.state = ordo::TaskState::Doing;
    first.review_stage = 2;

    ORDO_CHECK(repo.save_all({first, make_task(2, "普通任务")}).ok());

    const auto loaded = repo.load_all();
    ORDO_CHECK(loaded.has_value());
    ORDO_CHECK_EQ(loaded.value().size(), std::size_t{2});

    const ordo::Task& reloaded = loaded.value()[0];
    ORDO_CHECK_EQ(reloaded.id, ordo::TaskId{1});
    ORDO_CHECK_EQ(reloaded.title, std::string{"读书,第3章"});
    ORDO_CHECK_EQ(reloaded.course, std::string{"他说\"好\""});
    ORDO_CHECK_EQ(reloaded.importance, 4);
    ORDO_CHECK_EQ(reloaded.estimate, ordo::Minutes{90});
    ORDO_CHECK(reloaded.state == ordo::TaskState::Doing);
    ORDO_CHECK_EQ(reloaded.review_stage, 2);
    ORDO_CHECK_EQ(ordo::format_datetime(reloaded.deadline),
                  ordo::format_datetime(first.deadline));

    // 写入是"临时文件 + 原子替换"，完成后不应留下 .tmp。
    std::filesystem::path temp_path = path;
    temp_path += ".tmp";
    ORDO_CHECK(!std::filesystem::exists(temp_path));

    std::filesystem::remove(path);
}

ORDO_TEST(repository_treats_missing_file_as_empty) {
    const std::filesystem::path path = temp_file("ordo_repo_missing.csv");
    std::filesystem::remove(path);

    const ordo::CsvTaskRepository repo(path);
    const auto loaded = repo.load_all();
    ORDO_CHECK(loaded.has_value());
    ORDO_CHECK(loaded.value().empty());
}

ORDO_TEST(repository_accepts_utf8_bom) {
    const std::filesystem::path path = temp_file("ordo_repo_bom.csv");
    write_text(path, "\xEF\xBB\xBF" + kHeader + "1,带 BOM,,3,10,2026-10-30 23:59,todo,0\n");

    const ordo::CsvTaskRepository repo(path);
    const auto loaded = repo.load_all();
    ORDO_CHECK(loaded.has_value());
    ORDO_CHECK_EQ(loaded.value().size(), std::size_t{1});
    ORDO_CHECK_EQ(loaded.value()[0].title, std::string{"带 BOM"});

    std::filesystem::remove(path);
}

ORDO_TEST(repository_rejects_wrong_header) {
    const std::filesystem::path path = temp_file("ordo_repo_badheader.csv");
    write_text(path, "id,title\n1,x\n");

    const ordo::CsvTaskRepository repo(path);
    const auto loaded = repo.load_all();
    ORDO_CHECK(!loaded.has_value());
    ORDO_CHECK(loaded.status().code == ordo::ErrorCode::Corrupted);

    std::filesystem::remove(path);
}

ORDO_TEST(repository_rejects_wrong_field_count) {
    const std::filesystem::path path = temp_file("ordo_repo_badcolumns.csv");
    write_text(path, kHeader + "1,只有七列,数学,3,10,2026-10-30 23:59,todo\n");

    const ordo::CsvTaskRepository repo(path);
    const auto loaded = repo.load_all();
    ORDO_CHECK(!loaded.has_value());
    ORDO_CHECK(loaded.status().code == ordo::ErrorCode::Corrupted);

    std::filesystem::remove(path);
}

ORDO_TEST(repository_rejects_duplicate_id) {
    const std::filesystem::path path = temp_file("ordo_repo_duplicate.csv");
    write_text(path, kHeader +
                         "1,第一条,,3,10,2026-10-30 23:59,todo,0\n"
                         "1,第二条,,3,10,2026-10-30 23:59,todo,0\n");

    const ordo::CsvTaskRepository repo(path);
    const auto loaded = repo.load_all();
    ORDO_CHECK(!loaded.has_value());
    ORDO_CHECK(loaded.status().code == ordo::ErrorCode::Duplicate);

    std::filesystem::remove(path);
}

ORDO_TEST(repository_reads_old_header_as_other_kind) {
    const std::filesystem::path path = temp_file("ordo_repo_old_header.csv");
    write_text(path, kHeader + "1,旧数据,,3,10,2026-10-30 23:59,todo,0\n");

    const ordo::CsvTaskRepository repo(path);
    const auto loaded = repo.load_all();
    ORDO_CHECK(loaded.has_value());
    ORDO_CHECK_EQ(loaded.value().size(), std::size_t{1});
    ORDO_CHECK(loaded.value()[0].kind == ordo::TaskKind::Other);
    ORDO_CHECK(!loaded.value()[0].fixed_start.has_value());

    std::filesystem::remove(path);
}

ORDO_TEST(repository_round_trips_activity) {
    const std::filesystem::path path = temp_file("ordo_repo_activity.csv");
    std::filesystem::remove(path);

    ordo::Task activity = make_task(1, "社团例会");
    activity.kind = ordo::TaskKind::Activity;
    activity.estimate = 60;
    activity.fixed_start = ordo::add_minutes(kNow, 1940);
    activity.deadline = ordo::add_minutes(kNow, 2000);

    ordo::CsvTaskRepository repo(path);
    ORDO_CHECK(repo.save_all({activity}).ok());

    const auto loaded = repo.load_all();
    ORDO_CHECK(loaded.has_value());
    ORDO_CHECK_EQ(loaded.value().size(), std::size_t{1});
    ORDO_CHECK(loaded.value()[0].kind == ordo::TaskKind::Activity);
    ORDO_CHECK(loaded.value()[0].fixed_start.has_value());
    ORDO_CHECK_EQ(ordo::format_datetime(*loaded.value()[0].fixed_start),
                  ordo::format_datetime(*activity.fixed_start));

    std::filesystem::remove(path);
}

ORDO_TEST(repository_rejects_activity_without_fixed_start) {
    const std::filesystem::path path = temp_file("ordo_repo_activity_bad.csv");
    write_text(path, kHeaderV2 + "1,活动,,3,60,2026-10-30 20:00,todo,0,activity,\n");

    const ordo::CsvTaskRepository repo(path);
    const auto loaded = repo.load_all();
    ORDO_CHECK(!loaded.has_value());
    ORDO_CHECK(loaded.status().code == ordo::ErrorCode::InvalidValue);

    std::filesystem::remove(path);
}

ORDO_TEST(repository_rejects_unknown_kind) {
    const std::filesystem::path path = temp_file("ordo_repo_bad_kind.csv");
    write_text(path, kHeaderV2 + "1,任务,,3,60,2026-10-30 20:00,todo,0,lunch,\n");

    const ordo::CsvTaskRepository repo(path);
    const auto loaded = repo.load_all();
    ORDO_CHECK(!loaded.has_value());
    ORDO_CHECK(loaded.status().code == ordo::ErrorCode::InvalidFormat);

    std::filesystem::remove(path);
}

ORDO_TEST(repository_find_upsert_erase) {
    const std::filesystem::path path = temp_file("ordo_repo_crud.csv");
    std::filesystem::remove(path);

    ordo::CsvTaskRepository repo(path);
    ORDO_CHECK(repo.upsert(make_task(1, "初始标题")).ok());

    auto found = repo.find(1);
    ORDO_CHECK(found.has_value());
    ORDO_CHECK_EQ(found.value().title, std::string{"初始标题"});

    ordo::Task changed = found.value();
    changed.title = "更新后的标题";
    ORDO_CHECK(repo.upsert(changed).ok());
    ORDO_CHECK_EQ(repo.find(1).value().title, std::string{"更新后的标题"});

    ORDO_CHECK(repo.erase(1).ok());
    ORDO_CHECK(!repo.find(1).has_value());
    ORDO_CHECK(repo.erase(1).code == ordo::ErrorCode::NotFound);

    std::filesystem::remove(path);
}
